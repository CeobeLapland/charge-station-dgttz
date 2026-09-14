CREATE TABLE IF NOT EXISTS `chargestation`.`faq` (
  `id` STRING,
  `category` STRING,
  `question` STRING,
  `answer` STRING,
  `sort` STRING,
  `enabled` STRING,
  `create_time` STRING
)
ROW FORMAT DELIMITED FIELDS TERMINATED BY '\t'
NULL DEFINED AS '\\N'
STORED AS TEXTFILE;
