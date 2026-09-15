# -*- coding: utf-8 -*-
"""
dash_engine.py — 用 SparkSession 一次性聚合大屏数据，输出 JSON 缓存。
由网关在启动时/后台调用；`/api/snapshot` 读取该 JSON，秒级返回。
用法: python3 dash_engine.py <输出json路径> [region] [station_id] [date]
带筛选参数时，结果为该筛选域的数据。
"""
import sys
import json

OUT = sys.argv[1] if len(sys.argv) > 1 else "/home/hadoop/screen/dash_cache.json"
REGION = sys.argv[2] if len(sys.argv) > 2 else ""
SID = sys.argv[3] if len(sys.argv) > 3 else ""
DATE = sys.argv[4] if len(sys.argv) > 4 else ""

# 筛选 WHERE
W_ST = ""     # 站点表过滤  (这里 stations/charger 用它)
W_ORD = ""    # 订单 join station 过滤
W_ORD_DIR = ""# 订单直接 station_id 过滤
W_DT = ""     # 日期过滤
if REGION:
    W_ST += " and area='%s'" % REGION
    W_ORD += " and st.area='%s'" % REGION
if SID:
    W_ST += " and id='%s'" % SID
    W_ORD += " and st.id='%s'" % SID
    W_ORD_DIR += " and station_id='%s'" % SID
if DATE:
    W_DT = " and create_time like '%s%%'" % DATE

from pyspark.sql import SparkSession

spark = SparkSession.builder \
    .appName("dash_engine") \
    .master("local[2]") \
    .config("spark.sql.catalogImplementation", "hive") \
    .enableHiveSupport() \
    .getOrCreate()

def rows(sql):
    return [[str(c) if c is not None else "" for c in r] for r in spark.sql(sql).collect()]

def qr():
    return "join chargestation.station st on o.station_id=st.id"

result = {}
# 状态：按站点过滤时 join charger-station
result["status"] = rows("select ch.status as s, count(*) as c from chargestation.charger ch left join chargestation.station st on ch.station_id=st.id where 1=1%s group by ch.status" % W_ST)
result["charger_total"] = rows("select count(*) as c from chargestation.charger ch left join chargestation.station st on ch.station_id=st.id where 1=1%s" % W_ST)
result["station_total"] = rows("select count(*) as c from chargestation.station where 1=1%s" % W_ST)
result["orders"] = rows("select count(*) as cnt, round(sum(o.energy_kwh),1) as kwh, round(sum(o.pay_amount),2) as inc from chargestation.charging_order o %s where 1=1%s%s" % (qr(), W_ORD_DIR or W_ORD, W_DT if W_ORD_DIR else ""))
result["trend"] = rows("select substr(o.create_time,1,10) as d, count(*) as c, round(sum(o.energy_kwh),1) as kwh, round(sum(o.pay_amount),2) as inc from chargestation.charging_order o %s where 1=1%s%s group by substr(o.create_time,1,10) order by d desc limit 7" % (qr(), W_ORD_DIR or W_ORD, W_DT if W_ORD_DIR else ""))
result["rank"] = rows("select s.id as sid, s.name as snm, count(o.id) as c, round(sum(o.energy_kwh),1) as kwh from chargestation.charging_order o join chargestation.station s on o.station_id=s.id where 1=1%s%s group by s.id, s.name order by c desc limit 5" % (W_ORD, W_DT if not W_ORD_DIR else ""))
result["stations"] = rows("select id, name, area, longitude, latitude from chargestation.station where 1=1%s" % W_ST)
result["alarms"] = rows("select id, station_id, level, occur_time from chargestation.alarm where 1=1%s order by occur_time desc limit 10" % W_ORD_DIR)
result["user_trend"] = rows("select substr(register_time,1,10) as d, count(*) as c from chargestation.user group by substr(register_time,1,10) order by d desc limit 7")
result["measure_hour"] = rows("select cast(substr(m.measure_time,12,2) as int) as h, round(sum(m.power_kw),1) as tot, count(*) as c from chargestation.charging_measure m left join chargestation.station st on m.station_id=st.id where 1=1%s group by cast(substr(m.measure_time,12,2) as int)" % W_ST)
result["peakvalley"] = rows("select cast(substr(o.start_time,12,2) as int) as h, round(sum(o.energy_kwh),1) as kwh from chargestation.charging_order o %s where 1=1%s group by cast(substr(o.start_time,12,2) as int)" % (qr(), W_ORD_DIR or W_ORD))
result["total_energy"] = rows("select round(sum(o.energy_kwh),1) as e from chargestation.charging_order o %s where 1=1%s%s" % (qr(), W_ORD_DIR or W_ORD, W_DT if W_ORD_DIR else ""))
result["today"] = rows("select count(*) as cnt, round(sum(o.energy_kwh),1) as kwh, round(sum(o.pay_amount),2) as inc from chargestation.charging_order o %s where 1=1%s%s" % (qr(), W_ORD_DIR or W_ORD, W_DT if W_ORD_DIR else ""))
result["events"] = rows("select id, create_time, user_id, station_id from chargestation.charging_order where 1=1%s%s order by create_time desc limit 8" % (W_ORD_DIR, W_DT))

