#!/usr/bin/env python3
"""生成机器学习仿真数据集 machine_learning/data/ml_sim.db。

- 直接复用 server/sql/schema.sql 建库；
- 只用 python3 标准库（sqlite3/json/random/math），固定种子，可复现；
- 数据时间跨度：最近 14 个完整自然日 + 今天已过去的整点小时。
"""

import json
import math
import random
import sqlite3
import sys
from datetime import datetime, timedelta
from pathlib import Path

SEED = 20260909

SCRIPT_DIR = Path(__file__).resolve().parent
ML_DIR = SCRIPT_DIR.parent                  # machine_learning/
ROOT_DIR = ML_DIR.parent                    # 仓库根
SCHEMA_PATH = ROOT_DIR / "server" / "sql" / "schema.sql"
DB_PATH = ML_DIR / "data" / "ml_sim.db"

FULL_DAYS = 14          # 完整自然日数量（第 1 天 ~ 第 14 天）
NOW = datetime.now().replace(minute=0, second=0, microsecond=0)
TODAY = NOW.date()
FIRST_DAY = TODAY - timedelta(days=FULL_DAYS)   # 第 1 天
LAST_DAY = TODAY - timedelta(days=1)            # 第 14 天（回测目标日）

TAG_POOL = [
    "fast_charge", "spacious", "quiet_night", "high_parking_fee",
    "old_device", "good_fast_charge", "heavy_queue", "good_service",
]

STATIONS = [
    {
        "name": "科技园充电站", "area": "高新区", "address": "高新区科苑路 1 号",
        "lon": 116.301, "lat": 39.985, "fee": 0.55, "parking_fee": 0.0,
        "facilities": ["washroom", "wifi", "rain_shelter"],
        "owner": "self_run", "merchant": None,
        "chargers": [("fast", 60)] * 7 + [("slow", 7)] * 2,
        "base": 25.0, "m_amp": 30.0, "e_amp": 40.0, "orders_per_day": 12,
    },
    {
        "name": "商业中心快充站", "area": "商业区", "address": "商业区金街 88 号",
        "lon": 116.318, "lat": 39.992, "fee": 0.60, "parking_fee": 5.0,
        "facilities": ["convenience_store", "rest_area", "rain_shelter"],
        "owner": "franchise", "merchant": 1,
        "chargers": [("fast", 120)] * 8 + [("slow", 7)] * 2,
        "base": 35.0, "m_amp": 25.0, "e_amp": 55.0, "orders_per_day": 16,
    },
    {
        "name": "滨河小区充电站", "area": "住宅区", "address": "住宅区滨河东路 6 号",
        "lon": 116.289, "lat": 39.978, "fee": 0.45, "parking_fee": 0.0,
        "facilities": ["washroom", "underground_parking"],
        "owner": "self_run", "merchant": None,
        "chargers": [("fast", 60)] * 6 + [("slow", 7)] * 2,
        "base": 18.0, "m_amp": 12.0, "e_amp": 35.0, "orders_per_day": 9,
    },
    {
        "name": "物流园充电站", "area": "工业区", "address": "工业区货运大道 100 号",
        "lon": 116.332, "lat": 39.971, "fee": 0.40, "parking_fee": 0.0,
        "facilities": ["washroom", "wifi"],
        "owner": "partner", "merchant": None,
        "chargers": [("fast", 120)] * 3 + [("fast", 60)] * 3 + [("slow", 7)] * 2,
        "base": 30.0, "m_amp": 42.0, "e_amp": 22.0, "orders_per_day": 11,
    },
]

USER_PHONES = [f"1380000000{i}" for i in range(1, 9)]


def ts(dt):
    return dt.strftime("%Y-%m-%d %H:%M:%S")


def load_curve(st, hour, weekend):
    """基线 + 早晚双峰正弦（高斯峰近似）+ 周末衰减。"""
    morning = st["m_amp"] * math.exp(-((hour - 8) / 2.5) ** 2)
    evening = st["e_amp"] * math.exp(-((hour - 18) / 3.0) ** 2)
    factor = 0.8 if weekend else 1.0
    return (st["base"] + morning + evening) * factor


