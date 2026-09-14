CREATE TABLE IF NOT EXISTS `chargestation`.`weather` (
  `id` STRING,
  `area` STRING,
  `condition` STRING,
  `temperature` STRING,
  `forecast` STRING,
  `update_time` STRING
)
ROW FORMAT DELIMITED FIELDS TERMINATED BY '\t'
NULL DEFINED AS '\\N'
STORED AS TEXTFILE;
