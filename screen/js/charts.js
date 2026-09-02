(function () {
  "use strict";

  const instances = new Map();
  const axisColor = "#cbd5e1";
  const labelColor = "#64748b";
  const splitColor = "#edf2f7";

  function chartFor(id) {
    const element = document.getElementById(id);
    if (!element || !window.echarts) {
      if (element) {
        element.replaceChildren();
        const fallback = document.createElement("div");
        fallback.className = "chart-fallback";
        fallback.append(
          document.createTextNode("ECharts 未加载"),
          document.createElement("br"),
          document.createTextNode("正式离线交付时请使用本地 vendor 文件")
        );
        element.appendChild(fallback);
      }
      return null;
    }
    if (!instances.has(id)) {
      instances.set(id, window.echarts.init(element, null, { renderer: "canvas" }));
    }
    return instances.get(id);
  }

  const tooltip = {
    trigger: "item",
    backgroundColor: "rgba(6, 18, 31, 0.96)",
    borderColor: "rgba(80, 184, 255, 0.35)",
    textStyle: { color: "#dff4ff", fontSize: 11 }
  };

  const formatTime = (value) => {
    const date = new Date(value);
    return Number.isNaN(date.getTime())
      ? String(value ?? "")
      : date.toLocaleTimeString("zh-CN", { hour: "2-digit", minute: "2-digit", hour12: false });
  };

  function renderStations(stations) {
    const chart = chartFor("station-chart");
    if (!chart) return;
    const thresholds = window.ScreenConfig.loadThresholds;
    chart.setOption({
      animationDuration: 500,
      tooltip: {
        ...tooltip,
        formatter: ({ data }) => [
          `<strong>${data.name}</strong>`,
          `负载：${data.loadRate.toFixed(1)}%`,
          `空闲：${data.idle}/${data.total} 台`,
          `今日营收：¥${data.revenue.toFixed(2)}`
        ].join("<br>")
      },
      grid: { left: 46, right: 22, top: 30, bottom: 34 },
      xAxis: {
        type: "value",
        name: "经度",
        nameTextStyle: { color: labelColor },
        axisLabel: { color: labelColor, formatter: (value) => value.toFixed(2) },
        axisLine: { lineStyle: { color: axisColor } },
        splitLine: { lineStyle: { color: splitColor } }
      },
      yAxis: {
        type: "value",
        name: "纬度",
        nameTextStyle: { color: labelColor },
        axisLabel: { color: labelColor, formatter: (value) => value.toFixed(2) },
        axisLine: { lineStyle: { color: axisColor } },
        splitLine: { lineStyle: { color: splitColor } }
      },
      series: [{
        type: "effectScatter",
        symbolSize: (value) => Math.max(12, Math.min(28, value[2] / 4)),
        rippleEffect: { scale: 2.4, brushType: "stroke" },
        data: stations.map((station) => ({
          name: station.name,
          value: [station.longitude, station.latitude, station.load_rate],
          loadRate: station.load_rate,
          idle: station.idle_chargers,
          total: station.total_chargers,
          revenue: station.today_revenue,
          itemStyle: {
            color: station.load_rate > thresholds.congested
              ? "#ff5f6d"
              : station.load_rate >= thresholds.busy ? "#ffc857" : "#45e0a8"
          }
        }))
      }]
    }, true);
  }

  function renderLoad(loadSeries) {
    const chart = chartFor("load-chart");
    if (!chart) return;
    const actual = loadSeries.actual ?? [];
    const forecast = loadSeries.forecast ?? [];
    const timeline = [...new Set([...actual, ...forecast].map((item) => item.timestamp))].sort();
    const byTime = (items, key) => {
      const lookup = new Map(items.map((item) => [item.timestamp, Number(item[key])]));
      return timeline.map((time) => lookup.has(time) ? lookup.get(time) : null);
    };
    chart.setOption({
      tooltip: { ...tooltip, trigger: "axis" },
      legend: { data: ["实际负荷", "预测负荷"], right: 12, textStyle: { color: labelColor, fontSize: 10 } },
      grid: { left: 54, right: 22, top: 42, bottom: 35 },
      xAxis: {
        type: "category",
        data: timeline.map(formatTime),
        boundaryGap: false,
        axisLabel: { color: labelColor },
        axisLine: { lineStyle: { color: axisColor } }
      },
      yAxis: {
        type: "value",
        name: "kW",
        nameTextStyle: { color: labelColor },
        axisLabel: { color: labelColor },
        splitLine: { lineStyle: { color: splitColor } }
      },
      series: [
        {
          name: "实际负荷",
          type: "line",
          data: byTime(actual, "value_kw"),
          smooth: 0.25,
          showSymbol: false,
          lineStyle: { width: 3, color: "#34d9f4" },
          areaStyle: { color: "rgba(52, 217, 244, 0.10)" }
        },
        {
          name: "预测负荷",
          type: "line",
          data: byTime(forecast, "value_kw"),
          smooth: 0.25,
          showSymbol: false,
          connectNulls: true,
          lineStyle: { width: 2, type: "dashed", color: "#8e7dff" }
        }
      ]
    }, true);
  }

  function renderUtilization(items) {
    const chart = chartFor("utilization-chart");
    if (!chart) return;
    const sorted = [...items]
      .filter((item) => Number.isFinite(Number(item.utilization_rate)))
      .sort((a, b) => Number(b.utilization_rate) - Number(a.utilization_rate))
      .slice(0, 10)
      .reverse();
    chart.setOption({
      tooltip: { ...tooltip, trigger: "axis", axisPointer: { type: "shadow" } },
      grid: { left: 94, right: 28, top: 8, bottom: 24 },
      xAxis: {
        type: "value",
        max: 100,
        axisLabel: { color: labelColor, formatter: "{value}%" },
        splitLine: { lineStyle: { color: splitColor } }
      },
      yAxis: {
        type: "category",
        data: sorted.map((item) => item.station_name),
        axisLabel: { color: labelColor, width: 82, overflow: "truncate" },
        axisLine: { show: false },
        axisTick: { show: false }
      },
      series: [{
        type: "bar",
        data: sorted.map((item) => Number(item.utilization_rate)),
        barWidth: 8,
        itemStyle: {
          borderRadius: 6,
          color: new window.echarts.graphic.LinearGradient(0, 0, 1, 0, [
            { offset: 0, color: "#2779ff" },
            { offset: 1, color: "#34d9f4" }
          ])
        }
      }]
    }, true);
  }

  function renderUsers(items) {
    const chart = chartFor("user-chart");
    if (!chart) return;
    chart.setOption({
      tooltip: { ...tooltip, trigger: "axis" },
      grid: { left: 40, right: 14, top: 18, bottom: 28 },
      xAxis: {
        type: "category",
        data: items.map((item) => item.date),
        boundaryGap: false,
        axisLabel: { color: labelColor, fontSize: 9 },
        axisLine: { lineStyle: { color: axisColor } }
      },
      yAxis: {
        type: "value",
        axisLabel: { color: labelColor, fontSize: 9 },
        splitLine: { lineStyle: { color: splitColor } }
      },
      series: [{
        type: "line",
        data: items.map((item) => Number(item.new_users) || 0),
        smooth: true,
        symbolSize: 5,
        lineStyle: { color: "#45e0a8", width: 2 },
        itemStyle: { color: "#45e0a8" },
        areaStyle: { color: "rgba(69, 224, 168, 0.10)" }
      }]
    }, true);
  }

  function renderEnergy(energy) {
    const chart = chartFor("energy-chart");
    if (!chart) return;
    chart.setOption({
      tooltip: { ...tooltip, trigger: "item", formatter: "{b}<br>{c} kWh · {d}%" },
      legend: { bottom: 0, textStyle: { color: labelColor, fontSize: 10 } },
      series: [{
        type: "pie",
        radius: ["42%", "70%"],
        center: ["50%", "45%"],
        label: { color: "#cce7f8", fontSize: 10, formatter: "{b}\n{d}%" },
        itemStyle: { borderColor: "#ffffff", borderWidth: 3 },
        data: [
          { name: "谷时", value: energy.valley, itemStyle: { color: "#45e0a8" } },
          { name: "平时", value: energy.flat, itemStyle: { color: "#4a86ff" } },
          { name: "峰时", value: energy.peak, itemStyle: { color: "#ff8b5f" } }
        ]
      }]
    }, true);
  }

  let resizeTimer = null;
  window.addEventListener("resize", () => {
    window.clearTimeout(resizeTimer);
    resizeTimer = window.setTimeout(() => instances.forEach((chart) => chart.resize()), 120);
  });

  window.ScreenCharts = {
    render(data) {
      renderStations(data.stations);
      renderLoad(data.load_series);
      renderUtilization(data.utilization_rank);
      renderUsers(data.user_growth);
      renderEnergy(data.energy_by_price_level);
    },
    resize() {
      instances.forEach((chart) => chart.resize());
    },
    dispose() {
      instances.forEach((chart) => chart.dispose());
      instances.clear();
    }
  };
}());