def build_schema(conn):
    schema = SCHEMA_PATH.read_text(encoding="utf-8")
    conn.executescript(schema)
    conn.executescript(
        """
        CREATE INDEX IF NOT EXISTS idx_measure_station_time
            ON charging_measure(station_id, measure_time);
        CREATE INDEX IF NOT EXISTS idx_measure_charger_time
            ON charging_measure(charger_id, measure_time);
        CREATE INDEX IF NOT EXISTS idx_order_station_start
            ON charging_order(station_id, start_time);
        CREATE INDEX IF NOT EXISTS idx_reservation_user_time
            ON reservation(user_id, reserve_time);
        CREATE INDEX IF NOT EXISTS idx_review_station_time
            ON review(station_id, create_time);
        """
    )


def gen_users(cur, rng):
    user_ids = []
    for i, phone in enumerate(USER_PHONES):
        if i < 2:
            reg_day = TODAY                      # 今日新增（用户增长意图）
        elif i < 5:
            reg_day = TODAY - timedelta(days=rng.randint(1, 6))
        else:
            reg_day = TODAY - timedelta(days=rng.randint(7, 55))
        cur.execute(
            "INSERT INTO user(phone, nickname, balance, points, level, status,"
            " register_time) VALUES(?,?,?,?,?,?,?)",
            (phone, f"用户{phone[-4:]}", round(rng.uniform(50, 500), 2),
             rng.randint(0, 2000), "normal", "normal",
             ts(datetime.combine(reg_day, datetime.min.time()) + timedelta(hours=rng.randint(8, 22)))),
        )
        user_ids.append(cur.lastrowid)
    return user_ids


def gen_stations_chargers(cur, rng):
    """返回 {station_index: [charger_id,...]}，每站最后 2 台为故障桩。"""
    cur.execute("INSERT INTO merchant(name, contact_name, contact_phone,"
                " cooperation_type, status) VALUES(?,?,?,?,?)",
                ("绿能商业管理有限公司", "王合作", "13911112222", "franchise", "active"))
    station_ids, charger_map = [], {}
    for si, st in enumerate(STATIONS):
        cur.execute(
            "INSERT INTO station(name, address, area, longitude, latitude,"
            " total_chargers, online_rate, service_fee, parking_fee,"
            " business_hours, facilities, owner_type, merchant_id, has_swap)"
            " VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
            (st["name"], st["address"], st["area"], st["lon"], st["lat"],
             len(st["chargers"]), 100.0, st["fee"], st["parking_fee"],
             "00:00-24:00", json.dumps(st["facilities"], ensure_ascii=False),
             st["owner"], st["merchant"], 0),
        )
        sid = cur.lastrowid
        station_ids.append(sid)
        ids = []
        for ci, (ctype, power) in enumerate(st["chargers"]):
            is_fault = ci >= len(st["chargers"]) - 2
            if is_fault:
                status = "fault"
                temp = round(rng.uniform(66, 88), 1)
                comm = "abnormal" if ci % 2 == 0 else "normal"
                fault_code = rng.choice(["E12", "E07", "E31"])
                health = rng.randint(30, 50)
            else:
                status = "idle"
                temp = round(rng.uniform(28, 46), 1)
                comm = "normal"
                fault_code = ""
                health = 100
            cur.execute(
                "INSERT INTO charger(code, station_id, type, power, status,"
                " voltage, current, temperature, fault_code, comm_status,"
                " health_score, total_charge_count, total_charge_duration)"
                " VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)",
                (f"{chr(65 + si)}-{ci + 1:03d}", sid, ctype, power, status,
                 0 if is_fault else 380, 0, temp, fault_code, comm,
                 health, rng.randint(100, 3000) if not is_fault else 0,
                 rng.randint(1000, 9000) if not is_fault else 0),
            )
            ids.append(cur.lastrowid)
        charger_map[si] = ids
    return station_ids, charger_map


