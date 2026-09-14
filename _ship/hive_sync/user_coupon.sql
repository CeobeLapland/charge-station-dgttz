CREATE TABLE IF NOT EXISTS chargestation.user_coupon (
  id STRING,
  user_id STRING,
  coupon_id STRING,
  status STRING,
  receive_time STRING,
  used_order_id STRING
)
ROW FORMAT DELIMITED FIELDS TERMINATED BY '\t'
NULL DEFINED AS '\\N'
STORED AS TEXTFILE;
