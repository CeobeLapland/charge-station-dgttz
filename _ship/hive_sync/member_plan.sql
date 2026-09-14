CREATE TABLE IF NOT EXISTS chargestation.member_plan (
  id STRING,
  name STRING,
  price STRING,
  valid_days STRING,
  service_fee_discount STRING,
  night_discount STRING,
  points_multiplier STRING,
  status STRING,
  description STRING,
  create_time STRING
)
ROW FORMAT DELIMITED FIELDS TERMINATED BY '\t'
NULL DEFINED AS '\\N'
STORED AS TEXTFILE;