def gen_measures(cur, rng, station_ids, charger_map):
    """14 天整点 + 今天已过去小时；负荷 = 曲线值 + 桩间分配 + 随机噪声。"""
    total_rows = 0
    for si, sid in enumerate(station_ids):
        st = STATIONS[si]
        healthy = charger_map[si][:-2]
        fault = charger_map[si][-2:]
        rated = {cid: p for cid, (_, p) in zip(charger_map[si], st["chargers"])}
        hours_all = []
        for d in range(FULL_DAYS):
            day = FIRST_DAY + timedelta(days=d)
            hours_all += [datetime.combine(day, datetime.min.time()) + timedelta(hours=h) for h in range(24)]
        hours_all += [datetime.combine(TODAY, datetime.min.time()) + timedelta(hours=h)
                      for h in range(NOW.hour)]
        for t in hours_all:
            weekend = t.weekday() >= 5
            total = load_curve(st, t.hour, weekend) * (1 + rng.gauss(0, 0.03))
            rated_sum = sum(rated[c] for c in healthy)
            p_act = min(0.95, max(0.2, total / max(rated_sum, 1.0)))
            actives = [c for c in healthy if rng.random() < p_act]
            if not actives:
                actives = [rng.choice(healthy)]
            act_rated = sum(rated[c] for c in actives)
            for cid in charger_map[si]:
                if cid in fault:
                    power = 0.0
                    temp = None
                    soc = 0.0
                elif cid in actives:
                    share = rated[cid] / act_rated
                    power = min(rated[cid], total * share * (1 + rng.gauss(0, 0.03)))
                    temp = round(rng.uniform(30, 48), 1)
                    soc = round(rng.uniform(20, 95), 1)
                else:
                    power = 0.0
                    temp = round(rng.uniform(25, 32), 1)
                    soc = round(rng.uniform(95, 100), 1)
                cur.execute(
                    "INSERT INTO charging_measure(charger_id, station_id,"
                    " measure_time, power_kw, soc, energy_delta_kwh, temperature)"
                    " VALUES(?,?,?,?,?,?,?)",
                    (cid, sid, ts(t), round(power, 2), soc,
                     round(power * 1.0, 2), temp),
                )
                total_rows += 1
    return total_rows


def gen_orders(cur, rng, user_ids, station_ids, charger_map):
    order_ids = []
    risk_user_short = user_ids[6]    # 用户 7：短时刷单
    for si, sid in enumerate(station_ids):
        st = STATIONS[si]
        healthy = charger_map[si][:-2]
        for d in range(FULL_DAYS):
            day = FIRST_DAY + timedelta(days=d)
            weekend = day.weekday() >= 5
            n = max(1, round(st["orders_per_day"] * (0.75 if weekend else 1.0)))
            for _ in range(n):
                uid = rng.choice(user_ids[:6])
                hour = rng.choices(range(24), weights=[
                    load_curve(st, h, weekend) for h in range(24)])[0]
                start = datetime.combine(day, datetime.min.time()) + timedelta(
                    hours=hour, minutes=rng.randint(0, 59))
                cid = rng.choice(healthy)
                cpower = next(p for c, (_, p) in zip(charger_map[si], st["chargers"]) if c == cid)
                if cpower <= 7:
                    dur = rng.randint(120, 300)
                else:
                    dur = rng.randint(20, 90)
                energy = round(cpower * dur / 60 * rng.uniform(0.4, 0.7), 2)
                price = rng.uniform(0.9, 1.5)
                amount = round(energy * price, 2)
                cur.execute(
                    "INSERT INTO charging_order(user_id, station_id, charger_id,"
                    " status, start_soc, target_soc, end_soc, start_time, end_time,"
                    " duration_min, energy_kwh, price_level, amount,"
                    " discount_amount, pay_amount, create_time, settle_time)"
                    " VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                    (uid, sid, cid, "completed", round(rng.uniform(15, 40), 1),
                     95.0, 95.0, ts(start), ts(start + timedelta(minutes=dur)),
                     dur, energy, rng.choice(["valley", "flat", "flat", "peak"]),
                     amount, 0.0, amount, ts(start - timedelta(minutes=3)),
                     ts(start + timedelta(minutes=dur))),
                )
                order_ids.append(cur.lastrowid)
        # 今天已过去的订单
        for h in range(NOW.hour):
            if rng.random() < 0.5:
                continue
            uid = rng.choice(user_ids[:6])
            start = datetime.combine(TODAY, datetime.min.time()) + timedelta(hours=h, minutes=rng.randint(0, 59))
            cid = rng.choice(healthy)
            cpower = next(p for c, (_, p) in zip(charger_map[si], st["chargers"]) if c == cid)
            dur = rng.randint(120, 300) if cpower <= 7 else rng.randint(20, 90)
            energy = round(cpower * dur / 60 * rng.uniform(0.4, 0.7), 2)
            amount = round(energy * rng.uniform(0.9, 1.5), 2)
            cur.execute(
                "INSERT INTO charging_order(user_id, station_id, charger_id,"
                " status, start_time, end_time, duration_min, energy_kwh,"
                " price_level, amount, discount_amount, pay_amount,"
                " create_time, settle_time) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                (uid, sid, cid, "completed", ts(start),
                 ts(start + timedelta(minutes=dur)), dur, energy,
                 rng.choice(["valley", "flat", "peak"]), amount, 0.0, amount,
                 ts(start - timedelta(minutes=3)), ts(start + timedelta(minutes=dur))),
            )
            order_ids.append(cur.lastrowid)
    # 用户 7：今天 3 笔 <5 分钟订单（风控规则 2）
    sid0 = station_ids[0]
    for i in range(3):
        start = datetime.combine(TODAY, datetime.min.time()) + timedelta(hours=max(0, NOW.hour - 1), minutes=10 + i)
        cur.execute(
            "INSERT INTO charging_order(user_id, station_id, charger_id, status,"
            " start_time, end_time, duration_min, energy_kwh, price_level,"
            " amount, discount_amount, pay_amount, create_time, settle_time)"
            " VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
            (risk_user_short, sid0, charger_map[0][0], "completed", ts(start),
             ts(start + timedelta(minutes=rng.randint(2, 4))),
             rng.randint(2, 4), round(rng.uniform(0.1, 0.5), 2), "flat",
             0.5, 0.0, 0.5, ts(start), ts(start + timedelta(minutes=4))),
        )
        order_ids.append(cur.lastrowid)
    return order_ids


