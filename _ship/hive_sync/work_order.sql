CREATE TABLE IF NOT EXISTS chargestation.work_order (
  id STRING,
  type STRING,
  priority STRING,
  user_id STRING,
  station_id STRING,
  charger_id STRING,
  title STRING,
  description STRING,
  status STRING,
  handler STRING,
  result STRING,
  create_time STRING,
  handle_time STRING
)
ROW FORMAT DELIMITED FIELDS TERMINATED BY '\t'
NULL DEFINED AS '\\N'
STORED AS TEXTFILE;
