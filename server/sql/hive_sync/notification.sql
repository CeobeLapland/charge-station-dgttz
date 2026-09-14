CREATE TABLE IF NOT EXISTS `chargestation`.`notification` (
  `id` STRING,
  `user_id` STRING,
  `type` STRING,
  `title` STRING,
  `content` STRING,
  `related_id` STRING,
  `is_read` STRING,
  `create_time` STRING
)
ROW FORMAT DELIMITED FIELDS TERMINATED BY '\t'
NULL DEFINED AS '\\N'
STORED AS TEXTFILE;
