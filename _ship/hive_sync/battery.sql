CREATE TABLE IF NOT EXISTS chargestation.battery (
  id STRING,
  code STRING,
  station_id STRING,
  soc STRING,
  health_score STRING,
  temperature STRING,
  status STRING,
  swap_count STRING,
  create_time STRING
)
ROW FORMAT DELIMITED FIELDS TERMINATED BY '\t'
NULL DEFINED AS '\\N'
STORED AS TEXTFILE;
