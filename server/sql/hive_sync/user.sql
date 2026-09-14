CREATE TABLE IF NOT EXISTS `chargestation`.`user` (
  `id` STRING,
  `phone` STRING,
  `nickname` STRING,
  `avatar_path` STRING,
  `balance` STRING,
  `points` STRING,
  `level` STRING,
  `status` STRING,
  `register_time` STRING,
  `last_login_time` STRING
)
ROW FORMAT DELIMITED FIELDS TERMINATED BY '\t'
NULL DEFINED AS '\\N'
STORED AS TEXTFILE;
