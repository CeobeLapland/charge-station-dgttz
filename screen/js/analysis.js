(function () {
  "use strict";
  var byId = function (id) { return document.getElementById(id); };
  var mask = byId("analysis-mask");
  if (!mask) return;

  // 与大屏 /hadoop 页同款的 SPARK-SQL 快捷分析
  var QUERIES = [
    ["区域分布", "select st.area as region, count(distinct st.id) as stations, round(sum(co.energy_kwh),1) as kwh from chargestation.charging_order co join chargestation.station st on co.station_id=st.id where co.create_time like '20%%' group by st.area order by kwh desc limit 8;"],
    ["电桩类型", "select ch.type as type, count(*) as cnt, round(avg(ch.power),1) as avg_power from chargestation.charger ch group by ch.type;"],
    ["订单状态", "select status,count(*) as cnt from chargestation.charging_order group by status;"],
    ["分时电量", "select cast(substr(start_time,12,2) as int) as h, round(sum(energy_kwh),1) as kwh from chargestation.charging_order group by cast(substr(start_time,12,2) as int) order by h;"],
    ["站点订单TOP", "select st.name as station, count(*) as od from chargestation.charging_order o join chargestation.station st on o.station_id=st.id group by st.name order by od desc limit 6;"],
    ["用户等级", "select level, count(*) as cnt, round(avg(balance),2) as avg_bal from chargestation.user group by level;"],
    ["电价档位收入", "select price_level, round(sum(pay_amount),2) as income from chargestation.charging_order group by price_level order by income desc;"],
    ["峰谷电量", "select case when cast(substr(start_time,12,2) as int) between 0 and 8 then 'valley' when cast(substr(start_time,12,2) as int) between 17 and 21 then 'peak' else 'flat' end as period, round(sum(energy_kwh),1) kwh, round(sum(pay_amount),2) income from chargestation.charging_order group by case when cast(substr(start_time,12,2) as int) between 0 and 8 then 'valley' when cast(substr(start_time,12,2) as int) between 17 and 21 then 'peak' else 'flat' end;"]
  ];

  var chips = byId("analysis-chips");
  QUERIES.forEach(function (q) {
    var span = document.createElement("span");
    span.className = "q";
    span.textContent = q[0];
    span.onclick = function () { byId("analysis-sql").value = q[1]; run("spark"); };
    chips.appendChild(span);
  });

  function open() { mask.hidden = false; }
  function close() { mask.hidden = true; }
  byId("analysis-button").onclick = open;
  byId("analysis-close").onclick = close;
  mask.onclick = function (e) { if (e.target === mask) close(); };
  document.addEventListener("keydown", function (e) { if (e.key === "Escape" && !mask.hidden) close(); });
  byId("hadoop-button").onclick = function () { location.href = "/hadoop"; };
  byId("admin-button").onclick = function () { location.href = "/admin"; };

  function run(engine) {
    var sql = byId("analysis-sql").value.trim();
    var out = byId("analysis-out");
    var state = byId("analysis-state");
    if (!sql) { out.textContent = "SQL 不能为空"; return; }
    out.textContent = engine === "spark"
      ? "正在提交 SPARK-SQL 引擎执行，请稍候…（首次约 10~30 秒）"
      : "正在提交 HiveServer2 执行，请稍候…";
    state.textContent = engine === "spark" ? "SPARK-SQL 实时计算中…" : "Hive 执行中…";
    fetch("/api/" + (engine === "spark" ? "spark" : "hive"), {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ sql: sql })
    }).then(function (r) { return r.json(); }).then(function (res) {
      state.textContent = engine === "spark" ? "SPARK-SQL 实时计算，首次约 10~30 秒" : "";
      if (res.ok) {
        var hasCols = res.cols && res.cols.length;
        if (hasCols || (res.rows && res.rows.length)) {
          out.textContent = (hasCols ? "列: " + res.cols.join(" | ") + "\n" : "")
            + res.rows.map(function (r) { return r.join(" | "); }).join("\n")
            + "\n\n共 " + res.rows.length + " 行（" + (engine === "spark" ? "SPARK-SQL" : "Hive") + " 返回）";
        } else { out.textContent = "(空结果) 执行成功"; }
      } else { out.textContent = "执行失败:\n" + res.message; }
    }).catch(function (e) {
      state.textContent = "";
      out.textContent = "请求失败: " + e;
    });
  }
  byId("analysis-run").onclick = function () { run("spark"); };
  byId("analysis-run-hive").onclick = function () { run("hive"); };
}());
