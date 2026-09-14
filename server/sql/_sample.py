import sqlite3, json
for t in ["station","charger","charging_order","user","alarm","price_rule","weather"]:
    c = sqlite3.connect("charge.db"); c.row_factory=sqlite3.Row
    r = [dict(x) for x in c.execute(f"select * from {t} limit 1")]
    print(t, json.dumps(r[0], ensure_ascii=False, default=str))