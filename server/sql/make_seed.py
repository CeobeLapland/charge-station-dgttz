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
NOW  = datetime.now().replace(hour=12, minute=0, second=0, microsecond=0)

def ts(dt):  return dt.strftime("%Y-%m-%d %H:%M:%S")

def price_level_of(hour):
    """分时电价档位: 谷 00-08, 峰 17-21, 其余平"""
    if hour < 8:            return "valley", 0.40
    if 17 <= hour < 21:     return "peak",   1.00
    return "flat", 0.70

def main():
    if os.path.exists(DB):
        os.remove(DB)
    con = sqlite3.connect(DB)
    con.execute("PRAGMA foreign_keys=ON")
    con.executescript(open(os.path.join(HERE, "schema.sql"), encoding="utf-8").read())
    c = con.cursor()

    # ---------- 管理员 ----------
    c.execute("INSERT INTO admin(account,password) VALUES('admin','123456')")

    # ---------- 商户 ----------
    c.execute("""INSERT INTO merchant(name,contact_name,contact_phone,cooperation_type,status,remark,create_time)
                 VALUES('绿能合作运营有限公司','李经理','13900001234','partner','active','大学城站合作方',?)""",
              (ts(NOW - timedelta(days=90)),))
    merchant_id = c.lastrowid

    # ---------- 充电站 (5座, 覆盖不同区域/归属/设施, 1座换电) ----------
    stations = [
        # name, address, area, lng, lat, service_fee, parking_fee, hours, facilities, owner, merch, swap
        ("市中心快充站", "解放路100号",   "商业区", 123.4310, 41.8057, 0.80, 5, "00:00-24:00",
         '["convenience_store","wifi","rain_shelter"]', "self_run", None, 0),
        ("高新软件园站", "创新路2号",     "高新区", 123.4587, 41.7423, 0.60, 0, "00:00-24:00",
         '["washroom","rest_area","underground_parking"]', "self_run", None, 0),
        ("大学城充电站", "文华街33号",    "大学城", 123.4123, 41.7669, 0.50, 0, "06:00-23:00",
         '["washroom","convenience_store"]', "partner", merchant_id, 0),
        ("老城区慢充站", "中山巷8号",     "老城区", 123.3958, 41.8121, 0.40, 2, "00:00-24:00",
         '[]', "self_run", None, 0),
        ("滨河换电综合站", "滨河大道66号", "滨河区", 123.4776, 41.7890, 0.70, 0, "00:00-24:00",
         '["rest_area","wifi","rain_shelter","underground_parking"]', "self_run", None, 1),
    ]
    for s in stations:
        c.execute("""INSERT INTO station(name,address,area,longitude,latitude,total_chargers,online_rate,
                     service_fee,parking_fee,business_hours,facilities,owner_type,merchant_id,has_swap)
                     VALUES(?,?,?,?,?,0,100,?,?,?,?,?,?,?)""",
                  (s[0], s[1], s[2], s[3], s[4], s[5], s[6], s[7], s[8], s[9], s[10], s[11]))

    # ---------- 分时电价 (全站通用三档 + 2条专属站规则) ----------
    for level, price, rng in [("valley",0.40,"00:00-08:00"),("flat",0.70,"08:00-17:00"),
                              ("peak",1.00,"17:00-21:00"),("flat",0.70,"21:00-24:00")]:
        c.execute("INSERT INTO price_rule(station_id,level,price,time_range) VALUES(NULL,?,?,?)",
                  (level, price, rng))
    c.execute("INSERT INTO price_rule(station_id,level,price,time_range) VALUES(3,'valley',0.35,'00:00-08:00')")  # 大学城谷价更低
    c.execute("INSERT INTO price_rule(station_id,level,price,time_range) VALUES(1,'peak',1.20,'17:00-21:00')")    # 市中心峰价更高

    # ---------- 充电桩 (每站10台, 快慢混合, 全场共2台故障) ----------
    chargers = []          # (id, station_id, type, power)
    fault_assigned = [(1, 7), (4, 3)]      # (station_id, 第n台) 设为故障
    for sid in range(1, 6):
        for i in range(1, 11):
            typ   = "fast" if i <= 6 else "slow"
            power = random.choice([120, 90, 60]) if typ == "fast" else 7
            status = "idle"
            fault_code, comm, health = "", "normal", random.randint(88, 100)
            if (sid, i) in fault_assigned:
                status, fault_code, comm, health = "fault", "E-0301", "abnormal", random.randint(35, 55)
            # 状态分布多样化: 管理端首页的状态环形图/大屏需要 idle 以外的样本,
            # 否则演示时 50 台桩全是 idle, 图形毫无信息量。
            elif (sid + i) % 7 == 0:
                status, health = "charging", random.randint(75, 95)
            elif (sid + i) % 11 == 0:
                status = "reserved"
            elif i == 10 and sid % 2 == 1:
                status, comm, health = "offline", "abnormal", random.randint(60, 80)
            c.execute("""INSERT INTO charger(code,station_id,type,power,status,voltage,current,temperature,
                         fault_code,comm_status,health_score,total_charge_count,total_charge_duration,created_time)
                         VALUES(?,?,?,?,?,0,0,?,?,?,?,0,0,?)""",
                      (f"{chr(64+sid)}-{i:03d}", sid, typ, power, status,
                       round(random.uniform(22, 28), 1), fault_code, comm, health,
                       ts(NOW - timedelta(days=random.randint(200, 600)))))
            chargers.append((c.lastrowid, sid, typ, power))
    c.execute("UPDATE station SET total_chargers=10")
    c.execute("UPDATE station SET online_rate=90 WHERE id IN (1,4)")   # 各有1台故障 → 9/10

    # ---------- 用户 (5个) 与车辆 ----------
    users = [
        ("13800000001", "用户0001", 120.0), ("13800000002", "早高峰通勤者", 60.0),
        ("13800000003", "用户0003", 200.0), ("13800000004", "夜猫子网约车", 30.0),
        ("13800000005", "用户0005", 88.8),
    ]
    for i, (phone, nick, _bal) in enumerate(users, 1):
        c.execute("""INSERT INTO user(phone,nickname,avatar_path,balance,points,level,status,register_time)
                     VALUES(?,?,'',0,0,?, 'normal', ?)""",
                  (phone, nick, "vip" if i == 3 else "normal",
                   ts(NOW - timedelta(days=random.randint(30, 120)))))
    vehicles = [
        (1, "我的比亚迪", "car", 60, "dc_gb", 120, 1), (1, "买菜小车", "car", 30, "ac_gb", 7, 0),
        (2, "通勤电车",   "car", 75, "dc_gb", 150, 1),
        (3, "公司货车",   "light_truck", 100, "dc_gb", 120, 1),
        (4, "网约车",     "car", 60, "dc_gb", 90, 1),
        (5, "小电驴",     "two_wheeler", 2, "ac_gb", 0.5, 1),
    ]
    for v in vehicles:
        c.execute("""INSERT INTO vehicle(user_id,name,type,battery_kwh,connector_type,max_power_kw,is_default,created_time)
                     VALUES(?,?,?,?,?,?,?,?)""", v + (ts(NOW - timedelta(days=25)),))

    # ---------- 优惠券模板 + 发券 ----------
    c.execute("""INSERT INTO coupon(title,type,discount_amount,min_amount,station_id,time_range,valid_days,total,status,create_time)
                 VALUES('新人立减5元','new_user',5,0,NULL,'',30,-1,'active',?)""", (ts(NOW - timedelta(days=60)),))
    c.execute("""INSERT INTO coupon(title,type,discount_amount,min_amount,station_id,time_range,valid_days,total,status,create_time)
                 VALUES('满50减8','full_reduction',8,50,NULL,'',15,500,'active',?)""", (ts(NOW - timedelta(days=30)),))
    c.execute("""INSERT INTO coupon(title,type,discount_amount,min_amount,station_id,time_range,valid_days,total,status,create_time)
                 VALUES('夜间充电券','night',3,0,NULL,'22:00-06:00',30,-1,'active',?)""", (ts(NOW - timedelta(days=30)),))
    for uid in range(1, 6):   # 每人一张未用的新人券
        c.execute("""INSERT INTO user_coupon(user_id,coupon_id,status,receive_time)
                     VALUES(?,1,'unused',?)""", (uid, ts(NOW - timedelta(days=20))))

    # ---------- 历史订单 (近30天, 早晚双峰) + 流水/积分/时间轴 ----------
    hour_weight = [1,1,1,1,1,2,4,8,10,7,4,3,3,3,4,5,8,10,9,6,4,3,2,1]   # 双峰权重
    balances = {i: 0.0 for i in range(1, 6)}
    points   = {i: 0   for i in range(1, 6)}
    charger_stat = {cid: [0, 0] for cid, *_ in chargers}                 # cid -> [次数, 分钟]
    order_rows = 0
    # day=0 即"今天": 必须有订单, 否则管理端首页的"今日营收"永远是 0
    for day in range(30, -1, -1):
        base = NOW - timedelta(days=day)
        n = random.randint(16, 22) if base.weekday() < 5 else random.randint(10, 15)
        if day == 0:
            n = max(5, n // 2)          # 今天才过了半天, 订单量减半
        for _ in range(n):
            hour = random.choices(range(24), weights=hour_weight)[0]
            if day == 0:
                hour %= 11              # NOW 是中午, 今天的订单只发生在上午
            uid  = random.randint(1, 5)
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
    for uid in range(1, 6):
        c.execute("UPDATE user SET balance=?, points=?, last_login_time=? WHERE id=?",
                  (balances[uid], points[uid], ts(NOW - timedelta(hours=random.randint(1, 48))), uid))

    # ---------- 一笔"充电中"未结算订单 (演示强制结算流程) ----------
    cid, sid, typ, power = next(x for x in chargers if x[2] == "fast" and x[1] == 2)
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
    tags_pool = ["fast_charge","spacious","quiet_night","good_service","good_fast_charge","heavy_queue"]
    for _ in range(12):
        uid, sid = random.randint(1, 5), random.randint(1, 5)
        base = round(random.uniform(3.5, 5.0), 1)
        c.execute("""INSERT INTO review(user_id,station_id,order_id,overall_score,speed_score,device_score,
                     parking_score,hygiene_score,service_score,tags,content,useful_count,status,create_time)
                     VALUES(?,?,NULL,?,?,?,?,?,?,?,?,?, 'normal', ?)""",
                  (uid, sid, base, base, round(random.uniform(3, 5), 1), round(random.uniform(3, 5), 1),
                   round(random.uniform(3, 5), 1), round(random.uniform(3.5, 5), 1),
                   str([random.choice(tags_pool), random.choice(tags_pool)]).replace("'", '"'),
                   "充电速度不错，位置好找。", random.randint(0, 8),
                   ts(NOW - timedelta(days=random.randint(1, 25)))))
    c.execute("""INSERT INTO work_order(type,priority,user_id,station_id,charger_id,title,description,status,handler,create_time)
                 VALUES('device_fault','high',NULL,1,7,'A-007桩通信异常','桩自检报E-0301, 远程重启无效','processing','admin',?)""",
              (ts(NOW - timedelta(days=1)),))
    c.execute("""INSERT INTO work_order(type,priority,user_id,station_id,charger_id,title,description,status,handler,create_time)
                 VALUES('user_complaint','medium',2,3,NULL,'停车费争议','充电结束后被收停车费, 请核实','pending','',?)""",
              (ts(NOW - timedelta(hours=6)),))
    c.execute("""INSERT INTO alarm(charger_id,station_id,type,level,occur_time,status,handle_action)
                 VALUES(7,1,'comm_abnormal','critical',?,'open','')""", (ts(NOW - timedelta(days=1, hours=2)),))
    c.execute("""INSERT INTO alarm(charger_id,station_id,type,level,occur_time,status,handle_action)
                 VALUES(33,4,'offline','warning',?,'handled','repair')""", (ts(NOW - timedelta(days=3)),))
    c.execute("""INSERT INTO device_log(charger_id,action,operator,op_time,result)
                 VALUES(7,'restart','admin',?,'failed')""", (ts(NOW - timedelta(days=1, hours=1)),))
    c.execute("""INSERT INTO notification(user_id,type,title,content,related_id,is_read,create_time)
                 VALUES(1,'order','充电进行中','您在高新软件园站的充电已开始',?,0,?)""", (active_oid, ts(st)))

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

    # ---------- 天气 / 节假日 ----------
    c.execute("INSERT INTO weather(area,condition,temperature,forecast,update_time) VALUES('','sunny',27,NULL,?)", (ts(NOW),))
    for area, cond, temp in [("商业区","sunny",28),("高新区","cloudy",26),("大学城","sunny",27),
                             ("老城区","cloudy",26),("滨河区","rain",24)]:
        c.execute("INSERT INTO weather(area,condition,temperature,forecast,update_time) VALUES(?,?,?,NULL,?)",
                  (area, cond, temp, ts(NOW)))
    holidays = [("2026-01-01","元旦",0),("2026-02-16","春节",0),("2026-02-17","春节",0),("2026-02-18","春节",0),
                ("2026-04-05","清明节",0),("2026-05-01","劳动节",0),("2026-06-19","端午节",0),
                ("2026-09-25","中秋节",0),("2026-10-01","国庆节",0),("2026-10-02","国庆节",0),
                ("2026-10-03","国庆节",0),("2026-09-27","国庆调休上班",1)]
    for d, name, wk in holidays:
        c.execute("INSERT INTO holiday(date,name,is_workday,create_time) VALUES(?,?,?,?)", (d, name, wk, ts(NOW)))

    # ---------- 套餐 / FAQ / 换电域 ----------
    c.execute("""INSERT INTO member_plan(name,price,valid_days,service_fee_discount,night_discount,points_multiplier,status,description,create_time)
                 VALUES('畅充月卡',19.9,30,0.8,0.9,1.5,'active','服务费8折, 夜间再9折, 1.5倍积分',?)""", (ts(NOW - timedelta(days=40)),))
    c.execute("""INSERT INTO user_plan(user_id,plan_id,start_time,end_time,status,create_time)
                 VALUES(3,1,?,?,'active',?)""",
              (ts(NOW - timedelta(days=10)), ts(NOW + timedelta(days=20)), ts(NOW - timedelta(days=10))))
    faqs = [("充电","怎么开始充电?","在电站详情选择空闲电桩, 点击开始充电即可。",1),
            ("费用","电费怎么计算?","电费=分时电价×充电量, 谷/平/峰价格见电站详情。",2),
            ("账户","余额如何充值?","进入我的-钱包, 输入金额点击充值(模拟支付)。",3),
            ("故障","电桩启动失败怎么办?","请换桩重试, 并在我的-工单提交故障反馈。",4)]
    for cat, q_, a_, srt in faqs:
        c.execute("INSERT INTO faq(category,question,answer,sort,enabled,create_time) VALUES(?,?,?,?,1,?)",
                  (cat, q_, a_, srt, ts(NOW)))
    for i in range(1, 13):    # 滨河换电站(id=5) 12块电池
        c.execute("""INSERT INTO battery(code,station_id,soc,health_score,temperature,status,swap_count,create_time)
                     VALUES(?,5,?,?,?,?,?,?)""",
                  (f"BAT-{i:03d}", round(random.uniform(20, 100), 1), random.randint(80, 100),
                   round(random.uniform(22, 30), 1),
                   "charging" if i <= 3 else "idle", random.randint(0, 60), ts(NOW - timedelta(days=100))))
    c.execute("""INSERT INTO swap_order(user_id,station_id,battery_in_id,battery_out_id,amount,status,create_time)
                 VALUES(4,5,1,5,35.0,'completed',?)""", (ts(NOW - timedelta(days=2)),))
    c.execute("""INSERT INTO wallet_transaction(user_id,type,amount,balance_after,order_id,remark,create_time)
                 VALUES(4,'consume',-35.0,?, NULL,'换电',?)""",
              (round(balances[4] - 35.0, 2), ts(NOW - timedelta(days=2))))
    c.execute("UPDATE user SET balance=balance-35.0 WHERE id=4")
    # 商户汇总回填 (大学城站=3)
    c.execute("""UPDATE merchant SET
                 order_count=(SELECT COUNT(*) FROM charging_order WHERE station_id=3),
                 settle_amount=IFNULL((SELECT ROUND(SUM(pay_amount),2) FROM charging_order WHERE station_id=3 AND status='completed'),0),
                 service_score=IFNULL((SELECT ROUND(AVG(overall_score),2) FROM review WHERE station_id=3),0)
                 WHERE id=1""")

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
