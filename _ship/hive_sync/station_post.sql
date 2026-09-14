CREATE TABLE IF NOT EXISTS chargestation.station_post (
  id STRING,
  station_id STRING,
  user_id STRING,
  content STRING,
  like_count STRING,
  reply_count STRING,
  status STRING,
  create_time STRING
)
ROW FORMAT DELIMITED FIELDS TERMINATED BY '\t'
NULL DEFINED AS '\\N'
STORED AS TEXTFILE;
