CREATE TABLE IF NOT EXISTS `chargestation`.`merchant` (
  `id` STRING,
  `name` STRING,
  `contact_name` STRING,
  `contact_phone` STRING,
  `cooperation_type` STRING,
  `status` STRING,
  `order_count` STRING,
  `settle_amount` STRING,
  `service_score` STRING,
  `remark` STRING,
  `create_time` STRING
)
ROW FORMAT DELIMITED FIELDS TERMINATED BY '\t'
NULL DEFINED AS '\\N'
STORED AS TEXTFILE;
