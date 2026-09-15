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

with open(OUT, "w", encoding="utf-8") as f:
    json.dump(result, f, ensure_ascii=False)
print("DASH_ENGINE_OK", OUT, "region=%s sid=%s date=%s" % (REGION, SID, DATE))
spark.stop()