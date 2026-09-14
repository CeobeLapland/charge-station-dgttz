CREATE TABLE IF NOT EXISTS chargestation.user_plan (
  id STRING,
  user_id STRING,
  plan_id STRING,
  start_time STRING,
  end_time STRING,
  status STRING,
  create_time STRING
)
ROW FORMAT DELIMITED FIELDS TERMINATED BY '\t'
NULL DEFINED AS '\\N'
STORED AS TEXTFILE;
