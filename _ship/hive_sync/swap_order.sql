CREATE TABLE IF NOT EXISTS chargestation.swap_order (
  id STRING,
  user_id STRING,
  station_id STRING,
  battery_in_id STRING,
  battery_out_id STRING,
  amount STRING,
  status STRING,
  create_time STRING
)
ROW FORMAT DELIMITED FIELDS TERMINATED BY '\t'
NULL DEFINED AS '\\N'
STORED AS TEXTFILE;
