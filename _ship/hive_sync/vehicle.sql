CREATE TABLE IF NOT EXISTS chargestation.vehicle (
  id STRING,
  user_id STRING,
  name STRING,
  type STRING,
  battery_kwh STRING,
  connector_type STRING,
  max_power_kw STRING,
  is_default STRING,
  created_time STRING
)
ROW FORMAT DELIMITED FIELDS TERMINATED BY '\t'
NULL DEFINED AS '\\N'
STORED AS TEXTFILE;
