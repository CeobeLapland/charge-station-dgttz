CREATE TABLE IF NOT EXISTS chargestation.rule_config (
  id STRING,
  name STRING,
  object_type STRING,
  condition STRING,
  action STRING,
  action_params STRING,
  enabled STRING,
  priority STRING,
  description STRING,
  create_time STRING,
  update_time STRING
)
ROW FORMAT DELIMITED FIELDS TERMINATED BY '\t'
NULL DEFINED AS '\\N'
STORED AS TEXTFILE;
