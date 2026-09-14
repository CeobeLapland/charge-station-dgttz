CREATE TABLE IF NOT EXISTS chargestation.charging_measure (
  id STRING,
  charger_id STRING,
  station_id STRING,
  measure_time STRING,
  power_kw STRING,
  soc STRING,
  energy_delta_kwh STRING,
  temperature STRING
)
ROW FORMAT DELIMITED FIELDS TERMINATED BY '\t'
NULL DEFINED AS '\\N'
STORED AS TEXTFILE;
