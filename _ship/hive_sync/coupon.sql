CREATE TABLE IF NOT EXISTS chargestation.coupon (
  id STRING,
  title STRING,
  type STRING,
  discount_amount STRING,
  min_amount STRING,
  station_id STRING,
  time_range STRING,
  valid_days STRING,
  total STRING,
  status STRING,
  create_time STRING
)
ROW FORMAT DELIMITED FIELDS TERMINATED BY '\t'
NULL DEFINED AS '\\N'
STORED AS TEXTFILE;
