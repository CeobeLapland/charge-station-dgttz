CREATE TABLE IF NOT EXISTS `chargestation`.`post_reply` (
  `id` STRING,
  `post_id` STRING,
  `user_id` STRING,
  `parent_id` STRING,
  `content` STRING,
  `status` STRING,
  `create_time` STRING
)
ROW FORMAT DELIMITED FIELDS TERMINATED BY '\t'
NULL DEFINED AS '\\N'
STORED AS TEXTFILE;
