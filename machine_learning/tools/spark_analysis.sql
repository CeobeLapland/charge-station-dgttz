-- ============================================================
-- Spark-SQL 数据清洗 + 多维度分析
-- 数据库: chargestation (Hive 表)
-- 满足: 数据清洗 + >=8 分析维度 + >=2 组数据对比
-- 运行: spark-sql -f spark_analysis.sql  (或 --master local[2])
-- ============================================================

-- 使用库
use chargestation;

-- ---------- 1) 数据清洗 ----------
-- 临时视图：清洗掉电量<=0、关键字段为空、日期非法的订单
create or replace temporary view clean_order as
select *
from charging_order
where energy_kwh is not null
  and energy_kwh > 0
  and pay_amount is not null
  and station_id is not null
  and create_time is not null
  and create_time like '20%';

-- 清洗后对比
select '客单前' as dim, count(*) as val from charging_order
union all
select '客单后' as dim, count(*) as val from clean_order;

-- ---------- 2) 维度1: 区域分布 ----------
select st.area as region, count(distinct st.id) as stations,
       round(sum(co.energy_kwh),1) as kwh
from clean_order co
join station st on co.station_id=st.id
group by st.area order by kwh desc limit 8;

-- ---------- 3) 维度2: 电桩类型 ----------
select ch.type as type, count(*) as cnt,
       round(avg(ch.power),1) as avg_power,
       round(avg(ch.health_score),1) as avg_health
from charger ch
group by ch.type;

-- ---------- 4) 维度3: 订单状态 ----------
select status as status, count(*) as cnt
from charging_order group by status;

-- ---------- 5) 维度4: 按日期 ----------
select substr(create_time,1,10) as date,
       count(*) as orders,
       round(sum(energy_kwh),1) as kwh,
       round(sum(pay_amount),2) as income
from clean_order group by substr(create_time,1,10)
order by date desc limit 10;

-- ---------- 6) 维度5: 按小时(峰谷) ----------
select cast(substr(start_time,12,2) as int) as hour,
       count(*) as orders,
       round(sum(energy_kwh),1) as kwh
from clean_order group by cast(substr(start_time,12,2) as int)
order by hour;

-- ---------- 7) 维度6: 站点订单量TOP ----------
select st.name as station, count(*) as orders,
       round(sum(co.energy_kwh),1) as kwh
from clean_order co join station st on co.station_id=st.id
group by st.name order by orders desc limit 6;

-- ---------- 8) 维度7: 用户等级 ----------
select level as level, count(*) as cnt,
       round(avg(balance),2) as avg_balance
from user group by level;

-- ---------- 9) 维度8: 电价档位收入 ----------
select price_level as price_level, count(*) as orders,
       round(sum(pay_amount),2) as income
from clean_order group by price_level order by income desc;

-- ---------- 10) 对比1: 峰谷时段电量对比 ----------
select
  case
    when cast(substr(start_time,12,2) as int) between 0 and 8 then 'valley'
    when cast(substr(start_time,12,2) as int) between 17 and 21 then 'peak'
    else 'flat'
  end as period,
  count(*) as orders,
  round(sum(energy_kwh),1) as kwh,
  round(sum(pay_amount),2) as income
from clean_order
group by
  case
    when cast(substr(start_time,12,2) as int) between 0 and 8 then 'valley'
    when cast(substr(start_time,12,2) as int) between 17 and 21 then 'peak'
    else 'flat'
  end;

-- ---------- 11) 对比2: 快充 vs 慢充 效率对比 ----------
select ch.type as type,
       count(distinct co.id) as orders,
       round(avg(co.energy_kwh),1) as avg_kwh,
       round(sum(co.pay_amount),2) as total_income
from clean_order co
left join charger ch on co.charger_id=ch.id
group by ch.type;