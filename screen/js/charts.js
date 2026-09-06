(function () {
  "use strict";
  const instances = new Map();
  const axis = "#e3e7eb", label = "#788596", split = "#f4f5f7", blue = "#557da4", mutedBlue = "#91a6ba";
  const tooltip = { trigger: "item", backgroundColor: "rgba(255,255,255,.98)", borderColor: "#dfe5ed", textStyle: { color: "#263548", fontSize: 11 } };
  const escapeHtml = (value) => String(value).replaceAll("&", "&amp;").replaceAll("<", "&lt;").replaceAll(">", "&gt;").replaceAll('"', "&quot;").replaceAll("'", "&#039;");
  const timeLabel = (value) => { const date = new Date(value); return Number.isNaN(date.getTime()) ? String(value ?? "") : date.toLocaleTimeString("zh-CN", { hour: "2-digit", minute: "2-digit", hour12: false }); };
  const baseXAxis = { axisLabel: { color: label, fontSize: 10 }, axisLine: { lineStyle: { color: axis } }, axisTick: { show: false } };
  const baseYAxis = { axisLabel: { color: label, fontSize: 10 }, axisLine: { show: false }, axisTick: { show: false }, splitLine: { lineStyle: { color: split } } };

  function chartFor(id) {
    const element = document.getElementById(id);
    if (!element || !window.echarts) return null;
    if (!instances.has(id)) instances.set(id, window.echarts.init(element, null, { renderer: "canvas" }));
    return instances.get(id);
  }

  function renderLoad(actual) {
    const chart = chartFor("load-chart"); if (!chart) return;
    chart.setOption({
      tooltip: { ...tooltip, trigger: "axis" },
      legend: { data: ["总功率", "使用中电站"], right: 4, top: 0, itemWidth: 14, itemHeight: 6, textStyle: { color: label, fontSize: 9 } },
      grid: { left: 48, right: 48, top: 26, bottom: 22 },
      xAxis: { ...baseXAxis, type: "category", boundaryGap: false, data: actual.map((item) => timeLabel(item.timestamp)) },
      yAxis: [
        { ...baseYAxis, type: "value", name: "kW", nameTextStyle: { color: label, fontSize: 10 } },
        { ...baseYAxis, type: "value", name: "座", minInterval: 1, splitLine: { show: false }, nameTextStyle: { color: label, fontSize: 10 } }
      ],
      series: [
        { name: "总功率", type: "line", data: actual.map((item) => item.value_kw), showSymbol: false, smooth: .2, lineStyle: { width: 2, color: blue }, itemStyle: { color: blue } },
        { name: "使用中电站", type: "line", yAxisIndex: 1, data: actual.map((item) => item.active_station_count), showSymbol: false, step: "end", lineStyle: { width: 1.5, color: "#b45f68" }, itemStyle: { color: "#b45f68" } }
      ]
    }, true);
  }

  function renderStations(stations) {
    const chart = chartFor("station-chart"); if (!chart) return;
    const thresholds = window.ScreenConfig.loadThresholds;
    chart.setOption({
      tooltip: { ...tooltip, formatter: ({ data }) => `<strong>${escapeHtml(data.name)}</strong><br>负载：${data.loadRate.toFixed(1)}%<br>空闲：${data.idle}/${data.total} 台<br>今日营收：¥${data.revenue.toFixed(2)}` },
      grid: { left: 48, right: 15, top: 12, bottom: 28 },
      xAxis: { ...baseXAxis, type: "value", scale: true, min: ({ min }) => min - .01, max: ({ max }) => max + .01, name: "经度", nameTextStyle: { color: label } },
      yAxis: { ...baseYAxis, type: "value", scale: true, min: ({ min }) => min - .01, max: ({ max }) => max + .01, name: "纬度", nameTextStyle: { color: label } },
      series: [{ type: "scatter", symbolSize: (value) => Math.max(10, Math.min(20, value[2] / 5)), data: stations.map((s) => ({ name: s.name, value: [s.longitude, s.latitude, s.load_rate], loadRate: s.load_rate, idle: s.idle_chargers, total: s.total_chargers, revenue: s.today_revenue, itemStyle: { color: s.load_rate > thresholds.congested ? "#a7666d" : s.load_rate >= thresholds.busy ? "#8797a8" : blue } })) }]
    }, true);
  }

  function renderUtilization(items) {
    const chart = chartFor("utilization-chart"); if (!chart) return;
    const sorted = [...items].filter((item) => Number.isFinite(Number(item.utilization_rate))).sort((a,b) => b.utilization_rate - a.utilization_rate).slice(0,10).reverse();
    chart.setOption({ tooltip: { ...tooltip, trigger: "axis", axisPointer: { type: "shadow" } }, grid: { left: 132, right: 18, top: 8, bottom: 22 }, xAxis: { ...baseYAxis, type: "value", max: 100, axisLabel: { color: label, formatter: "{value}%" } }, yAxis: { ...baseXAxis, type: "category", data: sorted.map((item) => item.station_name), axisLabel: { color: label, width: 116, overflow: "truncate", ellipsis: "…" } }, series: [{ type: "bar", data: sorted.map((item) => item.utilization_rate), barWidth: 8, itemStyle: { borderRadius: 2, color: "#6687a8" } }] }, true);
  }

  function renderStationPower(station) {
    const chart = chartFor("station-power-chart"); if (!chart) return;
    const items = station?.power_series ?? [];
    chart.setOption({ tooltip: { ...tooltip, trigger: "axis" }, grid: { left: 46, right: 12, top: 18, bottom: 24 }, xAxis: { ...baseXAxis, type: "category", boundaryGap: false, data: items.map((item) => timeLabel(item.timestamp)) }, yAxis: { ...baseYAxis, type: "value", name: "kW", nameTextStyle: { color: label } }, series: [{ type: "line", data: items.map((item) => item.value_kw), showSymbol: false, smooth: .2, lineStyle: { width: 2, color: blue }, itemStyle: { color: blue } }], graphic: items.length ? [] : [{ type: "text", left: "center", top: "middle", style: { text: "暂无单站功率数据", fill: label, fontSize: 12 } }] }, true);
  }

  function renderUsers(items) {
    const chart = chartFor("user-chart"); if (!chart) return;
    chart.setOption({ tooltip: { ...tooltip, trigger: "axis" }, grid: { left: 42, right: 12, top: 12, bottom: 24 }, xAxis: { ...baseXAxis, type: "category", boundaryGap: false, data: items.map((item) => item.date) }, yAxis: { ...baseYAxis, type: "value" }, series: [{ type: "line", data: items.map((item) => Number(item.new_users) || 0), showSymbol: false, smooth: .2, lineStyle: { color: blue, width: 2 }, itemStyle: { color: blue } }] }, true);
  }

  function renderEnergy(energy) {
    const chart = chartFor("energy-chart"); if (!chart) return;
    chart.setOption({ tooltip: { ...tooltip, formatter: "{b}<br>{c} kWh · {d}%" }, legend: { bottom: 0, itemWidth: 12, itemHeight: 7, textStyle: { color: label, fontSize: 10 } }, series: [{ type: "pie", radius: ["50%","66%"], center: ["50%","43%"], label: { color: "#697586", fontSize: 10, formatter: "{b}  {d}%" }, labelLine: { length: 8, length2: 5, lineStyle: { color: "#d8dde3" } }, itemStyle: { borderColor: "#fff", borderWidth: 2 }, data: [{ name: "谷时", value: energy.valley, itemStyle: { color: "#c6d0da" } },{ name: "平时", value: energy.flat, itemStyle: { color: "#91a6ba" } },{ name: "峰时", value: energy.peak, itemStyle: { color: blue } }] }] }, true);
  }

  function renderStationRevenue(stations) {
    const chart = chartFor("station-revenue-chart"); if (!chart) return;
    const items = [...stations].sort((a,b) => b.today_revenue - a.today_revenue).slice(0,8).reverse();
    chart.setOption({ tooltip: { ...tooltip, trigger: "axis", axisPointer: { type: "shadow" } }, grid: { left: 132, right: 20, top: 10, bottom: 22 }, xAxis: { ...baseYAxis, type: "value", name: "元", nameTextStyle: { color: label } }, yAxis: { ...baseXAxis, type: "category", data: items.map((item) => item.name), axisLabel: { color: label, width: 116, overflow: "truncate", ellipsis: "…" } }, series: [{ type: "bar", data: items.map((item) => item.today_revenue), barWidth: 8, itemStyle: { color: blue, borderRadius: 2 } }] }, true);
  }

  function renderLoadDistribution(stations) {
    const chart = chartFor("load-distribution-chart"); if (!chart) return;
    const counts = [stations.filter((s) => s.load_rate < 60).length, stations.filter((s) => s.load_rate >= 60 && s.load_rate <= 85).length, stations.filter((s) => s.load_rate > 85).length];
    chart.setOption({ tooltip: { ...tooltip, formatter: "{b}<br>{c} 座 · {d}%" }, legend: { bottom: 0, itemWidth: 12, itemHeight: 7, textStyle: { color: label, fontSize: 10 } }, series: [{ type: "pie", radius: ["50%","66%"], center: ["50%","43%"], label: { color: "#697586", fontSize: 10, formatter: "{b}  {c}座" }, itemStyle: { borderColor: "#fff", borderWidth: 2 }, data: [{ name: "低负载", value: counts[0], itemStyle: { color: "#b8c7d5" } },{ name: "中负载", value: counts[1], itemStyle: { color: "#8299af" } },{ name: "高负载", value: counts[2], itemStyle: { color: "#a7666d" } }] }] }, true);
  }

  let timer = null;
  window.addEventListener("resize", () => { window.clearTimeout(timer); timer = window.setTimeout(() => instances.forEach((chart) => chart.resize()), 120); });
  window.ScreenCharts = {
    render(data, selectedStation) { renderLoad(data.load_series.actual); renderStations(data.stations); renderUtilization(data.utilization_rank); renderStationPower(selectedStation); renderUsers(data.user_growth); renderEnergy(data.energy_by_price_level); renderStationRevenue(data.stations); renderLoadDistribution(data.stations); },
    resize() { instances.forEach((chart) => chart.resize()); },
    dispose() { instances.forEach((chart) => chart.dispose()); instances.clear(); }
  };
}());
