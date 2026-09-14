CREATE TABLE IF NOT EXISTS `chargestation`.`device_log` (
  `id` STRING,
  `charger_id` STRING,
  `action` STRING,
  `operator` STRING,
  `op_time` STRING,
  `result` STRING
)
ROW FORMAT DELIMITED FIELDS TERMINATED BY '\t'
NULL DEFINED AS '\\N'
STORED AS TEXTFILE;
