#!/bin/bash
# ==========================================================
# 一键把 hive_sync/ 里的表灌入 Hive（分两步：建库建表、LOAD数据）
# 用法: bash load_hive.sh
# ==========================================================
set -e
# hive_sync 数据源目录：默认放 $HOME/hive_sync，可用环境变量覆盖
SYNC_DIR="${HIVE_SYNC_DIR:-$HOME/hive_sync}"
BEELINE="$(ls ~/.local/bin/beeline 2>/dev/null || command -v beeline)"
HS2="jdbc:hive2://node100:10000"
USER="hadoop"

echo "==> 使用 beeline: $BEELINE"
echo "==> hive_sync 目录: $SYNC_DIR"

# 1) 建库
echo "==> 建库 chargestation"
"$BEELINE" -u "$HS2" -n "$USER" --silent=true -e "create database if not exists chargestation;"

# 2) 建所有表
echo "==> 建表"
for f in "$SYNC_DIR"/*.sql; do
  table="$(basename "$f" .sql)"
  echo "    建表 $table"
  "$BEELINE" -u "$HS2" -n "$USER" --silent=true -f "$f" >/dev/null 2>&1 || echo "    [警告] $table 建表失败，继续"
done

# 3) LOAD 数据（放到 HDFS 表对应目录）
echo "==> 灌数据（大表 charging_measure 会慢，属正常）"
for f in "$SYNC_DIR"/*.tsv; do
  table="$(basename "$f" .tsv)"
  [ -s "$f" ] || continue
  echo "    LOAD $table ..."
  # 直接 load data local 到表（务必用绝对路径，否则 beeline 相对自身 cwd 解析导致找不到文件）
  ABS_F="$(cd "$(dirname "$f")" && pwd)/$(basename "$f")"
  "$BEELINE" -u "$HS2" -n "$USER" --silent=true \
    -e "load data local inpath '$ABS_F' into table chargestation.$table;" \
    >/dev/null 2>&1 && echo "    [OK] $table" || echo "    [警告] $table LOAD 失败"
done

# 4) 汇总统计
echo "==> 最终校验：每表行数"
ALL=""
for f in "$SYNC_DIR"/*.tsv; do
  table="$(basename "$f" .tsv)"
  [ -s "$f" ] || continue
  ALL="$ALL;select '$table' as t,count(*) from chargestation.$table;"
done
"$BEELINE" -u "$HS2" -n "$USER" --silent=true --outputformat=tsv2 -e "$ALL" 2>/dev/null | grep -v '^$' | head -200

echo ""
echo "==== 完成。到浏览器开 http://<虚拟机IP>:8080/hadoop 测试查询 ===="