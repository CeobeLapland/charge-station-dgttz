CREATE TABLE IF NOT EXISTS `chargestation`.`charger` (
  `id` STRING,
  `code` STRING,
  `station_id` STRING,
  `type` STRING,
  `power` STRING,
  `status` STRING,
  `voltage` STRING,
  `current` STRING,
  `temperature` STRING,
  `fault_code` STRING,
  `comm_status` STRING,
  `health_score` STRING,
  `total_charge_count` STRING,
  `total_charge_duration` STRING,
  `created_time` STRING
)
ROW FORMAT DELIMITED FIELDS TERMINATED BY '\t'
NULL DEFINED AS '\\N'
STORED AS TEXTFILE;
