# -*- coding: utf-8 -*-
"""
dash_engine.py — 用 SparkSession 一次性聚合大屏全量数据，输出 JSON 缓存。
由网关在启动时/后台调用；`/api/snapshot` 读取该 JSON，秒级返回。
用法: python3 dash_engine.py <输出json路径>
"""
import sys
import json

OUT = sys.argv[1] if len(sys.argv) > 1 else "/home/hadoop/screen/dash_cache.json"

from pyspark.sql import SparkSession

spark = SparkSession.builder \
    .appName("dash_engine") \
    .master("local[2]") \
    .config("spark.sql.catalogImplementation", "hive") \
    .enableHiveSupport() \
    .getOrCreate()

def rows(sql):
    return [[str(c) if c is not None else "" for c in r] for r in spark.sql(sql).collect()]

result = {}
result["status"] = rows("select status as s, count(*) as c from chargestation.charger group by status")
result["charger_total"] = rows("select count(*) as c from chargestation.charger")
result["station_total"] = rows("select count(*) as c from chargestation.station")
result["orders"] = rows("select count(*) as cnt, round(sum(energy_kwh),1) as kwh, round(sum(pay_amount),2) as inc from chargestation.charging_order")
result["trend"] = rows("select substr(create_time,1,10) as d, count(*) as c, round(sum(energy_kwh),1) as kwh, round(sum(pay_amount),2) as inc from chargestation.charging_order group by substr(create_time,1,10) order by d desc limit 7")
result["rank"] = rows("select s.id as sid, s.name as snm, count(o.id) as c, round(sum(o.energy_kwh),1) as kwh from chargestation.charging_order o join chargestation.station s on o.station_id=s.id group by s.id, s.name order by c desc limit 5")
result["stations"] = rows("select id, name, area, longitude, latitude from chargestation.station")
result["alarms"] = rows("select id, station_id, level, occur_time from chargestation.alarm order by occur_time desc limit 10")
result["user_trend"] = rows("select substr(register_time,1,10) as d, count(*) as c from chargestation.user group by substr(register_time,1,10) order by d desc limit 7")
result["measure_hour"] = rows("select cast(substr(measure_time,12,2) as int) as h, round(sum(power_kw),1) as tot, count(*) as c from chargestation.charging_measure group by cast(substr(measure_time,12,2) as int)")
# 峰谷(订单分时) + 总电量 + 今日 + 事件
result["peakvalley"] = rows("select cast(substr(start_time,12,2) as int) as h, round(sum(energy_kwh),1) as kwh from chargestation.charging_order group by cast(substr(start_time,12,2) as int)")
result["total_energy"] = rows("select round(sum(energy_kwh),1) as e from chargestation.charging_order")
result["today"] = rows("select count(*) as cnt, round(sum(energy_kwh),1) as kwh, round(sum(pay_amount),2) as inc from chargestation.charging_order where create_time like '2026-09-10%'")
result["events"] = rows("select id, create_time, user_id, station_id from chargestation.charging_order order by create_time desc limit 8")

with open(OUT, "w", encoding="utf-8") as f:
    json.dump(result, f, ensure_ascii=False)
print("DASH_ENGINE_OK", OUT)
spark.stop()