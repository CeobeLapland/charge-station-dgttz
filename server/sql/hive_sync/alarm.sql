CREATE TABLE IF NOT EXISTS `chargestation`.`alarm` (
  `id` STRING,
  `charger_id` STRING,
  `station_id` STRING,
  `type` STRING,
  `level` STRING,
  `occur_time` STRING,
  `status` STRING,
  `handle_action` STRING
)
ROW FORMAT DELIMITED FIELDS TERMINATED BY '\t'
NULL DEFINED AS '\\N'
STORED AS TEXTFILE;
