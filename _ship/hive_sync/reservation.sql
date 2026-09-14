CREATE TABLE IF NOT EXISTS chargestation.reservation (
  id STRING,
  user_id STRING,
  station_id STRING,
  charger_id STRING,
  queue_no STRING,
  reserve_time STRING,
  estimate_start_time STRING,
  notified STRING,
  status STRING
)
ROW FORMAT DELIMITED FIELDS TERMINATED BY '\t'
NULL DEFINED AS '\\N'
STORED AS TEXTFILE;
