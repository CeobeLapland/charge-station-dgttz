CREATE TABLE IF NOT EXISTS chargestation.wallet_transaction (
  id STRING,
  user_id STRING,
  type STRING,
  amount STRING,
  balance_after STRING,
  order_id STRING,
  remark STRING,
  create_time STRING
)
ROW FORMAT DELIMITED FIELDS TERMINATED BY '\t'
NULL DEFINED AS '\\N'
STORED AS TEXTFILE;
