CREATE TABLE IF NOT EXISTS `chargestation`.`station` (
  `id` STRING,
  `name` STRING,
  `address` STRING,
  `area` STRING,
  `longitude` STRING,
  `latitude` STRING,
  `total_chargers` STRING,
  `online_rate` STRING,
  `service_fee` STRING,
  `parking_fee` STRING,
  `business_hours` STRING,
  `facilities` STRING,
  `owner_type` STRING,
  `merchant_id` STRING,
  `has_swap` STRING,
  `status` STRING
)
ROW FORMAT DELIMITED FIELDS TERMINATED BY '\t'
NULL DEFINED AS '\\N'
STORED AS TEXTFILE;
