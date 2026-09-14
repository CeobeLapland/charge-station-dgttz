CREATE TABLE IF NOT EXISTS chargestation.charging_order (
  id STRING,
  user_id STRING,
  station_id STRING,
  charger_id STRING,
  vehicle_id STRING,
  status STRING,
  start_soc STRING,
  target_soc STRING,
  end_soc STRING,
  start_time STRING,
  end_time STRING,
  duration_min STRING,
  energy_kwh STRING,
  price_level STRING,
  amount STRING,
  discount_amount STRING,
  pay_amount STRING,
  points_used STRING,
  coupon_id STRING,
  points_earned STRING,
  create_time STRING,
  settle_time STRING
)
ROW FORMAT DELIMITED FIELDS TERMINATED BY '\t'
NULL DEFINED AS '\\N'
STORED AS TEXTFILE;
