CREATE TABLE IF NOT EXISTS `chargestation`.`holiday` (
  `id` STRING,
  `date` STRING,
  `name` STRING,
  `is_workday` STRING,
  `create_time` STRING
)
ROW FORMAT DELIMITED FIELDS TERMINATED BY '\t'
NULL DEFINED AS '\\N'
STORED AS TEXTFILE;
