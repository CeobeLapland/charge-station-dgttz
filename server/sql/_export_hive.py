import sqlite3
import json
import os
import io

SRC = "charge.db"
OUT = "hive_sync"
os.makedirs(OUT, exist_ok=True)

conn = sqlite3.connect(SRC)
conn.row_factory = sqlite3.Row

# 需要落 Hive 的表（全量），顺序无所谓
tables = [
    "admin", "alarm", "battery", "charger", "charging_measure",
    "charging_order", "coupon", "device_log", "faq", "favorite",
    "holiday", "member_plan", "merchant", "notification", "order_timeline",
    "point_record", "post_reply", "price_rule", "reservation", "review",
    "rule_config", "station", "station_post", "swap_order", "user",
    "user_coupon", "user_plan", "vehicle", "wallet_transaction",
    "weather", "work_order",
]

def tsv_str(v):
    if v is None:
        return "\\N"
    if isinstance(v, (int, float)):
        return str(v)
    s = str(v)
    # 转义 TSV 特殊字符
    s = s.replace("\\", "\\\\").replace("\t", " ").replace("\n", " ").replace("\r", " ")
    return s

for t in tables:
    cols = [r[1] for r in conn.execute(f"pragma table_info({t})")]
    rows = conn.execute(f"select * from {t}").fetchall()
    # 库名/表名也加反引号，规避 user 等保留字
    full_name = f"`chargestation`.`{t}`"
    # 列名统一加反引号，规避 Hive 保留字（type/date/current/status/value 等）
    cols_def = ",\n  ".join(f"`{c}` STRING" for c in cols)
    ddl = f"""CREATE TABLE IF NOT EXISTS {full_name} (
  {cols_def}
)
ROW FORMAT DELIMITED FIELDS TERMINATED BY '\\t'
NULL DEFINED AS '\\\\N'
STORED AS TEXTFILE;
"""
    with open(os.path.join(OUT, f"{t}.sql"), "w", encoding="utf-8") as f:
        f.write(ddl)
    with open(os.path.join(OUT, f"{t}.tsv"), "w", encoding="utf-8", newline="") as f:
        for r in rows:
            f.write("\t".join(tsv_str(r[c]) for c in cols) + "\n")
    print(f"OK {t}: {len(rows)} rows -> {t}.tsv + {t}.sql")

total = sum(conn.execute(f"select count(*) from {t}").fetchone()[0] for t in tables)
print(f"\nTOTAL rows: {total}")
print(f"输出目录: {os.path.abspath(OUT)}")