def gen_reservations(cur, rng, user_ids, station_ids, charger_map):
    """常规排队 + 用户 8 的高频取消（风控规则 1）。"""
    queue_plan = {0: 2, 1: 6, 2: 1, 3: 0}
    for si, sid in enumerate(station_ids):
        for q in range(queue_plan.get(si, 0)):
            cur.execute(
                "INSERT INTO reservation(user_id, station_id, queue_no,"
                " reserve_time, status) VALUES(?,?,?,?,?)",
                (rng.choice(user_ids[:6]), sid, q + 1,
                 ts(NOW - timedelta(minutes=rng.randint(5, 120))), "waiting"),
            )
        # 历史 matched / cancelled
        for _ in range(6):
            t = NOW - timedelta(days=rng.randint(1, 13), hours=rng.randint(0, 20))
            cur.execute(
                "INSERT INTO reservation(user_id, station_id, charger_id,"
                " queue_no, reserve_time, notified, status) VALUES(?,?,?,?,?,?,?)",
                (rng.choice(user_ids[:6]), sid,
                 rng.choice(charger_map[si][:-2]), 1, ts(t), 1,
                 rng.choice(["matched", "matched", "cancelled"])),
            )
    risk_user = user_ids[7]
    for i in range(10):
        t = NOW - timedelta(hours=rng.randint(0, 22), minutes=rng.randint(0, 59))
        cancelled = i < 7                        # 取消率 0.7
        cur.execute(
            "INSERT INTO reservation(user_id, station_id, queue_no, reserve_time,"
            " notified, status) VALUES(?,?,?,?,?,?)",
            (risk_user, station_ids[i % 4], i + 1, ts(t), 0,
             "cancelled" if cancelled else "waiting"),
        )


def gen_weather_price_alarm(cur, rng, station_ids):
    cur.execute("INSERT INTO weather(area, condition, temperature, update_time)"
                " VALUES(?,?,?,?)", ("", "sunny", 28.0, ts(NOW)))
    for st in STATIONS:
        cur.execute("INSERT INTO weather(area, condition, temperature,"
                    " forecast, update_time) VALUES(?,?,?,?,?)",
                    (st["area"], "sunny", round(rng.uniform(26, 32), 1),
                     json.dumps([{"condition": "sunny"}] * 24, ensure_ascii=False),
                     ts(NOW)))
    for sid in station_ids:
        for level, price, rng_ in [("valley", 0.70, "00:00-08:00"),
                                   ("flat", 1.00, "08:00-18:00"),
                                   ("peak", 1.35, "18:00-22:00"),
                                   ("flat", 1.00, "22:00-24:00")]:
            cur.execute("INSERT INTO price_rule(station_id, level, price, time_range)"
                        " VALUES(?,?,?,?)", (sid, level, price, rng_))
    for m, d, name in [(1, 1, "元旦"), (5, 1, "劳动节"), (10, 1, "国庆节")]:
        cur.execute("INSERT INTO holiday(date, name, is_workday) VALUES(?,?,0)"
                    " ON CONFLICT(date) DO NOTHING",
                    (f"2026-{m:02d}-{d:02d}", name))


