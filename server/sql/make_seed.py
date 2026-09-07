#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
make_seed.py — 建库 + 种子数据生成器
位置: server/sql/make_seed.py   (与 schema.sql 同目录)
用法: python3 make_seed.py      → 生成 charge.db (已存在则先删除重建)
依赖: 仅 Python 标准库, 无需安装任何东西
依据: docs/content/spec-数据库.md「种子数据约定」
要点: 历史订单/时序测量按「基线 + 早晚双峰 + 随机噪声」生成, 供负荷预测演示;
      余额/积分/流水/桩累计数等冗余字段全部按真实业务口径聚合回填, 保证跨表一致。
"""
import json
import math
import os
import random
import sqlite3
from datetime import datetime, timedelta

random.seed(42)                      # 固定随机种子, 每次生成结果一致, 方便联调对数
HERE = os.path.dirname(os.path.abspath(__file__))
DB   = os.path.join(HERE, "charge.db")
# 种子基准时刻 = 今天中午。写死日期会导致跑到第二天时"今日营收"变成 0,
# 演示当天数据必须"活"到今天。random.seed 固定, 同一天生成的库内容仍完全一致。
NOW  = datetime.now().replace(second=0, microsecond=0)   # 基准 = 此刻

def ts(dt):  return dt.strftime("%Y-%m-%d %H:%M:%S")

def price_level_of(hour):
    """分时电价档位: 谷 00-08, 峰 17-21, 其余平"""
    if hour < 8:            return "valley", 0.40
    if 17 <= hour < 21:     return "peak",   1.00
    return "flat", 0.70

# ---------- 静态种子加载 (来自 seed_data.json) ----------
def load_seed():
    """读取同目录 seed_data.json, 返回解析后的 dict。

    JSON 约定(详见文件顶部 _说明/_约定):
      - 顶层及数据行内以 `_` 开头的键均为注释说明, 由 seed_rows() 剔除;
      - 列表顺序即插入顺序 => 自增 id 从 1 递增, 不得调序;
      - 外键用引用字段表达, 由调用方解析成真实 id。
    """
    with open(os.path.join(HERE, "seed_data.json"), encoding="utf-8") as f:
        return json.load(f)

def seed_rows(rows):
    """剔除单行数据中的 `_` 注释字段, 返回纯净字段 dict 列表。"""
    return [{k: v for k, v in r.items() if not k.startswith("_")} for r in rows]

def main():
    if os.path.exists(DB):
        os.remove(DB)
    con = sqlite3.connect(DB)
    con.execute("PRAGMA foreign_keys=ON")
    con.executescript(open(os.path.join(HERE, "schema.sql"), encoding="utf-8").read())
    c = con.cursor()

    # ---------- 静态基础数据 (读取 seed_data.json) ----------
    seed = load_seed()

    # 商户: 记录 name -> id, 供后文 station.merchant_ref 解析
    merchant_id_by_name = {}
    for s in seed_rows(seed["merchant"]):
        c.execute("""INSERT INTO merchant(name,contact_name,contact_phone,cooperation_type,status,remark,create_time)
                     VALUES(?,?,?,?,?,?,?)""",
                  (s["name"], s["contact_name"], s["contact_phone"], s["cooperation_type"],
                   s["status"], s["remark"], ts(NOW - timedelta(days=s["create_days_ago"]))))
        merchant_id_by_name[s["name"]] = c.lastrowid

    # 充电站: facilities 以数组写入 JSON, 落库时转成 JSON 字符串;
    #         merchant_ref(商户名或 null) 解析为 merchant_id
    station_id_by_name = {}
    for s in seed_rows(seed["station"]):
        mid = merchant_id_by_name[s["merchant_ref"]] if s["merchant_ref"] else None
        c.execute("""INSERT INTO station(name,address,area,longitude,latitude,total_chargers,online_rate,
                     service_fee,parking_fee,business_hours,facilities,owner_type,merchant_id,has_swap)
                     VALUES(?,?,?,?,?,0,100,?,?,?,?,?,?,?)""",
                  (s["name"], s["address"], s["area"], s["longitude"], s["latitude"], s["service_fee"],
                   s["parking_fee"], s["business_hours"], json.dumps(s["facilities"], ensure_ascii=False),
                   s["owner_type"], mid, s["has_swap"]))
        station_id_by_name[s["name"]] = c.lastrowid

    # 分时电价: station_ref 为 null => 全站通用, 否则按电站名解析为专属站规则
    for p in seed_rows(seed["price_rule"]):
        sid = station_id_by_name[p["station_ref"]] if p["station_ref"] else None
        c.execute("INSERT INTO price_rule(station_id,level,price,time_range) VALUES(?,?,?,?)",
                  (sid, p["level"], p["price"], p["time_range"]))

    # 管理员
    for a in seed_rows(seed["admin"]):
        c.execute("INSERT INTO admin(account,password) VALUES(?,?)", (a["account"], a["password"]))

    # ---------- 充电桩: 按每站 charger_count 台动态生成 (快慢混合, 前两站各留1台故障演示) ----------
    # station_cfg: 有序 [ (station_id, name, charger_count) ], 供桩编码前缀与计数回填
    station_cfg = [(station_id_by_name[s["name"]], s["name"], int(s.get("charger_count", 10)))
                   for s in seed_rows(seed["station"])]
    chargers = []             # (id, station_id, type, power)
    charger_ref = {}          # (station_id, 第n台) -> charger_id, 供故障/告警引用
    fault_assigned = []       # 演示用故障桩: [(station_id, 第n台)]
    for (sid, _nm, cps), is_fault in zip(station_cfg, [True, True] + [False] * len(station_cfg)):
        if is_fault and cps >= 3:
            fault_assigned.append((sid, cps - 3))
    for pos, (sid, _nm, cps) in enumerate(station_cfg, 1):
        fast_n = min(6, max(1, cps // 2 + 1))          # 快充约一半(上限6), 其余慢充
        for i in range(1, cps + 1):
            typ   = "fast" if i <= fast_n else "slow"
            power = random.choice([120, 90, 60]) if typ == "fast" else 7
            status = "idle"
            fault_code, comm, health = "", "normal", random.randint(88, 100)
            if (sid, i) in fault_assigned:
                status, fault_code, comm, health = "fault", "E-0301", "abnormal", random.randint(35, 55)
            # 状态分布多样化: 管理端首页的状态环形图/大屏需要 idle 以外的样本,
            # 否则所有桩全是 idle, 图形毫无信息量。
            elif (sid + i) % 7 == 0:
                status, health = "charging", random.randint(75, 95)
            elif (sid + i) % 11 == 0:
                status = "reserved"
            elif i == cps and pos % 2 == 1:
                status, comm, health = "offline", "abnormal", random.randint(60, 80)
            c.execute("""INSERT INTO charger(code,station_id,type,power,status,voltage,current,temperature,
                         fault_code,comm_status,health_score,total_charge_count,total_charge_duration,created_time)
                         VALUES(?,?,?,?,?,0,0,?,?,?,?,0,0,?)""",
                      (f"{chr(64+pos)}-{i:03d}", sid, typ, power, status,
                       round(random.uniform(22, 28), 1), fault_code, comm, health,
                       ts(NOW - timedelta(days=random.randint(200, 600)))))
            charger_ref[(sid, i)] = c.lastrowid
            chargers.append((c.lastrowid, sid, typ, power))
    # 每站桩数回填; 有故障桩的站 → 在线率=(桩数-1)/桩数, 其余 100
    for sid, _nm, cps in station_cfg:
        c.execute("UPDATE station SET total_chargers=?, online_rate=? WHERE id=?",
                  (cps, 100, sid))
    for f_sid, _f_i in fault_assigned:
        cps = next(c for s_sid, _n, c in station_cfg if s_sid == f_sid)
        c.execute("UPDATE station SET online_rate=? WHERE id=?",
                  (round((cps-1)/cps*100), f_sid))

    # ---------- 用户 与 车辆 ----------
    # 用户: level 取自 JSON(vip 用于权益演示); register_time 保持随机(半动态, 不搬进 JSON)
    user_id_by_phone = {}
    for s in seed_rows(seed["user"]):
        c.execute("""INSERT INTO user(phone,nickname,avatar_path,balance,points,level,status,register_time)
                     VALUES(?,?,'',0,0,?, 'normal', ?)""",
                  (s["phone"], s["nickname"], s["level"],
                   ts(NOW - timedelta(days=random.randint(30, 120)))))
        user_id_by_phone[s["phone"]] = c.lastrowid
    user_ids = list(user_id_by_phone.values())   # 有序用户id, 供订单/回填等按 JSON 数量动态驱动
    # 车辆: user_ref(手机号) 解析为 user_id
    for v in seed_rows(seed["vehicle"]):
        c.execute("""INSERT INTO vehicle(user_id,name,type,battery_kwh,connector_type,max_power_kw,is_default,created_time)
                     VALUES(?,?,?,?,?,?,?,?)""",
                  (user_id_by_phone[v["user_ref"]], v["name"], v["type"], v["battery_kwh"],
                   v["connector_type"], v["max_power_kw"], v["is_default"], ts(NOW - timedelta(days=25))))

    # ---------- 优惠券模板 + 发券 ----------
    # 注意: 下方发券逻辑硬编码 coupon_id=1(新人券), 故 JSON 中 coupon 列表顺序不可调(第一张=新人立减5元)
    for cp in seed_rows(seed["coupon"]):
        c.execute("""INSERT INTO coupon(title,type,discount_amount,min_amount,station_id,time_range,valid_days,total,status,create_time)
                     VALUES(?,?,?,?,NULL,?,?,?,?,?)""",
                  (cp["title"], cp["type"], cp["discount_amount"], cp["min_amount"], cp["time_range"],
                   cp["valid_days"], cp["total"], cp["status"], ts(NOW - timedelta(days=cp["create_days_ago"]))))
    for uid in user_id_by_phone.values():   # 每个用户一张未用的新人券
        c.execute("""INSERT INTO user_coupon(user_id,coupon_id,status,receive_time)
                     VALUES(?,1,'unused',?)""", (uid, ts(NOW - timedelta(days=20))))

    # ---------- 历史订单 (近30天, 早晚双峰) + 流水/积分/时间轴 ----------
    hour_weight = [1,1,1,1,1,2,4,8,10,7,4,3,3,3,4,5,8,10,9,6,4,3,2,1]   # 双峰权重
    balances = {uid: 0.0 for uid in user_ids}
    points   = {uid: 0   for uid in user_ids}
    charger_stat = {cid: [0, 0] for cid, *_ in chargers}                 # cid -> [次数, 分钟]
    order_rows = 0
    # day=0 即"今天": 必须有订单, 否则管理端首页的"今日营收"永远是 0
    for day in range(30, -1, -1):
        base = NOW - timedelta(days=day)
        n = random.randint(16, 22) if base.weekday() < 5 else random.randint(10, 15)
        if day == 0:
            n = max(5, n * max(1, NOW.hour) // 24)   # 今天只过了一部分, 订单量按比例缩减
        for _ in range(n):
            hour = random.choices(range(24), weights=hour_weight)[0]
            if day == 0:
                hour = random.randrange(0, max(1, NOW.hour))   # 今天的订单只能发生在已过去的小时里
            uid  = random.choice(user_ids)
            cid, sid, typ, power = random.choice(chargers)
            dur  = random.randint(40, 90) if typ == "fast" else random.randint(120, 300)
            start = base.replace(hour=hour, minute=random.randint(0, 59))
            end   = start + timedelta(minutes=dur)
            level, price = price_level_of(hour)
            energy = round(power * dur / 60 * random.uniform(0.80, 0.95), 2)
            amount = round(energy * price, 2)
            pay    = amount
            s_soc  = round(random.uniform(10, 40), 1)
            e_soc  = min(100.0, round(s_soc + energy, 1))
            # 充值保证余额够扣 (充值也写流水)
            if balances[uid] < pay:
                topup = math.ceil((pay - balances[uid] + 50) / 50) * 50
                balances[uid] += topup
                c.execute("""INSERT INTO wallet_transaction(user_id,type,amount,balance_after,order_id,remark,create_time)
                             VALUES(?,'recharge',?,?,NULL,'余额充值',?)""",
                          (uid, topup, round(balances[uid], 2), ts(start - timedelta(minutes=5))))
            c.execute("""INSERT INTO charging_order(user_id,station_id,charger_id,vehicle_id,status,
                         start_soc,target_soc,end_soc,start_time,end_time,duration_min,energy_kwh,
                         price_level,amount,discount_amount,pay_amount,points_used,coupon_id,points_earned,
                         create_time,settle_time)
                         VALUES(?,?,?,NULL,'completed',?,95,?,?,?,?,?,?,?,0,?,0,NULL,?,?,?)""",
                      (uid, sid, cid, s_soc, e_soc, ts(start), ts(end), dur, energy,
                       level, amount, pay, int(energy // 10), ts(start - timedelta(minutes=6)),
                       ts(end + timedelta(minutes=2))))
            oid = c.lastrowid
            order_rows += 1
            balances[uid] = round(balances[uid] - pay, 2)
            earned = int(energy // 10)
            points[uid] += earned
            c.execute("""INSERT INTO wallet_transaction(user_id,type,amount,balance_after,order_id,remark,create_time)
                         VALUES(?,'consume',?,?,?,'充电结算',?)""",
                      (uid, -pay, balances[uid], oid, ts(end + timedelta(minutes=2))))
            if earned:
                c.execute("INSERT INTO point_record(user_id,change,reason,create_time) VALUES(?,?,'charge',?)",
                          (uid, earned, ts(end + timedelta(minutes=2))))
            charger_stat[cid][0] += 1
            charger_stat[cid][1] += dur
    # 桩累计数回填
    for cid, (cnt, mins) in charger_stat.items():
        c.execute("UPDATE charger SET total_charge_count=?, total_charge_duration=? WHERE id=?", (cnt, mins, cid))
    # 用户余额/积分回填
    for uid in user_ids:
        c.execute("UPDATE user SET balance=?, points=?, last_login_time=? WHERE id=?",
                  (balances[uid], points[uid], ts(NOW - timedelta(hours=random.randint(1, 48))), uid))

    # ---------- 一笔"充电中"未结算订单 (演示强制结算流程) ----------
    cid, sid, typ, power = next(x for x in chargers if x[2] == "fast")
    st = NOW - timedelta(minutes=35)
    c.execute("""INSERT INTO charging_order(user_id,station_id,charger_id,vehicle_id,status,start_soc,target_soc,
                 start_time,duration_min,energy_kwh,price_level,amount,discount_amount,pay_amount,
                 points_used,points_earned,create_time)
                 VALUES(1,?,?,1,'charging',22.5,95,?,0,0,'flat',0,0,0,0,0,?)""",
              (sid, cid, ts(st), ts(st - timedelta(minutes=4))))
    active_oid = c.lastrowid
    c.execute("UPDATE charger SET status='charging', voltage=380, current=156 WHERE id=?", (cid,))
    for node, label, mins in [("reserved","预约成功",-4),("arrived","到达充电站",-1),("started","开始充电",0)]:
        c.execute("""INSERT INTO order_timeline(order_id,node,label,event_time) VALUES(?,?,?,?)""",
                  (active_oid, node, label, ts(st + timedelta(minutes=mins))))

    # ---------- 评价 / 工单 / 告警 / 设备日志 / 通知 ----------
    station_ids = [sid for sid, _nm, _c in station_cfg]
    station_name = {sid: nm for sid, nm, _c in station_cfg}          # 供通知文案引用站名
    tags_pool = ["fast_charge","spacious","quiet_night","good_service","good_fast_charge","heavy_queue"]
    for _ in range(12):
        uid, sid = random.choice(user_ids), random.choice(station_ids)
        base = round(random.uniform(3.5, 5.0), 1)
        c.execute("""INSERT INTO review(user_id,station_id,order_id,overall_score,speed_score,device_score,
                     parking_score,hygiene_score,service_score,tags,content,useful_count,status,create_time)
                     VALUES(?,?,NULL,?,?,?,?,?,?,?,?,?, 'normal', ?)""",
                  (uid, sid, base, base, round(random.uniform(3, 5), 1), round(random.uniform(3, 5), 1),
                   round(random.uniform(3, 5), 1), round(random.uniform(3.5, 5), 1),
                   str([random.choice(tags_pool), random.choice(tags_pool)]).replace("'", '"'),
                   "充电速度不错，位置好找。", random.randint(0, 8),
                   ts(NOW - timedelta(days=random.randint(1, 25)))))
    # 故障桩演示: 工单/告警/设备日志引用前两处故障桩, 保证 FK 一致
    def _fault_charger(idx):
        if idx < len(fault_assigned):
            return fault_assigned[idx]
        return None
    f0 = _fault_charger(0)
    if f0:
        f0_sid, f0_i = f0
        f0_cid = charger_ref[(f0_sid, f0_i)]
        c.execute("""INSERT INTO work_order(type,priority,user_id,station_id,charger_id,title,description,status,handler,create_time)
                     VALUES('device_fault','high',NULL,?,?,?,?,?,?,?)""",
                  (f0_sid, f0_cid, f"{chr(64+station_ids.index(f0_sid)+1)}-{f0_i:03d}桩通信异常",
                   '桩自检报E-0301, 远程重启无效',
                   'processing', 'admin', ts(NOW - timedelta(days=1))))
        c.execute("""INSERT INTO alarm(charger_id,station_id,type,level,occur_time,status,handle_action)
                     VALUES(?,?,'comm_abnormal','critical',?,'open','')""",
                  (f0_cid, f0_sid, ts(NOW - timedelta(days=1, hours=2))))
        c.execute("""INSERT INTO device_log(charger_id,action,operator,op_time,result)
                     VALUES(?,'restart','admin',?,'failed')""", (f0_cid, ts(NOW - timedelta(days=1, hours=1))))
    if len(user_ids) > 1 and len(station_ids) > 2:
        c.execute("""INSERT INTO work_order(type,priority,user_id,station_id,charger_id,title,description,status,handler,create_time)
                     VALUES('user_complaint','medium',?,?,NULL,'停车费争议','充电结束后被收停车费, 请核实','pending','',?)""",
                  (user_ids[1], station_ids[2], ts(NOW - timedelta(hours=6))))
    f1 = _fault_charger(1)
    if f1:
        f1_sid, f1_i = f1
        f1_cid = charger_ref[(f1_sid, f1_i)]
        c.execute("""INSERT INTO alarm(charger_id,station_id,type,level,occur_time,status,handle_action)
                     VALUES(?,?,'offline','warning',?,'handled','repair')""",
                  (f1_cid, f1_sid, ts(NOW - timedelta(days=3))))
    # 近 7 日注册的散户: 大屏"用户增长"曲线需要样本, 否则 7 天全是 0。
    # 这些用户不下单, 只用于增长曲线; 主测试账号仍是 13800000001~5。
    for k in range(15):
        d_ago = k % 7
        c.execute("""INSERT INTO user(phone,nickname,balance,points,level,status,register_time,last_login_time)
                     VALUES(?,?,?,?,'normal','normal',?,?)""",
                  (f"1370000{k:04d}", f"新用户{k+1:02d}",
                   round(random.uniform(0, 80), 2), random.randint(0, 200),
                   ts(NOW - timedelta(days=d_ago, hours=random.randint(1, 10))),
                   ts(NOW - timedelta(days=d_ago, hours=random.randint(0, 5)))))

    # 大屏告警面板需要多几条不同级别/类型的样本, 否则只有两行, 演示时很空
    # 按站取若干块桩制造告警: 每站取前若干个充电桩 id, 保证 FK 一致
    chargers_by_station = {}
    for cid, sid, _typ, _pw in chargers:
        chargers_by_station.setdefault(sid, []).append(cid)
    for st_seq, a_type, a_lvl, a_hrs, a_status in [
        (0, 'overheat',      'critical', 3,  'open'),
        (2, 'power_drop',    'warning',  6,  'open'),
        (4, 'offline',       'warning',  9,  'handled'),
        (1, 'user_behavior', 'info',     14, 'handled'),
        (2, 'comm_abnormal', 'warning',  20, 'open'),
    ]:
        if st_seq >= len(station_cfg):
            continue
        sid = station_cfg[st_seq][0]
        pool = chargers_by_station.get(sid, [])
        if not pool:
            continue
        a_cid = pool[-1]                          # 取该站最末一台桩做告警样本
        c.execute("""INSERT INTO alarm(charger_id,station_id,type,level,occur_time,status,handle_action)
                     VALUES(?,?,?,?,?,?,?)""",
                  (a_cid, sid, a_type, a_lvl, ts(NOW - timedelta(hours=a_hrs)), a_status,
                   'repair' if a_status == 'handled' else ''))

    c.execute("""INSERT INTO notification(user_id,type,title,content,related_id,is_read,create_time)
                 VALUES(1,'order','充电进行中',?,?,0,?)""",
              (f"您正在 {station_name[sid]} 充电", active_oid, ts(st)))

    # 每个测试用户几条通知, 供用户端消息中心展示 (文案来自 JSON notif_seed)
    notif_seeds = seed_rows(seed["notif_seed"])
    for uid in user_ids:
        for k, n in enumerate(notif_seeds):
            if (uid + k) % 2 == 0:                       # 不是每人都全给, 免得千篇一律
                continue
            c.execute("""INSERT INTO notification(user_id,type,title,content,related_id,is_read,create_time)
                         VALUES(?,?,?,?,NULL,?,?)""",
                      (uid, n["type"], n["title"], n["content"], n["is_read"],
                       ts(NOW - timedelta(hours=n["hours_ago"] + uid))))

    # ---------- 时序测量 (近3天, 每15分钟, 基线+双峰正弦+噪声) ----------
    rows = []
    for cid, sid, typ, power in chargers:
        cap = power
        for step in range(3 * 96):                      # 3天 × 96个15分钟
            t = NOW - timedelta(minutes=15 * (3 * 96 - step))
            h = t.hour + t.minute / 60
            load = (0.15
                    + 0.45 * math.exp(-((h - 8.5) ** 2) / 3.0)     # 早高峰
                    + 0.55 * math.exp(-((h - 18.5) ** 2) / 4.0)    # 晚高峰
                    + random.uniform(-0.08, 0.08))
            load = max(0.0, min(1.0, load))
            pkw  = round(cap * load, 2)
            rows.append((cid, sid, ts(t), pkw, round(random.uniform(20, 90), 1),
                         round(pkw * 0.25, 3), round(24 + 14 * load + random.uniform(-2, 2), 1)))
    c.executemany("""INSERT INTO charging_measure(charger_id,station_id,measure_time,power_kw,soc,energy_delta_kwh,temperature)
                     VALUES(?,?,?,?,?,?,?)""", rows)

    # ---------- 天气 / 节假日 (来自 JSON) ----------
    for w in seed_rows(seed["weather"]):
        c.execute("INSERT INTO weather(area,condition,temperature,forecast,update_time) VALUES(?,?,?,?,?)",
                  (w["area"], w["condition"], w["temperature"], w["forecast"], ts(NOW)))
    for h in seed_rows(seed["holiday"]):
        c.execute("INSERT INTO holiday(date,name,is_workday,create_time) VALUES(?,?,?,?)",
                  (h["date"], h["name"], h["is_workday"], ts(NOW)))

    # ---------- 套餐 / FAQ / 换电域 ----------
    # 会员套餐 (来自 JSON; 注意下列 user_plan 取 vip 用户 + plan_id=2=畅充月卡, JSON 顺序不可调)
    for p in seed_rows(seed["member_plan"]):
        c.execute("""INSERT INTO member_plan(name,price,valid_days,service_fee_discount,night_discount,
                     points_multiplier,status,description,create_time) VALUES(?,?,?,?,?,?,'active',?,?)""",
                  (p["name"], p["price"], p["valid_days"], p["service_fee_discount"], p["night_discount"],
                   p["points_multiplier"], p["description"], ts(NOW - timedelta(days=40))))
    # vip 用户开一张月卡 (plan_id=2); 从 JSON 找升级的 vip 用户
    vip_uid = next((uid for uid in user_ids if uid == user_id_by_phone.get(
        next(s["phone"] for s in seed_rows(seed["user"]) if s["level"] == "vip"), -1)), user_ids[0])
    c.execute("""INSERT INTO user_plan(user_id,plan_id,start_time,end_time,status,create_time)
                 VALUES(?,2,?,?,'active',?)""",
              (vip_uid, ts(NOW - timedelta(days=10)), ts(NOW + timedelta(days=20)), ts(NOW - timedelta(days=10))))
    # 收藏: 前几个测试用户各收藏 1-2 个站 (站点取自 JSON 列表)
    for k, uid in enumerate(user_ids[:4]):
        sid_k = station_ids[k % len(station_ids)]                 # 收藏"自己站序"对应的站
        sid_2 = station_ids[(k + 2) % len(station_ids)]           # 再收藏一个跨区域站
        for sid in ({sid_k, sid_2}):
            c.execute("INSERT INTO favorite(user_id,station_id,create_time) VALUES(?,?,?)",
                      (uid, sid, ts(NOW - timedelta(days=uid + sid))))
    # FAQ (来自 JSON)
    for f in seed_rows(seed["faq"]):
        c.execute("INSERT INTO faq(category,question,answer,sort,enabled,create_time) VALUES(?,?,?,?,1,?)",
                  (f["category"], f["question"], f["answer"], f["sort"], ts(NOW)))
    # ---------- 换电域: 给"换电站"(has_swap=1) 造 12 块电池 + 1 笔换电订单 ----------
    swap_sid = next((station_id_by_name[s["name"]] for s in seed_rows(seed["station"]) if s.get("has_swap")), None)
    swap_uid = user_ids[-1] if len(user_ids) > 1 else user_ids[0]    # 用最后一个测试用户演示换电
    if swap_sid is not None:
        for i in range(1, 13):
            c.execute("""INSERT INTO battery(code,station_id,soc,health_score,temperature,status,swap_count,create_time)
                         VALUES(?,?,?,?,?,?,?,?)""",
                      (f"BAT-{i:03d}", swap_sid, round(random.uniform(20, 100), 1), random.randint(80, 100),
                       round(random.uniform(22, 30), 1),
                       "charging" if i <= 3 else "idle", random.randint(0, 60), ts(NOW - timedelta(days=100))))
        c.execute("""INSERT INTO swap_order(user_id,station_id,battery_in_id,battery_out_id,amount,status,create_time)
                     VALUES(?,?,1,5,35.0,'completed',?)""", (swap_uid, swap_sid, ts(NOW - timedelta(days=2))))
        c.execute("""INSERT INTO wallet_transaction(user_id,type,amount,balance_after,order_id,remark,create_time)
                     VALUES(?,'consume',-35.0,?, NULL,'换电',?)""",
                  (swap_uid, round(balances[swap_uid] - 35.0, 2), ts(NOW - timedelta(days=2))))
        c.execute("UPDATE user SET balance=balance-35.0 WHERE id=?", (swap_uid,))
    # 商户汇总回填: 取"合作运营"(merchant_ref 非空) 的那座站
    partner_sid = next((station_id_by_name[s["name"]] for s in seed_rows(seed["station"])
                        if s.get("merchant_ref") and station_id_by_name.get(s["name"])), 1)
    c.execute("""UPDATE merchant SET
                 order_count=(SELECT COUNT(*) FROM charging_order WHERE station_id=?),
                 settle_amount=IFNULL((SELECT ROUND(SUM(pay_amount),2) FROM charging_order WHERE station_id=? AND status='completed'),0),
                 service_score=IFNULL((SELECT ROUND(AVG(overall_score),2) FROM review WHERE station_id=?),0)
                 WHERE id=1""", (partner_sid, partner_sid, partner_sid))

    con.commit()
    # ---------- 自检 ----------
    bad = con.execute("PRAGMA foreign_key_check").fetchall()
    assert not bad, f"外键检查未通过: {bad}"
    print(f"charge.db 生成完毕 → {DB}")
    for t in ["user","station","charger","charging_order","wallet_transaction","point_record",
              "charging_measure","review","work_order","alarm","holiday","battery"]:
        print(f"  {t:20s} {con.execute('SELECT COUNT(*) FROM ' + t).fetchone()[0]:>6} 行")
    print("近7日营收(演示销售业绩图):")
    for row in con.execute("""SELECT substr(settle_time,1,10) d, ROUND(SUM(pay_amount),2)
                              FROM charging_order WHERE status='completed'
                              GROUP BY d ORDER BY d DESC LIMIT 7"""):
        print(f"  {row[0]}  {row[1]:>9} 元")
    con.close()

if __name__ == "__main__":
    main()
