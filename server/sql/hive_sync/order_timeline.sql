CREATE TABLE IF NOT EXISTS `chargestation`.`order_timeline` (
  `id` STRING,
  `order_id` STRING,
  `node` STRING,
  `label` STRING,
  `event_time` STRING,
  `detail` STRING
)
ROW FORMAT DELIMITED FIELDS TERMINATED BY '\t'
NULL DEFINED AS '\\N'
STORED AS TEXTFILE;
