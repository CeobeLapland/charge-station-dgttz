CREATE TABLE IF NOT EXISTS chargestation.review (
  id STRING,
  user_id STRING,
  station_id STRING,
  order_id STRING,
  overall_score STRING,
  speed_score STRING,
  device_score STRING,
  parking_score STRING,
  hygiene_score STRING,
  service_score STRING,
  tags STRING,
  content STRING,
  useful_count STRING,
  status STRING,
  create_time STRING
)
ROW FORMAT DELIMITED FIELDS TERMINATED BY '\t'
NULL DEFINED AS '\\N'
STORED AS TEXTFILE;
