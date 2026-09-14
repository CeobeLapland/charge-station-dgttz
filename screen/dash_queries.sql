-- ============================================================
-- 大屏快照数据一次性聚合（由网关调 spark-sql -f 执行）
-- 每段用 --@SECTION:<name> 分隔，网关按 name 解析成 JSON
-- ============================================================
use chargestation;

--@SECTION:status
select status as s, count(*) as c from charger group by status;

--@SECTION:charger_total
select count(*) as c from charger;

--@SECTION:station_total
select count(*) as c from station;

--@SECTION:orders
select count(*) as cnt, round(sum(energy_kwh),1) as kwh, round(sum(pay_amount),2) as inc from charging_order;

--@SECTION:trend
select substr(create_time,1,10) as d, count(*) as c, round(sum(energy_kwh),1) as kwh, round(sum(pay_amount),2) as inc from charging_order group by substr(create_time,1,10) order by d desc limit 7;

--@SECTION:rank
select s.id as sid, s.name as snm, count(o.id) as c, round(sum(o.energy_kwh),1) as kwh from charging_order o join station s on o.station_id=s.id group by s.id, s.name order by c desc limit 5;

--@SECTION:stations
select id, name, area, longitude, latitude from station;

--@SECTION:alarms
select id, station_id, level, occur_time from alarm order by occur_time desc limit 10;

--@SECTION:user_trend
select substr(register_time,1,10) as d, count(*) as c from user where register_time >= '2026-09-04' group by substr(register_time,1,10) order by d;

--@SECTION:peakvalley
select cast(substr(start_time,12,2) as int) as h, round(sum(energy_kwh),1) as kwh from charging_order group by cast(substr(start_time,12,2) as int);

--@SECTION:total_energy
select round(sum(energy_kwh),1) as e from charging_order;

--@SECTION:events
select id, create_time, user_id, station_id from charging_order order by create_time desc limit 8;

--@SECTION:measure_hour
select cast(substr(measure_time,12,2) as int) as h, round(sum(power_kw),1) as tot, count(*) as c from charging_measure group by cast(substr(measure_time,12,2) as int);