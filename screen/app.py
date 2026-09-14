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
  out.textContent='正在提交到 Hive 执行，请稍候…';
  fetch('/api/hive',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({sql:sql})})
    .then(function(r){return r.json();})
    .then(function(res){
      if(res.ok){
        if(res.cols){out.textContent='列: '+res.cols.join(' | ')+'\\n'+res.rows.map(function(r){return r.join(' | ');}).join('\\n')+'\\n\\n共 '+res.rows.length+' 行（数据由 Hive 实时计算返回）';}
        else{out.textContent=res.message;}
      }else{out.textContent='执行失败:\\n'+res.message;}
    })
    .catch(function(e){out.textContent='请求失败: '+e;});
}
</script>
</body></html>"""

def _q(sql):
    """执行查询，返回 (ok, rows[list[list]], err)"""
    ok, cols, rows, elapsed, err = run_hive_sql(sql)
    return ok, ok and rows or [], ("" if ok else err)

def _row0(rows):
    """返回第一行第一列或 None"""
    return rows[0][0] if rows else None

def build_snapshot():
    """从 Hive 聚合出大屏需要的完整快照（与 screen.snapshot_resp.payload 同构）"""
    # 1) 状态分布：charger.status group by（reserved 并入 charging）
    ok, rows, err = _q("select status,count(*) as c from `chargestation`.`charger` group by status;")
    sd = {"idle": 0, "charging": 0, "offline": 0, "fault": 0}
    for r in rows or []:
        st, n = (r[0] or "").strip(), int(r[1] or 0)
        if st == "idle": sd["idle"] = n
        elif st == "offline": sd["offline"] = n
        elif st == "fault": sd["fault"] = n
        else: sd["charging"] += n  # charging / reserved 都算在充电
    # 桩总数 & 在线桩 = idle + charging
    ok, rows_c, err = _q("select count(*) from `chargestation`.`charger`;")
    charger_count = int(_row0(rows_c) or 0)
    online_charger_count = sd["idle"] + sd["charging"]

    # 2) 站点数与在线率
    _ok, rows_s, _ = _q("select count(*) from `chargestation`.`station`;")
    station_count = int(_row0(rows_s) or 0)

    # 3) 订单指标：今日订单/近30天电量/收入
    year, month, day = "2026", "09", "10"  # 以数据日期最大日作为"今天"
    _ok, rows_t, _ = _q(f"select count(*), round(sum(energy_kwh),1), round(sum(pay_amount),2) from `chargestation`.`charging_order` where create_time like '{year}-{month}-{day}%';")
    today_orders = today_energy = today_revenue = 0
    if rows_t and rows_t[0]:
        today_orders = int(rows_t[0][0] or 0)
        today_energy = float(rows_t[0][1] or 0)
        today_revenue = float(rows_t[0][2] or 0)

    # 4) 近7天趋势（按创建日期分组）
    trend_rows_by_date = {}
    _ok, rows_trend, _ = _q(f"select substr(create_time,1,10) as d, count(*), round(sum(energy_kwh),1), round(sum(pay_amount),2) from `chargestation`.`charging_order` group by substr(create_time,1,10) order by d desc limit 7;")
    order_trend, energy_revenue_trend = [], []
    for r in (rows_trend or []):
        d = r[0]
        order_trend.append({"date": d, "order_count": int(r[1] or 0)})
        energy_revenue_trend.append({"date": d, "energy_kwh": float(r[2] or 0), "revenue": float(r[3] or 0)})
    order_trend.reverse(); energy_revenue_trend.reverse()

    # 5) 站点排行（近30天每站订单量）
    station_rank = []
    _ok, rows_rank, _ = _q("select s.id, s.name, count(o.id) as c, round(sum(o.energy_kwh),1) from `chargestation`.`charging_order` o join `chargestation`.`station` s on o.station_id=s.id where o.create_time >= '2026-08-11' group by s.id, s.name order by c desc limit 5;")
    for r in (rows_rank or []):
        station_rank.append({"station_id": r[0], "station_name": r[1], "today_orders": int(r[2] or 0), "today_energy_kwh": float(r[3] or 0)})

    # 6) 地图站点
    stations = []
    _ok, rows_map, _ = _q("select id,name,area,longitude,latitude from `chargestation`.`station`;")
    for r in (rows_map or []):
        try:
            lon = float(r[3] or 0); lat = float(r[4] or 0)
        except Exception:
            lon = lat = 0
        stations.append({"id": r[0], "name": r[1], "region": r[2] or "", "longitude": lon, "latitude": lat, "status": "normal", "status_text": "正常运行"})

    # 7) 告警
    alarms = []
    _ok, rows_a, _ = _q("select id, station_id, level, occur_time from `chargestation`.`alarm` order by occur_time desc limit 10;")
    for r in (rows_a or []):
        st_name = ""
        alarms.append({"id": r[0], "occur_time": r[3], "station_id": r[1], "station_name": st_name, "charger_id": None, "charger_code": "", "content": f"告警等级 {r[2]}", "level": r[2] if r[2] in ("info","warning","critical") else "warning"})

    # 8) 用户增长趋势（近7日注册）
    user_trend = []
    _ok, rows_u, _ = _q("select substr(register_time,1,10) as d, count(*) from `chargestation`.`user` where register_time >= '2026-09-04' group by substr(register_time,1,10) order by d;")
    for r in (rows_u or []):
        user_trend.append({"date": r[0], "user_count": int(r[1] or 0)})

    # 9) 峰谷电量（按时段手动分档，与默认 price_rule 一致）
    #    谷 00-08 / 平 08-17+21-24 / 峰 17-21
    _ok, rows_pv, _ = _q("select substr(start_time,12,2) as h, round(sum(energy_kwh),1) from `chargestation`.`charging_order` group by substr(start_time,12,2);")
    pv = {"day": today_energy, "valley": 0.0, "flat": 0.0, "peak": 0.0}
    for r in (rows_pv or []):
        try:
            hh = int(r[0]); e = float(r[1] or 0)
        except Exception:
            continue
        if 0 <= hh < 8: pv["valley"] += e
        elif 17 <= hh < 21: pv["peak"] += e
        else: pv["flat"] += e

    # 10) 碳排 / 绿色充电指数（碳排因子 0.7kg/kWh）
    _ok, rows_e, _ = _q("select round(sum(energy_kwh),1) from `chargestation`.`charging_order`;")
    total_energy = float(_row0(rows_e) or 0)
    FACTOR = 0.7  # kg CO2 / kWh
    today_reduce = round(today_energy * FACTOR / 1000.0, 2)      # 吨
    total_reduce = round(total_energy * FACTOR / 1000.0, 2)
    valley_ratio = (pv["valley"] / pv["day"]) if pv["day"] else 0
    green_index = round(min(100, 60 + valley_ratio * 40), 1)     # 基于谷电占比的样子指数
    carbon = {
        "today_energy_kwh": today_energy,
        "today_reduce_tons": today_reduce,
        "total_energy_kwh": total_energy,
        "total_reduce_tons": total_reduce,
        "green_index": green_index,
    }

    # 11) 实时事件流（订单创建 + 告警）
    events = []
    _ok, rows_ev, _ = _q("select id, create_time, user_id, station_id from `chargestation`.`charging_order` order by create_time desc limit 8;")
    st_map = {str(s["id"]): s["name"] for s in stations}
    for r in (rows_ev or []):
        uid = r[2]; st = st_map.get(str(r[3]), "某站")
        phone = f"用户{uid}"
        events.append({"id": f"evt-o{r[0]}", "occur_time": r[1], "type": "order", "content": f"{phone} 在 {st} 完成充电"})
    for r in (rows_a or []):
        events.append({"id": f"evt-a{r[0]}", "occur_time": r[3], "type": "alarm", "content": f"站点发生{ r[2] }告警"})
    events = events[:10]

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
        "filter_options": {"regions": [], "stations": [{"id": s["id"], "name": s["name"]} for s in stations]},
    }

class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **kw):
        super().__init__(*a, directory=SCREEN_DIR, **kw)

    def log_message(self, fmt, *args):
        sys.stderr.write(f"[{self.command}] {self.path}\n")

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
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
            try:
                payload = build_snapshot()
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