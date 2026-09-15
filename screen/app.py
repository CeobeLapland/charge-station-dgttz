#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
充电站大数据演示网关（纯标准库，零依赖）
职责：
  1) 静态托管 screen/ 大屏（浏览器直接看，默认走大屏自带 mock 数据）
  2) /hadoop 查询演示页：把一句 Hive SQL 真正交给 HiveServer2 执行并回显结果
  3) /api/hive 接口：POST {"sql":"..."} 返回 Hive 执行结果 JSON（供页面前端调用）
用法：
  python3 app.py [端口] [screen目录]
  默认端口 8080，默认 screen 目录 ./screen
"""
import sys
import json
import os
import threading
import http.server
import subprocess
import urllib.parse

# ---------- 可调配置 ----------
HOST = "0.0.0.0"
PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
SCREEN_DIR = os.path.abspath(sys.argv[2]) if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "screen")
# HiveServer2 连接信息
HS2_URI = "jdbc:hive2://node100:10000"
HS2_USER = "hadoop"
BEELINE = os.path.expanduser("~/.local/bin/beeline")
# -----------------------------

def run_hive_sql(sql):
    """通过 beeline 执行一句 Hive SQL，返回 (ok, columns, rows, elapsed, err)"""
    if not sql or not sql.strip():
        return False, [], [], 0, "sql 为空"
    if not os.path.exists(BEELINE):
        return False, [], [], 0, f"找不到 beeline：{BEELINE}"
    cmd = [
        BEELINE, "-u", HS2_URI, "-n", HS2_USER,
        "--silent=true", "--outputformat=tsv2", "-e", sql,
    ]
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    except subprocess.TimeoutExpired:
        return False, [], [], 0, "执行超时（>120s）"
    out = p.stdout or ""
    if "Error:" in out or p.returncode != 0:
        # 提取错误行
        err = "\n".join(l for l in out.splitlines() if "Error:" in l or "Exception" in l)[:2000]
        return False, [], [], 0, (err or p.stderr or f"退出码 {p.returncode}")[:2000]
    # 解析 tsv2 格式：首行为列名，后续为数据
    lines = [l for l in out.strip().splitlines() if l.strip()]
    cols = []
    rows = []
    if lines:
        if lines[0].startswith("+"):  # 去掉分隔线
            lines = lines[1:]
        # tsv2 输出形如 "col1\tcol2" 数据行
        head_idx = 0
        while head_idx < len(lines) and not lines[head_idx].strip():
            head_idx += 1
        if head_idx < len(lines):
            cols = lines[head_idx].split("\t")
        data_lines = lines[head_idx+1:]
        for dl in data_lines:
            if dl.strip().startswith("+"):
                continue
            rows.append(dl.split("\t"))
    return True, cols, rows, 0, ""

SPARK_BIN = "/opt/module/spark-3.4.1/bin/spark-sql"

def run_spark_sql(sql, timeout=180):
    """通过 spark-sql 执行（用于演示 SPARK-SQL 分析），返回 (ok, cols, rows, err)"""
    if not os.path.exists(SPARK_BIN):
        return False, [], [], "找不到 spark-sql: " + SPARK_BIN
    cmd = [SPARK_BIN, "--master", "local[2]", "-e", sql]
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        return False, [], [], "spark 执行超时(>%ss)" % timeout
    out = (p.stdout or "") + "\n" + (p.stderr or "")
    if "Error:" in out and p.returncode != 0:
        err = "\n".join(l for l in out.splitlines() if "Error:" in l or "Exception" in l)[:1500]
        return False, [], [], (err or "spark 执行失败")
    # 解析 tsv 输出：过滤掉 spark 日志/警告行，第一行为列名，其余为数据
    lines = [l for l in out.splitlines() if l.strip()]
    clean = []
    for l in lines:
        if l.startswith("Time taken") or l in ("OK",) or l.startswith("Warning:") \
           or l.startswith("Spark master") or l.startswith("Application Id") \
           or l.startswith("log4j") or l.startswith("SLF4J") or l.startswith("Setting default") \
           or "To adjust logging level" in l or "WARN" in l or "ERROR" in l \
           or l.startswith("22/") or l.startswith("23/") or l.startswith("24/") or l.startswith("25/") or l.startswith("26/"):
            continue
        clean.append(l)
    cols, rows = [], []
    if clean:
        # 判断首行是表头还是数据：若第二列是纯数字，则首行其实是数据（spark -e 对单查询可能无表头）
        def _is_num(s):
            try:
                float(s); return True
            except Exception:
                return False
        if len(clean[0].split("\t")) >= 2 and _is_num(clean[0].split("\t")[-1]) and len(clean) > 1:
            rows = [r.split("\t") for r in clean if r.strip()]
        else:
            cols = clean[0].split("\t")
            rows = [r.split("\t") for r in clean[1:] if r.strip()]
    return True, cols, rows, ""

# ================= 开发者模式（/admin）辅助 =================
# 说明：所有管理员操作都走 beeline（HiveServer2），不做 SQL 注入面——
#       表名一律用 show tables 白名单，值一律转义单引号。

def _beeline_lines(sql, timeout=240):
    """执行一句 Hive SQL，返回过滤噪声后的原始行列表（用于 /admin 自己解析）"""
    if not os.path.exists(BEELINE):
        return []
    cmd = [BEELINE, "-u", HS2_URI, "-n", HS2_USER, "--silent=true", "--outputformat=tsv2", "-e", sql]
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    except Exception:
        return []
    out = []
    for l in (p.stdout or "").splitlines():
        s = l.strip()
        if not s:
            continue
        if s == "OK" or s.startswith("Time taken") or s.startswith("WARN") \
           or s.startswith("SLF4J") or s.startswith("log4j") or s.startswith("Setting default") \
           or "To adjust logging level" in s or s.startswith("Beeline version"):
            continue
        out.append(l.rstrip("\n"))
    return out

_ADMIN_TABLES = {"ts": 0.0, "names": []}

def _read_dash_cache():
    """读大屏预聚合缓存（dash_cache.json），读不到返回 {}"""
    try:
        p = os.path.join(SCREEN_DIR, "dash_cache.json")
        if os.path.exists(p):
            with open(p, encoding="utf-8") as f:
                return json.load(f)
    except Exception:
        pass
    return {}

def admin_table_names(force=False):
    """列出 chargestation 库下的业务表名（排除 _ 开头临时表），300s 缓存"""
    now = _time.time()
    if not force and _ADMIN_TABLES["names"] and now - _ADMIN_TABLES["ts"] < 300:
        return list(_ADMIN_TABLES["names"])
    names = []
    for l in _beeline_lines("show tables in chargestation;"):
        parts = l.split("\t")
        name = (parts[0] or "").strip()
        if name and name != "tab_name" and not name.startswith("_"):
            names.append(name)
    names.sort()
    _ADMIN_TABLES.update(ts=now, names=names)
    return names

def admin_table_cols(table):
    """返回表的列名列表（DESCRIBE）"""
    cols = []
    for l in _beeline_lines("describe chargestation.%s;" % table, timeout=120):
        parts = l.split("\t")
        name = (parts[0] or "").strip()
        if not name or name == "col_name" or name.startswith("#"):
            continue
        if len(parts) < 2 or not (parts[1] or "").strip():
            continue
        cols.append(name)
    return cols

_ADMIN_COUNTS = {"ts": 0.0, "counts": {}}

def admin_row_counts(tables):
    """多表行数。三级加速：
    1) dash_cache.json 的 table_counts（dash_engine 单 Spark 会话算好，秒回）
    2) spark-sql 单会话批量算（起一次 JVM，约 30~60s），内存缓存 10 分钟
    3) beeline 兜底（30 个 MR 串行，很慢，仅保证可用）
    """
    if not tables:
        return {}
    # 1) 预聚合缓存
    ce = _read_dash_cache()
    if ce.get("table_counts"):
        m = {}
        for r in ce["table_counts"]:
            try:
                m[str(r[0])] = int(r[1])
            except Exception:
                continue
        if m:
            return {t: m.get(t) for t in tables}
    # 2) spark 单会话批量 + 内存缓存
    now = _time.time()
    if _ADMIN_COUNTS["counts"] and now - _ADMIN_COUNTS["ts"] < 600:
        return {t: _ADMIN_COUNTS["counts"].get(t) for t in tables}
    sql = ";".join("select '%s' as t, count(*) as c from chargestation.%s" % (t, t) for t in tables)
    ok, _, rows, err = run_spark_sql(sql)
    out = {}
    if ok:
        for r in (rows or []):
            if len(r) >= 2:
                t, c = r[0].strip(), r[1].strip()
                if t in tables and c.isdigit():
                    out[t] = int(c)
        if out:
            _ADMIN_COUNTS.update(ts=now, counts=out)
            return out
    # 3) beeline 兜底
    sql = ";".join("select '%s' as t, count(*) as c from chargestation.%s" % (t, t) for t in tables)
    out = {}
    for l in _beeline_lines(sql, timeout=300):
        parts = l.split("\t")
        if len(parts) >= 2:
            t, c = parts[0].strip(), parts[1].strip()
            if t in tables and c.isdigit():
                out[t] = int(c)
    return out

def _esc_sql(v):
    """SQL 字符串字面量转义（单引号翻倍）"""
    return str(v).replace("'", "''")

def _parse_multipart(body, boundary):
    """解析 multipart/form-data 原始字节 -> (fields: dict, files: dict[filename->(name, bytes]))"""
    fields, files = {}, {}
    sep = ("--" + boundary).encode("utf-8")
    for part in body.split(sep):
        part = part.strip(b"\r\n")
        if not part or part == b"--":
            continue
        head, _, content = part.partition(b"\r\n\r\n")
        head_text = head.decode("utf-8", "ignore")
        name = filename = None
        for line in head_text.split("\r\n"):
            if line.lower().startswith("content-disposition"):
                for seg in line.split(";"):
                    seg = seg.strip()
                    if seg.startswith('name="'):
                        name = seg[6:-1]
                    elif seg.startswith("filename="):
                        filename = seg[9:].strip().strip('"')
        if name is None:
            continue
        if filename is not None:
            files[name] = (filename, content)
        else:
            fields[name] = content.decode("utf-8", "ignore")
    return fields, files

def _json_reply(handler, payload):
    body = json.dumps(payload, ensure_ascii=False)
    handler.send_response(200)
    handler.send_header("Content-Type", "application/json; charset=utf-8")
    handler.send_header("Cache-Control", "no-store")
    handler.end_headers()
    handler.wfile.write(body.encode("utf-8"))


INDEX_PAGE = """<!doctype html><html lang="zh-CN"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Hadoop / Hive 查询演示</title>
<style>
body{font-family:menlo,consolas,monospace;background:#0f1626;color:#cde3ff;margin:0;padding:24px}
h1{font-size:20px;color:#7dd3fc;border-bottom:1px solid #233}muted
pre{background:#0a0f1c;border:1px solid #233;padding:12px;border-radius:6px;overflow:auto;font-size:12px}
textarea{width:100%;height:60px;background:#0a0f1c;color:#cde3ff;border:1px solid #355;border-radius:6px;padding:8px;font-size:13px}
button{background:#1d4ed8;color:#fff;border:0;padding:8px 16px;border-radius:6px;cursor:pointer;font-size:13px;margin-top:8px}
button:hover{background:#1e40af}
#out{white-space:pre-wrap;background:#0a0f1c;border:1px solid #233;padding:12px;border-radius:6px;min-height:80px;margin-top:12px;font-size:12px}
.status{border:1px solid #123;background:#082032;padding:8px 12px;border-radius:6px;margin-bottom:12px;font-size:12px}
.q{margin:6px 0;padding:6px 10px;background:#101a30;border:1px solid #233;border-radius:6px;cursor:pointer;font-size:12px;display:inline-block;margin-right:8px}
.q:hover{border-color:#3b82f6}
a{color:#7dd3fc}
</style></head><body>
<h1>Big Data 演示 · Hadoop + Hive 查询网关</h1>
<div style="margin-bottom:8px;font-size:12px"><a href="/index.html">← 返回大屏</a> ｜ <a href="/admin">开发者模式</a></div>
<div class="status">HiveServer2: <b>{HS2_URI}</b> ｜ 用户: <b>{HS2_USER}</b></div>
<h3>常用演示查询（点击即执行，或自行输入 SQL）：</h3>
<div id="quick">
<span class="q" data-sql="show databases;">show databases;</span>
<span class="q" data-sql="select count(*) as 记录数 from chargestation.charging_measure;">测量表行数(海量数据)</span>
<span class="q" data-sql="select count(*) as 站点数 from chargestation.station;">站点总数</span>
<span class="q" data-sql="select count(*) as 桩数 from chargestation.charger;">充电桩总数</span>
<span class="q" data-sql="select status,count(*) as 数量 from chargestation.charger group by status;">按状态统计电桩</span>
<span class="q" data-sql="select s.name,count(*) as 订单数 from chargestation.charging_order o join chargestation.station s on o.station_id=s.id group by s.name order by 订单数 desc limit 5;">每站订单数TOP5</span>
<span class="q" data-sql="select date_format(create_time,'yyyy-MM-dd') as 日期,round(sum(pay_amount),2) as 收入 from chargestation.charging_order group by 日期 order by 日期 desc limit 10;">近10日收入</span>
</div>
<h3 style="margin-top:14px">★ SPARK-SQL 多维度大数据分析（Spark 引擎实时计算，首次较慢请稍候）：</h3>
<div id="spark-quick">
<span class="q spark" data-sql="select st.area as region, count(distinct st.id) as stations, round(sum(co.energy_kwh),1) as kwh from chargestation.charging_order co join chargestation.station st on co.station_id=st.id where co.create_time like '20%%' group by st.area order by kwh desc limit 8;">维度1·区域分布</span>
<span class="q spark" data-sql="select ch.type as type, count(*) as cnt, round(avg(ch.power),1) as avg_power from chargestation.charger ch group by ch.type;">维度2·电桩类型</span>
<span class="q spark" data-sql="select status,count(*) as cnt from chargestation.charging_order group by status;">维度3·订单状态</span>
<span class="q spark" data-sql="select cast(substr(start_time,12,2) as int) as h, round(sum(energy_kwh),1) as kwh from chargestation.charging_order group by cast(substr(start_time,12,2) as int) order by h;">维度4·分时电量</span>
<span class="q spark" data-sql="select st.name as station, count(*) as od from chargestation.charging_order o join chargestation.station st on o.station_id=st.id group by st.name order by od desc limit 6;">维度5·站点订单TOP</span>
<span class="q spark" data-sql="select level, count(*) as cnt, round(avg(balance),2) as avg_bal from chargestation.user group by level;">维度6·用户等级</span>
<span class="q spark" data-sql="select price_level, round(sum(pay_amount),2) as income from chargestation.charging_order group by price_level order by income desc;">维度7·电价档位收入</span>
<span class="q spark" data-sql="select case when cast(substr(start_time,12,2) as int) between 0 and 8 then 'valley' when cast(substr(start_time,12,2) as int) between 17 and 21 then 'peak' else 'flat' end as period, round(sum(energy_kwh),1) kwh, round(sum(pay_amount),2) income from chargestation.charging_order group by case when cast(substr(start_time,12,2) as int) between 0 and 8 then 'valley' when cast(substr(start_time,12,2) as int) between 17 and 21 then 'peak' else 'flat' end;">对比1·峰谷电量</span>
<span class="q spark" data-sql="select ch.type, round(avg(co.energy_kwh),1) avg_kwh, round(sum(co.pay_amount),2) tot from chargestation.charging_order co left join chargestation.charger ch on co.charger_id=ch.id group by ch.type;">对比2·快慢充效益</span>
</div>
<h3>自定义 SQL：</h3>
<textarea id="sql">select count(*) as 总记录数 from chargestation.charging_order;</textarea><br>
<button onclick="run()">执行</button>
<pre id="out">请点击上方快捷查询，或输入 SQL 后点“执行”。</pre>
<script>
var qs=document.querySelectorAll('.q');
qs.forEach(function(q){q.onclick=function(){document.getElementById('sql').value=q.getAttribute('data-sql');run();}});
function run(){
  var out=document.getElementById('out');
  var sql=document.getElementById('sql').value.trim();
  if(!sql){out.textContent='SQL 不能为空';return;}
  out.textContent='正在提交到引擎执行，请稍候…（Spark 首次启动较慢，约 10~30 秒）';
  fetch('/api/spark',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({sql:sql})})
    .then(function(r){return r.json();})
    .then(function(res){
      if(res.ok){
        var hasCols = res.cols && res.cols.length;
        if(hasCols||(res.rows&&res.rows.length)){out.textContent=(hasCols?('列: '+res.cols.join(' | ')+'\\n'):'')+res.rows.map(function(r){return r.join(' | ');}).join('\\n')+'\\n\\n共 '+res.rows.length+' 行（结果由 SPARK-SQL 引擎实时计算返回）';}
        else{out.textContent='(空结果) 已完成，执行成功';}
      }else{out.textContent='执行失败:\\n'+res.message;}
    })
    .catch(function(e){out.textContent='请求失败: '+e;});
}
function runHive(){
  var out=document.getElementById('out');
  var sql=document.getElementById('sql').value.trim();
  if(!sql){out.textContent='SQL 不能为空';return;}
  out.textContent='正在提交到 HiveServer2 执行，请稍候…';
  fetch('/api/hive',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({sql:sql})})
    .then(function(r){return r.json();})
    .then(function(res){
      if(res.ok){out.textContent=(res.cols?('列: '+res.cols.join(' | ')+'\\n'+res.rows.map(function(r){return r.join(' | ');}).join('\\n')+'\\n\\n共 '+res.rows.length+' 行 （Hive 返回）'):res.message);}
      else{out.textContent='执行失败:\\n'+res.message;}
    })
    .catch(function(e){out.textContent='请求失败: '+e;});
}
document.querySelector('#quick .q').addEventListener('click',null);
</script>
</body></html>"""

_CACHE = {}          # sql -> (timestamp, rows)
_CACHE_TTL = 120     # 秒：缓存窗口，避免大屏高频刷新反复起 spark
import time as _time
_SNAP_CACHE = []     # [timestamp, payload]  整体快照缓存，避免多条 spark 串行慢（旧）
_SNAP_CACHE2 = {}    # ckey -> [timestamp, payload]  按筛选组合缓存
_SNAP_TTL = 60       # 秒

def _q(sql):
    """执行查询，返回 (ok, rows[list[list]], err)。
    优先用 Spark-SQL（快、稳，绕开 Hive MR 慢问题）；带 2 分钟结果缓存。"""
    now = _time.time()
    hit = _CACHE.get(sql)
    if hit and now - hit[0] < _CACHE_TTL:
        return True, hit[1], ""
    ok, cols, rows, err = run_spark_sql(sql)
    if ok:
        _CACHE[sql] = (now, rows)
        return True, rows, ""
    # spark 失败则回落 beeline
    ok2, cols2, rows2, e2, err2 = run_hive_sql(sql) if False else (False, [], [], 0, "")
    try:
        ok2, _, rows2, _, err2 = run_hive_sql(sql)
    except Exception:
        ok2, rows2, err2 = False, [], "beeline失败"
    if ok2:
        return True, rows2, ""
    return False, [], (err or err2)

def _row0(rows):
    """返回第一行第一列或 None"""
    return rows[0][0] if rows else None

# ---- PME 周期性移动平均外推（Python 复刻自 machine_learning/README）----
import random as _rng
_FORECAST_NOISE = 0.05      # 乘性噪声 σ
_Z90 = 1.645                # 90% 置信区间
_GROWTH = 0.015             # 小时均值温和增长系数

def _pme_forecast(hour_stats):
    """
    hour_stats: dict { hour:int -> [total_kwh, count] } 全网负荷小时聚合
    返回: {hours:[0..23], actual:[近一段实际(小时均值)], forecast:[未来24h], lower, upper}
    用"小时同均值"作为模板 + 乘性噪声 + 置信区间（满足"早晚双峰+周期性外推"的可解释白盒）。
    """
    base = {}   # hour -> mean
    for h, (total, cnt) in hour_stats.items():
        if cnt:
            base[h] = total / cnt
    if not base:
        return None
    hours = list(range(24))
    # 预测：小时均值模板 × (1+增长) × 乘性噪声
    forecast, lower, upper = [], [], []
    for h in hours:
        m = base.get(h, sum(base.values()) / len(base))
        g = _GROWTH if h in (8, 9, 18, 19) else 0.0   # 早晚高峰略增
        val = m * (1 + g)
        sigma = abs(val) * _FORECAST_NOISE
        forecast.append(round(val, 1))
        lower.append(round(max(0, val - _Z90 * sigma), 1))
        upper.append(round(val + _Z90 * sigma, 1))
    actual = [round(base.get(h, 0), 1) for h in hours]
    return {"hours": hours, "actual": actual, "forecast": forecast, "lower": lower, "upper": upper}

def build_forecast():
    """从 Hive charging_measure 聚合小时负荷 -> PME 预测（优先读 dash_cache.json）"""
    _CE = {}
    try:
        _p = os.path.join(os.path.dirname(os.path.abspath(__file__)), "dash_cache.json")
        if os.path.exists(_p) and _time.time() - os.path.getmtime(_p) < 600:
            with open(_p, encoding="utf-8") as _f:
                _CE = json.load(_f)
    except Exception:
        _CE = {}
    if _CE.get("measure_hour"):
        rows = _CE["measure_hour"]
    else:
        _ok, rows, _ = _q("select cast(substr(measure_time,12,2) as int) as h, round(sum(power_kw),1) as tot, count(*) as c from `chargestation`.`charging_measure` group by cast(substr(measure_time,12,2) as int);")
    if not rows:
        return {"error": "无时序数据"}
    hour_stats = {}
    for r in rows:
        try:
            h, tot, c = int(r[0]), float(r[1] or 0), int(r[2] or 0)
            hour_stats[h] = [tot, c]
        except Exception:
            continue
    fc = _pme_forecast(hour_stats)
    if fc is None:
        return {"error": "预测失败"}
    # 简单模型评估：以"预测均值 vs 实际均值"的 MAE/相对误差作为评估量
    actual_mean = sum(fc["actual"]) / 24 if fc["actual"] else 0
    mape = round(sum(abs(a - f) / a for a, f in zip(fc["actual"], fc["forecast"]) if a) / 24 * 100, 2) if fc["actual"] else 0
    fc["mape"] = mape
    fc["model"] = "PME 周期性移动平均外推"
    return fc

def _build_filtered(ce, region, sid, date):
    """从 dash_cache 的区域/站点预聚合区段组装筛选快照（纯内存，秒回，零 spark）。
    区域/站点/日期可任意组合；返回结构与全量快照一致。"""
    st_rows = ce.get("stations") or []
    st_ch = {}   # station_id -> [count, online]
    st_od = {}   # station_id -> [orders, energy, revenue]
    for r in ce.get("station_charger") or []:
        try:
            st_ch[str(r[0])] = [int(r[1] or 0), int(r[2] or 0)]
        except Exception:
            continue
    for r in ce.get("station_order") or []:
        try:
            st_od[str(r[0])] = [int(r[1] or 0), float(r[2] or 0), float(r[3] or 0)]
        except Exception:
            continue
    # 站点作用域（区域/站点筛选）
    scope = []
    for r in st_rows:
        if region and (r[2] or "") != region:
            continue
        if sid and str(r[0]) != str(sid):
            continue
        scope.append(r)
    scope_ids = {str(r[0]) for r in scope}
    st_map = {str(r[0]): r[1] for r in scope}

    # 1) 指标卡
    station_count = len(scope)
    charger_count = sum(st_ch.get(str(r[0]), [0, 0])[0] for r in scope)
    online_charger_count = sum(st_ch.get(str(r[0]), [0, 0])[1] for r in scope)
    if date:
        today_orders = today_energy = today_revenue = 0
        for r in ce.get("station_trend") or []:
            if str(r[0]) not in scope_ids:
                continue
            if (r[1] or "") != date:
                continue
            today_orders += int(r[2] or 0)
            today_energy += float(r[3] or 0)
            today_revenue += float(r[4] or 0)
        today_energy = round(today_energy, 1)
        today_revenue = round(today_revenue, 2)
    else:
        today_orders = sum(st_od.get(str(r[0]), [0, 0, 0])[0] for r in scope)
        today_energy = round(sum(st_od.get(str(r[0]), [0, 0, 0])[1] for r in scope), 1)
        today_revenue = round(sum(st_od.get(str(r[0]), [0, 0, 0])[2] for r in scope), 2)

    # 2) 设备状态分布
    sd = {"idle": 0, "charging": 0, "offline": 0, "fault": 0}
    for r in ce.get("station_status") or []:
        if str(r[0]) not in scope_ids:
            continue
        s, n = (r[1] or "").strip(), int(r[2] or 0)
        if s == "idle": sd["idle"] += n
        elif s == "offline": sd["offline"] += n
        elif s == "fault": sd["fault"] += n
        else: sd["charging"] += n

    # 3) 趋势（scope 内按日期聚合；选了日期则只显示该日期）
    trend_map = {}
    for r in ce.get("station_trend") or []:
        if str(r[0]) not in scope_ids:
            continue
        d = r[1] or ""
        if not d:
            continue
        if date and d != date:
            continue
        t = trend_map.setdefault(d, [0, 0.0, 0.0])
        t[0] += int(r[2] or 0); t[1] += float(r[3] or 0); t[2] += float(r[4] or 0)
    order_trend, energy_revenue_trend = [], []
    for d in sorted(trend_map):
        t = trend_map[d]
        order_trend.append({"date": d, "order_count": t[0]})
        energy_revenue_trend.append({"date": d, "energy_kwh": round(t[1], 1), "revenue": round(t[2], 2)})
    order_trend = order_trend[-7:]
    energy_revenue_trend = energy_revenue_trend[-7:]

    # 4) 站点排行（scope 内按订单数 TOP5）
    rank_items = []
    for r in scope:
        o = st_od.get(str(r[0]), [0, 0, 0])
        rank_items.append({"station_id": r[0], "station_name": r[1], "today_orders": o[0], "today_energy_kwh": o[1]})
    rank_items.sort(key=lambda x: x["today_orders"], reverse=True)
    station_rank = rank_items[:5]

    # 5) 地图站点
    stations = []
    for r in scope:
        try:
            lon, lat = float(r[3] or 0), float(r[4] or 0)
        except Exception:
            lon = lat = 0
        stations.append({"id": r[0], "name": r[1], "region": r[2] or "", "longitude": lon, "latitude": lat, "status": "normal", "status_text": "正常运行"})

    # 6) 告警 / 事件（按 scope 站点过滤，日期可选）
    alarms = []
    for r in ce.get("alarms") or []:
        if str(r[1]) not in scope_ids:
            continue
        if date and not (r[3] or "").startswith(date):
            continue
        alarms.append({"id": r[0], "occur_time": r[3], "station_id": r[1], "station_name": "", "charger_id": None, "charger_code": "", "content": f"告警等级 {r[2]}", "level": r[2] if r[2] in ("info", "warning", "critical") else "warning"})
    events = []
    for r in ce.get("events") or []:
        if str(r[3]) not in scope_ids:
            continue
        if date and not (r[1] or "").startswith(date):
            continue
        uid = r[2]
        st = st_map.get(str(r[3]), "某站")
        events.append({"id": f"evt-o{r[0]}", "occur_time": r[1], "type": "order", "content": f"用户{uid} 在 {st} 完成充电"})
    for r in alarms:
        events.append({"id": f"evt-a{r['id']}", "occur_time": r["occur_time"], "type": "alarm", "content": f"站点发生{r['content']}告警"})
    events = events[:10]

    # 7) 峰谷电量
    pv = {"day": today_energy, "valley": 0.0, "flat": 0.0, "peak": 0.0}
    for r in ce.get("station_peakvalley") or []:
        if str(r[0]) not in scope_ids:
            continue
        try:
            hh, e = int(r[1]), float(r[2] or 0)
        except Exception:
            continue
        if 0 <= hh < 8: pv["valley"] += e
        elif 17 <= hh < 21: pv["peak"] += e
        else: pv["flat"] += e

    # 8) 碳排（沿用全局公式）
    FACTOR = 0.7
    today_reduce = round(today_energy * FACTOR / 1000.0, 2)
    total_reduce = round(today_energy * FACTOR / 1000.0, 2)
    valley_ratio = (pv["valley"] / pv["day"]) if pv["day"] else 0
    green_index = round(min(100, 60 + valley_ratio * 40), 1)
    carbon = {"today_energy_kwh": today_energy, "today_reduce_tons": today_reduce, "total_energy_kwh": today_energy, "total_reduce_tons": total_reduce, "green_index": green_index}

    # 9) 用户增长（平台级，保持全局）
    user_trend = [{"date": r[0], "user_count": int(r[1] or 0)} for r in (ce.get("user_trend") or [])]

    # 10) 筛选项（全局候选，便于切换）
    regions, st_opt = [], []
    reg_set = set()
    for r in ce.get("stations") or []:
        st_opt.append({"id": r[0], "name": r[1]})
        if len(r) > 2 and r[2]:
            reg_set.add(r[2])
    filter_options = {"regions": sorted(reg_set), "stations": st_opt}

    return {
        "metrics": {
            "station_count": station_count,
            "charger_count": charger_count,
            "online_charger_count": online_charger_count,
            "today_energy_kwh": today_energy,
            "today_orders": today_orders,
            "today_revenue": today_revenue,
        },
        "status_distribution": sd,
        "order_trend": order_trend,
        "energy_revenue_trend": energy_revenue_trend,
        "station_rank": station_rank,
        "stations": stations,
        "alarms": alarms,
        "events": events,
        "user_trend": user_trend,
        "peak_valley": pv,
        "carbon": carbon,
        "filter_options": filter_options,
    }

def build_ml():
    """从 dash_cache 组装机器学习面板数据（需求热力/健康度/WhatIf 基线），纯内存秒回。"""
    ce = _read_dash_cache()
    # ---- 需求热力：星期×小时×区域 需求 + 激增检测 ----
    heat, agg = [], {}
    for r in ce.get("demand_heat") or []:
        try:
            dow, hour, c = int(r[0]), int(r[1]), int(r[3] or 0)
        except Exception:
            continue
        area = r[2] or ""
        if dow < 1 or dow > 7 or hour < 0 or hour > 23:
            continue
        heat.append({"dow": dow, "hour": hour, "area": area, "demand": c})
        a = agg.setdefault((hour, area), [0, 0])
        a[0] += c; a[1] += 1
    mean = {k: v[0] / max(v[1], 1) for k, v in agg.items()}
    for item in heat:
        m = mean.get((item["hour"], item["area"]), 0)
        delta = (item["demand"] - m) / m if m else 0
        item["delta"] = round(delta, 2)
        item["surge"] = delta > 0.5
    surge = sorted([x for x in heat if x["surge"]], key=lambda x: x["delta"], reverse=True)[:5]

    # ---- 设备健康度：扣分制评分 + 风险等级 + 分布 + 高风险 TOP ----
    health = {"dist": [0, 0, 0, 0, 0], "risk": []}   # 分布桶:<60,60-70,70-80,80-90,90-100
    for r in ce.get("health_charger") or []:
        try:
            sname, code = r[0] or "", r[1] or ""
            h = int(r[2] if r[2] not in (None, "") else 100)
            t = float(r[3] or 0)
        except Exception:
            continue
        cm = (r[4] or "").strip()
        score = h
        if t > 50: score -= int(min(2 * (t - 50), 60))
        if cm == "abnormal": score -= 15
        score = max(0, min(100, score))
        risk = (100 - score) / 100.0 * 0.8
        level = "high" if risk >= 0.6 else ("medium" if risk >= 0.3 else "low")
        if score < 60: health["dist"][0] += 1
        elif score < 70: health["dist"][1] += 1
        elif score < 80: health["dist"][2] += 1
        elif score < 90: health["dist"][3] += 1
        else: health["dist"][4] += 1
        health["risk"].append({"station": sname, "code": code, "health": score, "level": level, "risk": round(risk, 2)})
    health["risk"].sort(key=lambda x: x["risk"], reverse=True)
    health["risk"] = health["risk"][:8]

    # ---- WhatIf 基线（前端滑条推演用）----
    base = {"avg_revenue_per_order": 0.0, "avg_energy_per_order": 0.0, "total_orders": 0,
            "peak_hour": 0, "peak_demand": 0, "queue": 0, "charger_count": 0,
            "daily_orders": 0, "daily_revenue": 0.0}
    m = ce.get("ml_agg")
    if m and m[0]:
        try:
            base["avg_revenue_per_order"] = float(m[0][0] or 0)
            base["avg_energy_per_order"] = float(m[0][1] or 0)
            base["total_orders"] = int(m[0][2] or 0)
        except Exception:
            pass
    pk = ce.get("ml_peak")
    if pk and pk[0]:
        try:
            base["peak_hour"] = int(pk[0][0] or 0)
            base["peak_demand"] = int(pk[0][1] or 0)
        except Exception:
            pass
    q = ce.get("ml_queue")
    if q and q[0]:
        try:
            base["queue"] = int(q[0][0] or 0)
        except Exception:
            pass
    ct = ce.get("charger_total")
    if ct and ct[0]:
        try:
            base["charger_count"] = int(ct[0][0] or 0)
        except Exception:
            pass
    trend = ce.get("trend") or []
    if trend:
        days = len(trend)
        try:
            base["daily_orders"] = int(round(sum(int(r[1] or 0) for r in trend) / max(days, 1)))
            base["daily_revenue"] = round(sum(float(r[3] or 0) for r in trend) / max(days, 1), 2)
        except Exception:
            pass
    return {"ok": True, "demand_heat": heat, "top_surge": surge, "health": health, "whatif": base}

def build_snapshot(region="", station_id="", date=""):
    """从 Hive 聚合出大屏需要的完整快照（与 screen.snapshot_resp.payload 同构）。
    优先读取 dash_engine.py 预生成的 dash_cache.json（单次spark会话产物），秒回；缺失再逐条查。
    region/station_id/date 非空时按筛选过滤（走实时 spark 聚合）。"""
    # ---- 生成筛选 WHERE（注入各查询）----
    WSPEC = ""          # 站点过滤，用于不带 join 的表(station/charger)
    WORDER = ""         # 订单过滤：join station 时的 station 条件
    WORDER_DIRECT = ""  # 订单过滤：直接 station_id 条件
    WDATE = ""          # 日期前缀过滤
    fs = []
    if region:
        WSPEC += f" and area = '{region}'"
        WORDER += f" and st.area = '{region}'"
    if station_id:
        WSPEC += f" and id = '{station_id}'"
        WORDER += f" and st.id = '{station_id}'"
        WORDER_DIRECT += f" and station_id = '{station_id}'"
    if date:
        WDATE = f" and create_time like '{date}%'"
    use_filter = bool(region or station_id or date)
    _SCREEN = os.path.dirname(os.path.abspath(__file__))
    _CE = {}
    # 读取全局预聚合缓存（dash_cache.json）。筛选实时重算因 spark 逐条启动过慢且不稳，
    # 统一回退用全局缓存，保证大屏稳定、秒回（筛选暂按全量展示）。
    try:
        _p = os.path.join(_SCREEN, "dash_cache.json")
        if os.path.exists(_p):
            with open(_p, encoding="utf-8") as _f:
                _CE = json.load(_f)
    except Exception:
        _CE = {}
    import sys as _sys
    _sys.stderr.write(f"[dash] cache_keys={list(_CE.keys())[:6]} use_filter={bool(region or station_id or date)}\n")
    # 筛选：若缓存含站点维度预聚合区段（dash_engine 新版产物），走内存组装，秒回
    if use_filter and _CE.get("station_order"):
        return _build_filtered(_CE, region, station_id, date)
    def _rc(key, i, default=""):
        """从缓存取第i行第0~col列"""
        try:
            return _CE[key][i]
        except Exception:
            return default

    # 1) 状态分布：从 cache[status]=[[s,c],...] 或回退 _q
    if _CE.get("status"):
        rows = _CE["status"]
        sd = {"idle": 0, "charging": 0, "offline": 0, "fault": 0}
        for r in rows:
            st, n = (r[0] or "").strip(), int(r[1] or 0)
            if st == "idle": sd["idle"] = n
            elif st == "offline": sd["offline"] = n
            elif st == "fault": sd["fault"] = n
            else: sd["charging"] += n
        charger_count = int(_rc("charger_total", 0, [0])[0] or 0)
        station_count = int(_rc("station_total", 0, [0])[0] or 0)
        online_charger_count = sd["idle"] + sd["charging"]
    else:
        # 回退：逐条 spark/beeline
        ok, rows, err = _q(f"select ch.status as s, count(*) as c from `chargestation`.`charger` ch left join `chargestation`.`station` st on ch.station_id=st.id where 1=1{WSPEC} group by ch.status;")
        sd = {"idle": 0, "charging": 0, "offline": 0, "fault": 0}
        for r in rows or []:
            st, n = (r[0] or "").strip(), int(r[1] or 0)
            if st == "idle": sd["idle"] = n
            elif st == "offline": sd["offline"] = n
            elif st == "fault": sd["fault"] = n
            else: sd["charging"] += n
        _ok, rows_c, _ = _q(f"select count(*) from `chargestation`.`charger` ch left join `chargestation`.`station` st on ch.station_id=st.id where 1=1{WSPEC};")
        charger_count = int(_row0(rows_c) or 0)
        online_charger_count = sd["idle"] + sd["charging"]
        _ok, rows_s, _ = _q(f"select count(*) from `chargestation`.`station` where 1=1{WSPEC};")
        station_count = int(_row0(rows_s) or 0)

    # 2) 订单指标（今日）
    year, month, day = "2026", "09", "10"
    if _CE.get("orders"):
        r = _CE["orders"][0]
        today_orders = int(r[0] or 0); today_energy = float(r[1] or 0); today_revenue = float(r[2] or 0)
    else:
        _ok, rows_t, _ = _q(f"select count(*), round(sum(energy_kwh),1), round(sum(pay_amount),2) from `chargestation`.`charging_order` o left join `chargestation`.`station` st on o.station_id=st.id where 1=1{WORDER_DIRECT or WORDER}{WDATE} and (o.status='completed' or o.status is not null);")
        today_orders = today_energy = today_revenue = 0
        if rows_t and rows_t[0]:
            today_orders = int(rows_t[0][0] or 0); today_energy = float(rows_t[0][1] or 0); today_revenue = float(rows_t[0][2] or 0)

    # 3) 趋势
    order_trend, energy_revenue_trend = [], []
    if _CE.get("trend"):
        for r in _CE["trend"]:
            order_trend.append({"date": r[0], "order_count": int(r[1] or 0)})
            energy_revenue_trend.append({"date": r[0], "energy_kwh": float(r[2] or 0), "revenue": float(r[3] or 0)})
        order_trend.reverse(); energy_revenue_trend.reverse()
    else:
        _ok, rows_trend, _ = _q(f"select substr(o.create_time,1,10) as d, count(*), round(sum(o.energy_kwh),1), round(sum(o.pay_amount),2) from `chargestation`.`charging_order` o left join `chargestation`.`station` st on o.station_id=st.id where 1=1{WORDER_DIRECT or WORDER}{WDATE} group by substr(o.create_time,1,10) order by d desc limit 7;")
        for r in (rows_trend or []):
            order_trend.append({"date": r[0], "order_count": int(r[1] or 0)})
            energy_revenue_trend.append({"date": r[0], "energy_kwh": float(r[2] or 0), "revenue": float(r[3] or 0)})
        order_trend.reverse(); energy_revenue_trend.reverse()

    # 4) 站点排行
    station_rank = []
    if _CE.get("rank"):
        for r in _CE["rank"]:
            station_rank.append({"station_id": r[0], "station_name": r[1], "today_orders": int(r[2] or 0), "today_energy_kwh": float(r[3] or 0)})
    else:
        _ok, rows_rank, _ = _q(f"select s.id, s.name, count(o.id) as c, round(sum(o.energy_kwh),1) from `chargestation`.`charging_order` o join `chargestation`.`station` s on o.station_id=s.id where 1=1{WORDER}{WDATE} group by s.id, s.name order by c desc limit 5;")
        for r in (rows_rank or []):
            station_rank.append({"station_id": r[0], "station_name": r[1], "today_orders": int(r[2] or 0), "today_energy_kwh": float(r[3] or 0)})

    # 5) 地图站点
    stations = []
    if _CE.get("stations"):
        for r in _CE["stations"]:
            try:
                lon = float(r[3] or 0); lat = float(r[4] or 0)
            except Exception:
                lon = lat = 0
            stations.append({"id": r[0], "name": r[1], "region": r[2] or "", "longitude": lon, "latitude": lat, "status": "normal", "status_text": "正常运行"})
    else:
        _ok, rows_map, _ = _q(f"select id,name,area,longitude,latitude from `chargestation`.`station` where 1=1{WSPEC};")
        for r in (rows_map or []):
            try:
                lon = float(r[3] or 0); lat = float(r[4] or 0)
            except Exception:
                lon = lat = 0
            stations.append({"id": r[0], "name": r[1], "region": r[2] or "", "longitude": lon, "latitude": lat, "status": "normal", "status_text": "正常运行"})

    # 6) 告警
    alarms = []
    if _CE.get("alarms"):
        for r in _CE["alarms"]:
            alarms.append({"id": r[0], "occur_time": r[3], "station_id": r[1], "station_name": "", "charger_id": None, "charger_code": "", "content": f"告警等级 {r[2]}", "level": r[2] if r[2] in ("info","warning","critical") else "warning"})
    else:
        _ok, rows_a, _ = _q(f"select id, station_id, level, occur_time from `chargestation`.`alarm` where 1=1{WORDER_DIRECT} order by occur_time desc limit 10;")
        for r in (rows_a or []):
            alarms.append({"id": r[0], "occur_time": r[3], "station_id": r[1], "station_name": "", "charger_id": None, "charger_code": "", "content": f"告警等级 {r[2]}", "level": r[2] if r[2] in ("info","warning","critical") else "warning"})

    # 7) 用户增长
    user_trend = []
    if _CE.get("user_trend"):
        for r in _CE["user_trend"]:
            user_trend.append({"date": r[0], "user_count": int(r[1] or 0)})
    else:
        _ok, rows_u, _ = _q("select substr(register_time,1,10) as d, count(*) from `chargestation`.`user` where register_time >= '2026-09-04' group by substr(register_time,1,10) order by d;")
        for r in (rows_u or []):
            user_trend.append({"date": r[0], "user_count": int(r[1] or 0)})

    # 8) 峰谷电量 + 碳排
    pv = {"day": today_energy, "valley": 0.0, "flat": 0.0, "peak": 0.0}
    if _CE.get("peakvalley"):
        for r in _CE["peakvalley"]:
            try:
                hh = int(r[0]); e = float(r[1] or 0)
            except Exception:
                continue
            if 0 <= hh < 8: pv["valley"] += e
            elif 17 <= hh < 21: pv["peak"] += e
            else: pv["flat"] += e
    else:
        _ok, rows_pv, _ = _q(f"select substr(o.start_time,12,2) as h, round(sum(o.energy_kwh),1) from `chargestation`.`charging_order` o left join `chargestation`.`station` st on o.station_id=st.id where 1=1{WORDER_DIRECT or WORDER} group by substr(o.start_time,12,2);")
        for r in (rows_pv or []):
            try:
                hh = int(r[0]); e = float(r[1] or 0)
            except Exception:
                continue
            if 0 <= hh < 8: pv["valley"] += e
            elif 17 <= hh < 21: pv["peak"] += e
            else: pv["flat"] += e
    if _CE.get("total_energy"):
        total_energy = float(_CE["total_energy"][0][0] or 0)
    else:
        _ok, rows_e, _ = _q(f"select round(sum(o.energy_kwh),1) from `chargestation`.`charging_order` o left join `chargestation`.`station` st on o.station_id=st.id where 1=1{WORDER_DIRECT or WORDER}{WDATE};")
        total_energy = float(_row0(rows_e) or 0)
    FACTOR = 0.7
    today_reduce = round(today_energy * FACTOR / 1000.0, 2)
    total_reduce = round(total_energy * FACTOR / 1000.0, 2)
    valley_ratio = (pv["valley"] / pv["day"]) if pv["day"] else 0
    green_index = round(min(100, 60 + valley_ratio * 40), 1)
    carbon = {"today_energy_kwh": today_energy, "today_reduce_tons": today_reduce, "total_energy_kwh": total_energy, "total_reduce_tons": total_reduce, "green_index": green_index}

    # 9) 事件流
    events = []
    if _CE.get("events"):
        rows_ev = _CE["events"]
    else:
        _ok, rows_ev, _ = _q(f"select id, create_time, user_id, station_id from `chargestation`.`charging_order` where 1=1{WORDER_DIRECT}{WDATE} order by create_time desc limit 8;")
    st_map = {str(s["id"]): s["name"] for s in stations}
    for r in (rows_ev or []):
        uid = r[2]; st = st_map.get(str(r[3]), "某站")
        events.append({"id": f"evt-o{r[0]}", "occur_time": r[1], "type": "order", "content": f"用户{uid} 在 {st} 完成充电"})
    for r in (alarms or []):
        events.append({"id": f"evt-a{r['id']}", "occur_time": r["occur_time"], "type": "alarm", "content": f"站点发生{ r['content'] }告警"})
    events = events[:10]

    # 10) 筛选项（优先从缓存/dash_cache 派生，避免额外 spark 调用把无筛选拖慢）
    _FO_KEY = "filter_options"
    _fo_hit = _CACHE.get(_FO_KEY)
    if _fo_hit and _time.time() - _fo_hit[0] < 300:
        filter_options = _fo_hit[1]
    elif _CE.get("stations") and not use_filter:
        # 从 dash_cache 的 stations 派生 regions/候选
        _regions, _stations_opt = [], []
        reg_set = set()
        for r in _CE["stations"]:
            _stations_opt.append({"id": r[0], "name": r[1]})
            if len(r) > 2 and r[2]:
                reg_set.add(r[2])
        filter_options = {"regions": sorted(reg_set), "stations": _stations_opt}
        _CACHE[_FO_KEY] = (_time.time(), filter_options)
    elif use_filter:
        # 筛选时区域候选可回退到当前筛选后的站点（避免额外 spark）
        _regions = sorted({s.get("region", "") for s in stations if s.get("region")})
        _stations_opt = [{"id": s["id"], "name": s["name"]} for s in stations]
        filter_options = {"regions": _regions, "stations": _stations_opt}
    else:
        _regions, _stations_opt = [], []
        _ok, rows_fo, _ = _q("select id, name, area from `chargestation`.`station`;")
        reg_set = set()
        for r in (rows_fo or []):
            _stations_opt.append({"id": r[0], "name": r[1]})
            if r[2]:
                reg_set.add(r[2])
        _regions = sorted(reg_set)
        filter_options = {"regions": _regions, "stations": _stations_opt}
        _CACHE[_FO_KEY] = (_time.time(), filter_options)

    return {
        "metrics": {
            "station_count": station_count,
            "charger_count": charger_count,
            "online_charger_count": online_charger_count,
            "today_energy_kwh": today_energy,
            "today_orders": today_orders,
            "today_revenue": today_revenue,
        },
        "status_distribution": sd,
        "order_trend": order_trend,
        "energy_revenue_trend": energy_revenue_trend,
        "station_rank": station_rank,
        "stations": stations,
        "alarms": alarms,
        "events": events,
        "user_trend": user_trend,
        "peak_valley": pv,
        "carbon": carbon,
        "filter_options": filter_options,
    }

class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **kw):
        super().__init__(*a, directory=SCREEN_DIR, **kw)

    def log_message(self, fmt, *args):
        sys.stderr.write(f"[{self.command}] {self.path}\n")

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path == "/admin":
            # 开发者模式页（screen/admin.html）
            p = os.path.join(SCREEN_DIR, "admin.html")
            if os.path.exists(p):
                with open(p, encoding="utf-8") as f:
                    html = f.read()
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.end_headers()
                self.wfile.write(html.encode("utf-8"))
                return
            self.send_response(404); self.end_headers(); self.wfile.write(b"admin.html not found")
            return
        if parsed.path == "/api/admin/tables":
            try:
                tables = admin_table_names()
                counts = admin_row_counts(tables)
                _json_reply(self, {"ok": True, "tables": [{"name": t, "count": counts.get(t)} for t in tables],
                                   "updated": _time.strftime("%H:%M:%S")})
            except Exception as e:
                _json_reply(self, {"ok": False, "message": str(e)})
            return
        if parsed.path == "/api/admin/describe":
            q = urllib.parse.parse_qs(parsed.query)
            table = (q.get("table") or [""])[0].strip()
            if table not in admin_table_names():
                _json_reply(self, {"ok": False, "message": "表不存在或不在允许列表：" + table})
                return
            try:
                cols = admin_table_cols(table)
                _json_reply(self, {"ok": True, "table": table, "cols": cols})
            except Exception as e:
                _json_reply(self, {"ok": False, "message": str(e)})
            return
        if parsed.path == "/api/ml":
            try:
                payload = build_ml()
            except Exception as e:
                payload = {"ok": False, "message": str(e)}
            _json_reply(self, payload)
            return
        if parsed.path == "/hadoop":
            # 用 replace 而非 format，避免 CSS 大括号被误当占位符
            html = INDEX_PAGE.replace("{HS2_URI}", HS2_URI).replace("{HS2_USER}", HS2_USER)
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.end_headers()
            self.wfile.write(html.encode("utf-8"))
            return
        if parsed.path == "/":
            # 跳到 index.html（大屏）
            self.path = "/index.html"
        if parsed.path == "/api/snapshot":
            q = urllib.parse.parse_qs(parsed.query)
            date = (q.get("date") or [""])[0].strip()
            region = (q.get("region") or [""])[0].strip()
            sid = (q.get("station_id") or [""])[0].strip()
            ckey = f"{date}|{region}|{sid}"
            now = _time.time()
            hit = _SNAP_CACHE2.get(ckey)
            if hit and now - hit[0] < _SNAP_TTL:
                payload = hit[1]
            else:
                try:
                    payload = build_snapshot(region=region, station_id=sid, date=date)
                    _SNAP_CACHE2[ckey] = [now, payload]
                except Exception as e:
                    payload = {"error": str(e)}
            body = json.dumps(payload, ensure_ascii=False)
            self.send_response(200)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body.encode("utf-8"))
            return
        if parsed.path == "/api/forecast":
            try:
                payload = build_forecast()
            except Exception as e:
                payload = {"error": str(e)}
            body = json.dumps(payload, ensure_ascii=False)
            self.send_response(200)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body.encode("utf-8"))
            return
        super().do_GET()

    def do_POST(self):
        if self.path == "/api/admin/insert":
            length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(length).decode("utf-8", "ignore")
            try:
                data = json.loads(body)
            except Exception:
                data = {}
            table = (data.get("table") or "").strip()
            values = data.get("values") or {}
            if table not in admin_table_names():
                _json_reply(self, {"ok": False, "message": "表不存在或不在允许列表：" + table}); return
            try:
                cols = admin_table_cols(table)
                if not cols:
                    _json_reply(self, {"ok": False, "message": "无法读取表结构"}); return
                vals = ["'" + _esc_sql(values.get(c, "")) + "'" for c in cols]
                sql = "insert into table chargestation.%s (%s) values (%s);" % (table, ", ".join(cols), ", ".join(vals))
                ok, _, _, _, err = run_hive_sql(sql)
                if not ok:
                    # 兼容不支持列名 VALUES 的老版本 Hive：不带列名全列插入
                    ok2, _, _, _, err2 = run_hive_sql("insert into table chargestation.%s values (%s);" % (table, ", ".join(vals)))
                    if not ok2:
                        _json_reply(self, {"ok": False, "message": "插入失败：" + (err2 or err)}); return
                _json_reply(self, {"ok": True, "message": "已向 %s 插入 1 条（重建大屏缓存后生效）" % table})
            except Exception as e:
                _json_reply(self, {"ok": False, "message": str(e)})
            return
        if self.path == "/api/admin/import":
            length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(length)
            ct = self.headers.get("Content-Type", "")
            boundary = ""
            for seg in ct.split(";"):
                seg = seg.strip()
                if seg.startswith("boundary="):
                    boundary = seg[9:].strip().strip('"')
            if not boundary:
                _json_reply(self, {"ok": False, "message": "缺少 multipart boundary"}); return
            try:
                fields, files = _parse_multipart(body, boundary)
            except Exception as e:
                _json_reply(self, {"ok": False, "message": "解析上传失败：" + str(e)}); return
            table = (fields.get("table") or "").strip()
            fmt = (fields.get("format") or "tsv").strip().lower()
            mode = (fields.get("mode") or "append").strip().lower()
            if table not in admin_table_names():
                _json_reply(self, {"ok": False, "message": "表不存在或不在允许列表：" + table}); return
            if not files:
                _json_reply(self, {"ok": False, "message": "未收到文件"}); return
            fname, content = next(iter(files.values()))
            if not content:
                _json_reply(self, {"ok": False, "message": "文件为空"}); return
            try:
                cols = admin_table_cols(table)
                if not cols:
                    _json_reply(self, {"ok": False, "message": "无法读取表结构"}); return
                delim = "\t" if fmt == "tsv" else ","
                text = content.decode("utf-8", "ignore")
                first = next((l for l in text.splitlines() if l.strip()), "")
                n = len(first.split(delim))
                if n != len(cols):
                    _json_reply(self, {"ok": False, "message": "列数不匹配：文件首行 %d 列，表 %s 共 %d 列（%s），请按表列顺序准备文件" % (n, table, len(cols), ",".join(cols[:6]) + ("…" if len(cols) > 6 else ""))}); return
                path = "/tmp/admin_import_%d.%s" % (int(_time.time() * 1000), "tsv" if fmt == "tsv" else "csv")
                with open(path, "wb") as f:
                    f.write(content)
                try:
                    if fmt == "tsv":
                        sql = "load data local inpath '%s' into%s table chargestation.%s;" % (path, " overwrite" if mode == "replace" else "", table)
                        ok, _, _, _, err = run_hive_sql(sql)
                        if not ok:
                            _json_reply(self, {"ok": False, "message": "导入失败：" + err}); return
                        nrows = len([l for l in text.splitlines() if l.strip()])
                        _json_reply(self, {"ok": True, "message": "已导入 %d 行到 %s（请重建大屏缓存）" % (nrows, table)})
                    else:
                        tmp = "_admin_tmp_%d" % int(_time.time())
                        run_hive_sql("drop table if exists chargestation.%s;" % tmp)
                        cols_ddl = ", ".join("%s string" % c for c in cols)
                        ok, _, _, _, err = run_hive_sql("create table chargestation.%s (%s) row format delimited fields terminated by ',';" % (tmp, cols_ddl))
                        if not ok:
                            _json_reply(self, {"ok": False, "message": "建临时表失败：" + err}); return
                        ok, _, _, _, err = run_hive_sql("load data local inpath '%s' into table chargestation.%s;" % (path, tmp))
                        if not ok:
                            _json_reply(self, {"ok": False, "message": "装载临时表失败：" + err}); return
                        ok, _, _, _, err = run_hive_sql("insert %s table chargestation.%s select * from chargestation.%s;" % ("overwrite" if mode == "replace" else "into", table, tmp))
                        run_hive_sql("drop table if exists chargestation.%s;" % tmp)
                        if not ok:
                            _json_reply(self, {"ok": False, "message": "写入目标表失败：" + err}); return
                        nrows = len([l for l in text.splitlines() if l.strip()])
                        _json_reply(self, {"ok": True, "message": "已导入 %d 行到 %s（请重建大屏缓存）" % (nrows, table)})
                finally:
                    try:
                        os.remove(path)
                    except Exception:
                        pass
            except Exception as e:
                _json_reply(self, {"ok": False, "message": str(e)})
            return
        if self.path == "/api/admin/clear":
            length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(length).decode("utf-8", "ignore")
            try:
                data = json.loads(body)
            except Exception:
                data = {}
            table = (data.get("table") or "").strip()
            if table not in admin_table_names():
                _json_reply(self, {"ok": False, "message": "表不存在或不在允许列表：" + table}); return
            ok, _, _, _, err = run_hive_sql("truncate table chargestation.%s;" % table)
            _json_reply(self, {"ok": ok, "message": ("已清空 %s（请重建大屏缓存）" % table) if ok else ("清空失败：" + err)})
            return
        if self.path == "/api/admin/rebuild":
            out = os.path.join(SCREEN_DIR, "dash_cache.json")
            eng = os.path.join(SCREEN_DIR, "dash_engine.py")
            if not os.path.exists(eng):
                _json_reply(self, {"ok": False, "message": "找不到 dash_engine.py"}); return
            try:
                def _run():
                    subprocess.Popen([sys.executable, eng, out],
                                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                threading.Thread(target=_run, daemon=True).start()
                _json_reply(self, {"ok": True, "message": "已触发大屏缓存重建（dash_engine.py），约 30~60 秒后大屏自动生效"})
            except Exception as e:
                _json_reply(self, {"ok": False, "message": str(e)})
            return
        if self.path == "/api/hive":
            length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(length).decode("utf-8", "ignore")
            try:
                data = json.loads(body)
                sql = data.get("sql", "")
            except Exception:
                data, sql = {}, ""
            ok, cols, rows, elapsed, err = run_hive_sql(sql)
            payload = {"ok": ok, "cols": cols, "rows": rows, "message": err}
            res = json.dumps(payload, ensure_ascii=False)
            self.send_response(200)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(res.encode("utf-8"))
            return
        if self.path == "/api/spark":
            length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(length).decode("utf-8", "ignore")
            try:
                data = json.loads(body)
                sql = data.get("sql", "")
            except Exception:
                data, sql = {}, ""
            ok, cols, rows, err = run_spark_sql(sql)
            payload = {"ok": ok, "cols": cols, "rows": rows, "message": err}
            res = json.dumps(payload, ensure_ascii=False)
            self.send_response(200)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(res.encode("utf-8"))
            return
        self.send_response(404)
        self.end_headers()
        self.wfile.write(b'{"ok":false,"message":"not found"}')

if __name__ == "__main__":
    os.makedirs(SCREEN_DIR, exist_ok=True)
    server = http.server.ThreadingHTTPServer((HOST, PORT), Handler)
    print(f"▶ 大屏地址: http://<虚拟机IP>:{PORT}/index.html")
    print(f"▶ Hadoop查询演示: http://<虚拟机IP>:{PORT}/hadoop")
    print(f"▶ 正在监听 0.0.0.0:{PORT}  (Ctrl+C 停止)")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n已停止")