(function () {
  "use strict";
  var byId = function (id) { return document.getElementById(id); };
  var DOW = { 1: "周日", 2: "周一", 3: "周二", 4: "周三", 5: "周四", 6: "周五", 7: "周六" };
  var LEVEL = { low: "低风险", medium: "中风险", high: "高风险" };

  // ---------- 兜底样例数据（mock / 网关不可用时保证面板有内容）----------
  function mockData() {
    var heat = [];
    for (var dow = 2; dow <= 7; dow++) { for (var h = 0; h < 24; h++) {
      var peak = Math.exp(-Math.pow((h - 8) / 2.5, 2)) * 12 + Math.exp(-Math.pow((h - 18) / 3, 2)) * 18;
      var weekend = dow >= 6 ? 0.75 : 1;
      heat.push({ dow: dow, hour: h, area: "样例", demand: Math.max(1, Math.round(peak * weekend)) });
    }}
    heat.push({ dow: 1, hour: 18, area: "样例", demand: 15 });
    return {
      demand_heat: heat,
      top_surge: [{ dow: 6, hour: 18, area: "商业区", delta: 0.62, demand: 45 }],
      health: { dist: [2, 1, 2, 5, 30],
        risk: [
          { station: "科技园充电站", code: "A-008", health: 42, level: "high", risk: 0.46 },
          { station: "科技园充电站", code: "A-009", health: 50, level: "high", risk: 0.4 },
          { station: "商业中心快充站", code: "B-009", health: 58, level: "medium", risk: 0.34 }
        ] },
      whatif: { avg_revenue_per_order: 11.6, avg_energy_per_order: 8.1, daily_orders: 168, daily_revenue: 1950,
                peak_hour: 18, peak_demand: 72, queue: 6, charger_count: 38 }
    };
  }

  function renderHeat(rows) {
    var map = {};
    (rows || []).forEach(function (x) {
      var key = x.dow + "-" + x.hour;
      map[key] = (map[key] || 0) + (x.demand || 0);
    });
    var agg = Object.keys(map).map(function (k) {
      var p = k.split("-");
      return { dow: +p[0], hour: +p[1], value: map[k] };
    });
    window.ScreenCharts && window.ScreenCharts.demandHeat(agg);
  }

  function renderSurge(items) {
    var el = byId("demand-surge");
    if (!el) return;
    el.replaceChildren();
    if (!items || !items.length) {
      var none = document.createElement("span");
      none.className = "tag muted";
      none.textContent = "暂无需求激增窗口";
      el.appendChild(none);
      return;
    }
    items.slice(0, 3).forEach(function (s) {
      var t = document.createElement("span");
      t.className = "tag surge";
      t.textContent = DOW[s.dow] + " " + s.hour + "时 " + s.area + " 需求+" + Math.round(s.delta * 100) + "%";
      el.appendChild(t);
    });
  }

  function renderHealth(health) {
    health = health || { dist: [0, 0, 0, 0, 0], risk: [] };
    window.ScreenCharts && window.ScreenCharts.healthDist(health.dist);
    var el = byId("health-risk");
    if (!el) return;
    el.replaceChildren();
    var items = (health.risk || []).slice(0, 3);
    if (!items.length) {
      var e = document.createElement("div");
      e.className = "empty";
      e.textContent = "暂无高风险设备";
      el.appendChild(e);
      return;
    }
    items.forEach(function (r) {
      var row = document.createElement("div");
      row.className = "ml-row";
      var info = document.createElement("span");
      info.className = "ml-info";
      info.textContent = r.station + " / " + r.code;
      var h = document.createElement("span");
      h.className = "ml-health";
      h.textContent = "健康 " + r.health;
      var lv = document.createElement("span");
      lv.className = "ml-level " + r.level;
      lv.textContent = LEVEL[r.level] || r.level;
      row.append(info, h, lv);
      el.appendChild(row);
    });
  }

  // ---------- What-If 决策推演 ----------
  var _base = null;
  function fmt(n, d) { return Number(n || 0).toLocaleString("zh-CN", { maximumFractionDigits: d == null ? 0 : d }); }

  function whatifDOM(base) {
    _base = base;
    var el = byId("whatif-panel");
    if (!el) return;
    el.replaceChildren();
    var params = [
      { key: "add", label: "增配桩数（台）", min: -5, max: 10, step: 1, val: 0, unit: "台" },
      { key: "price", label: "电价调整（元/kWh）", min: -0.5, max: 0.5, step: 0.05, val: 0, unit: "元" },
      { key: "traffic", label: "客流变化", min: -30, max: 30, step: 5, val: 0, unit: "%" }
    ];
    params.forEach(function (p) {
      var lab = document.createElement("label");
      lab.className = "whatif-item";
      var head = document.createElement("span");
      head.className = "whatif-label";
      var val = document.createElement("b");
      val.className = "whatif-value";
      val.id = "wi-" + p.key + "-v";
      head.textContent = p.label + "：";
      lab.append(head, val);
      var input = document.createElement("input");
      input.type = "range";
      input.min = p.min; input.max = p.max; input.step = p.step; input.value = p.val;
      input.dataset.key = p.key;
      input.oninput = computeWhatif;
      lab.appendChild(input);
      el.appendChild(lab);
    });
    var grid = document.createElement("div");
    grid.className = "whatif-results";
    [["预计日订单量", "wi-orders", "笔"], ["预计日营收", "wi-revenue", "元"],
     ["高峰利用率", "wi-util", "%"], ["预计排队等待", "wi-wait", "分钟"]].forEach(function (c) {
      var card = document.createElement("div");
      card.className = "whatif-card";
      var t = document.createElement("span");
      t.className = "whatif-card-label";
      t.textContent = c[0];
      var v = document.createElement("strong");
      v.id = c[1];
      var u = document.createElement("small");
      u.textContent = c[2];
      card.append(t, v, u);
      grid.appendChild(card);
    });
    el.appendChild(grid);
    var note = document.createElement("div");
    note.className = "whatif-note";
    note.textContent = "基线：日订单 " + fmt(base.daily_orders) + " 笔 · 日营收 ¥" + fmt(base.daily_revenue, 0) +
      " · 高峰 " + base.peak_hour + "时 " + fmt(base.peak_demand) + " 单 · 桩 " + fmt(base.charger_count) + " 台";
    el.appendChild(note);
    computeWhatif();
  }

  function computeWhatif() {
    if (!_base) return;
    var add = 0, price = 0, traffic = 0;
    document.querySelectorAll("#whatif-panel input[type=range]").forEach(function (i) {
      var v = parseFloat(i.value);
      if (i.dataset.key === "add") add = v;
      if (i.dataset.key === "price") price = v;
      if (i.dataset.key === "traffic") traffic = v;
      byId("wi-" + i.dataset.key + "-v").textContent = (i.dataset.key === "traffic" ? (v > 0 ? "+" : "") + v + "%" : (v > 0 ? "+" : "") + v + (i.dataset.key === "price" ? " 元" : " 台"));
    });
    var base = _base;
    var tf = traffic / 100, pd = price;
    var orders = Math.max(0, Math.round(base.daily_orders * (1 + tf) * (1 - 0.2 * pd)));
    var revPerOrder = base.avg_revenue_per_order + pd * base.avg_energy_per_order;
    var revenue = Math.max(0, Math.round(orders * revPerOrder));
    var total = Math.max(1, base.charger_count + add);
    var util = Math.min(100, Math.round(base.peak_demand * (1 + tf) / total * 100));
    var wait = Math.max(0, Math.round(base.queue * 20 * base.charger_count / total));
    byId("wi-orders").textContent = fmt(orders);
    byId("wi-revenue").textContent = "¥" + fmt(revenue, 0);
    byId("wi-util").textContent = fmt(util);
    byId("wi-wait").textContent = fmt(wait);
  }

  // ---------- 入口 ----------
  function load() {
    if (window.ScreenConfig && window.ScreenConfig.mode === "live") {
      fetch("/api/ml").then(function (r) { return r.json(); }).then(function (res) {
        if (res && res.ok) { render(res); return; }
        render(mockData());
      }).catch(function () { render(mockData()); });
    } else {
      render(mockData());
    }
  }

  function render(res) {
    renderHeat(res.demand_heat);
    renderSurge(res.top_surge);
    renderHealth(res.health);
    whatifDOM(res.whatif);
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", load);
  } else {
    load();
  }
}());