def gen_alarms(cur, rng, station_ids, charger_map):
    for si, sid in enumerate(station_ids):
        for cid in charger_map[si][-2:]:
            for _ in range(rng.randint(2, 3)):
                t = NOW - timedelta(days=rng.randint(0, 25), hours=rng.randint(0, 20))
                cur.execute(
                    "INSERT INTO alarm(charger_id, station_id, type, level,"
                    " occur_time, status, handle_action) VALUES(?,?,?,?,?,?,?)",
                    (cid, sid, rng.choice(["overheat", "power_drop", "offline"]),
                     rng.choice(["warning", "critical"]), ts(t),
                     rng.choice(["open", "handled", "handled"]),
                     rng.choice(["restart", "repair", "ignore"])))


def gen_reviews(cur, rng, user_ids, station_ids, order_ids):
    for si, sid in stations_enumerate(station_ids):
        n = rng.randint(30, 40)
        for _ in range(n):
            days_ago = rng.randint(0, 59)
            t = NOW - timedelta(days=days_ago, hours=rng.randint(0, 20))
            n_tags = rng.randint(1, 3)
            tags = rng.sample(TAG_POOL, n_tags)
            device_score = round(max(1.0, rng.gauss(3.5 if si % 2 else 4.2, 0.6)), 1)
            scores = [round(min(5.0, max(1.0, rng.gauss(4.2, 0.5))), 1) for _ in range(4)]
            cur.execute(
                "INSERT INTO review(user_id, station_id, order_id, overall_score,"
                " speed_score, device_score, parking_score, hygiene_score,"
                " service_score, tags, content, useful_count, status, create_time)"
                " VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                (rng.choice(user_ids), sid,
                 rng.choice(order_ids) if order_ids else None,
                 round(min(5.0, max(1.0, (device_score + sum(scores)) / 5)), 1),
                 scores[0], device_score, scores[1], scores[2], scores[3],
                 json.dumps(tags, ensure_ascii=False),
                 rng.choice(["充电很快，体验不错", "排队有点久", "位置好找",
                             "设备有点旧了", "价格实惠", ""]),
                 rng.randint(0, 20), "normal", ts(t)))


def stations_enumerate(station_ids):
    return list(enumerate(station_ids))


def gen_charger_snapshot(cur, rng, station_ids, charger_map):
    """按当前小时利用率设定桩状态快照（站 2 全忙 + 排队，用于调度/等待演示）。"""
    for si, sid in enumerate(station_ids):
        st = STATIONS[si]
        healthy = charger_map[si][:-2]
        if si == 1:
            busy = healthy
        else:
            p_act = min(0.6, max(0.15, load_curve(st, NOW.hour, False) / 300.0))
            busy = [c for c in healthy if rng.random() < p_act]
        for cid in healthy:
            if cid in busy:
                cur.execute("UPDATE charger SET status='charging' WHERE id=?", (cid,))


def main():
    if not SCHEMA_PATH.exists():
        print(f"schema 不存在: {SCHEMA_PATH}", file=sys.stderr)
        return 1
    rng = random.Random(SEED)
    DB_PATH.parent.mkdir(parents=True, exist_ok=True)
    if DB_PATH.exists():
        DB_PATH.unlink()
    conn = sqlite3.connect(DB_PATH)
    try:
        with conn:
            build_schema(conn)
            cur = conn.cursor()
            cur.execute("INSERT INTO admin(account, password) VALUES('admin','123456')")
            user_ids = gen_users(cur, rng)
            station_ids, charger_map = gen_stations_chargers(cur, rng)
            n_measure = gen_measures(cur, rng, station_ids, charger_map)
            order_ids = gen_orders(cur, rng, user_ids, station_ids, charger_map)
            gen_reservations(cur, rng, user_ids, station_ids, charger_map)
            gen_weather_price_alarm(cur, rng, station_ids)
            gen_alarms(cur, rng, station_ids, charger_map)
            gen_reviews(cur, rng, user_ids, station_ids, order_ids)
            gen_charger_snapshot(cur, rng, station_ids, charger_map)
        conn.commit()
    finally:
        conn.close()
    size_kb = DB_PATH.stat().st_size // 1024
    print(f"已生成 {DB_PATH} ({size_kb} KB)")
    print(f"数据范围: {FIRST_DAY} 00:00 ~ 第14天 {LAST_DAY} 23:00 + 今日 {NOW.hour} 小时")
    print(f"charging_measure 行数: {n_measure}，订单数: {len(order_ids)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