# 各表行数（供 /admin 数据表一览秒回；同一 SparkSession 内 30 个 count 共享 JVM，秒级）
try:
    _tabs = [r[1] for r in spark.sql("show tables in chargestation").collect()]
    result["table_counts"] = [[t, str(spark.sql("select count(*) as c from chargestation.%s" % t).collect()[0][0])] for t in _tabs if not str(t).startswith("_")]
except Exception:
    result["table_counts"] = []

# ================= 筛选预聚合（区域/站点维度，供 /api/snapshot 筛选秒回）=================
# 设计：把"区域/站点维度"的统计在重建时一次性算好（GROUP BY 行，量小），
#       网关收到筛选参数只做内存行选择+聚合，不再逐条起 spark。
# 注意：charger/order 与 station 是 1:1 join（无放大），可安全 join；避免 order×charger 多对多放大。

# 区域维度
result["region_charger"] = rows("select st.area as area, count(*) as cnt, sum(case when ch.status in ('idle','charging') then 1 else 0 end) as online from chargestation.charger ch join chargestation.station st on ch.station_id=st.id group by st.area")
result["region_order"] = rows("select st.area as area, count(*) as cnt, round(sum(o.energy_kwh),1) as kwh, round(sum(o.pay_amount),2) as inc from chargestation.charging_order o join chargestation.station st on o.station_id=st.id group by st.area")
result["region_status"] = rows("select st.area as area, ch.status as s, count(*) as c from chargestation.charger ch join chargestation.station st on ch.station_id=st.id group by st.area, ch.status")
result["region_trend"] = rows("select st.area as area, substr(o.create_time,1,10) as d, count(*) as c, round(sum(o.energy_kwh),1) as kwh, round(sum(o.pay_amount),2) as inc from chargestation.charging_order o join chargestation.station st on o.station_id=st.id where substr(o.create_time,1,10) >= date_format(date_sub(current_date, 30), 'yyyy-MM-dd') group by st.area, substr(o.create_time,1,10)")
result["region_peakvalley"] = rows("select st.area as area, cast(substr(o.start_time,12,2) as int) as h, round(sum(o.energy_kwh),1) as kwh from chargestation.charging_order o join chargestation.station st on o.station_id=st.id group by st.area, cast(substr(o.start_time,12,2) as int)")

# 站点维度（单表 group by，无 join）
result["station_charger"] = rows("select station_id, count(*) as cnt, sum(case when status in ('idle','charging') then 1 else 0 end) as online from chargestation.charger group by station_id")
result["station_order"] = rows("select station_id, count(*) as cnt, round(sum(energy_kwh),1) as kwh, round(sum(pay_amount),2) as inc from chargestation.charging_order group by station_id")
result["station_status"] = rows("select station_id, status as s, count(*) as c from chargestation.charger group by station_id, status")
result["station_trend"] = rows("select station_id, substr(create_time,1,10) as d, count(*) as c, round(sum(energy_kwh),1) as kwh, round(sum(pay_amount),2) as inc from chargestation.charging_order where substr(create_time,1,10) >= date_format(date_sub(current_date, 30), 'yyyy-MM-dd') group by station_id, substr(create_time,1,10)")
result["station_peakvalley"] = rows("select station_id, cast(substr(start_time,12,2) as int) as h, round(sum(energy_kwh),1) as kwh from chargestation.charging_order group by station_id, cast(substr(start_time,12,2) as int)")

# ================= 机器学习面板预聚合（需求热力 / 健康度 / WhatIf 基线）=================
# 每个区段独立 try，单段失败（如字段缺失）不影响整包重建
# 需求热力：星期(1=周日..7=周六)×小时×区域 有效订单数
try:
    result["demand_heat"] = rows("select dayofweek(o.start_time) as dow, cast(substr(o.start_time,12,2) as int) as h, st.area as area, count(*) as c from chargestation.charging_order o join chargestation.station st on o.station_id=st.id where o.status='completed' group by dayofweek(o.start_time), cast(substr(o.start_time,12,2) as int), st.area")
except Exception:
    result["demand_heat"] = []
# 健康度：全桩基础数据（健康分/温度/通信状态）
try:
    result["health_charger"] = rows("select st.name as sname, ch.code as code, ch.health_score as h, ch.temperature as t, ch.comm_status as cm from chargestation.charger ch join chargestation.station st on ch.station_id=st.id")
except Exception:
    result["health_charger"] = []
# WhatIf 基线：平均客单价/平均电量/总订单 + 高峰小时 + 排队数 + 桩数
try:
    result["ml_agg"] = rows("select round(avg(o.pay_amount),2), round(avg(o.energy_kwh),2), count(*) from chargestation.charging_order o where o.status='completed'")
except Exception:
    result["ml_agg"] = []
try:
    result["ml_peak"] = rows("select cast(substr(start_time,12,2) as int) as h, count(*) as c from chargestation.charging_order where status='completed' group by cast(substr(start_time,12,2) as int) order by c desc limit 1")
except Exception:
    result["ml_peak"] = []
try:
    result["ml_queue"] = rows("select count(*) from chargestation.reservation where status='waiting'")
except Exception:
    result["ml_queue"] = []

with open(OUT, "w", encoding="utf-8") as f:
    json.dump(result, f, ensure_ascii=False)
print("DASH_ENGINE_OK", OUT, "region=%s sid=%s date=%s" % (REGION, SID, DATE))
spark.stop()