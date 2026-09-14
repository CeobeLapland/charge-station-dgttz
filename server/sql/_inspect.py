import sqlite3, json
c = sqlite3.connect("charge.db")
c.row_factory = sqlite3.Row
tabs = [r[0] for r in c.execute("select name from sqlite_master where type='table' and name not like 'sqlite_%' order by name")]
info = []
for t in tabs:
    n = c.execute(f"select count(*) from {t}").fetchone()[0]
    cols = [r[1] for r in c.execute(f"pragma table_info({t})")]
    info.append({"table": t, "rows": n, "cols": cols})
print(json.dumps(info, ensure_ascii=False, indent=1))