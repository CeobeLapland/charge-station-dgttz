(function () {
  "use strict";

  const instances = new Map();
  const axisColor = "#e3e7eb";
  const labelColor = "#788596";
  const splitColor = "#f5f6f8";

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
    backgroundColor: "rgba(255, 255, 255, 0.98)",
    borderColor: "#dfe5ed",
    textStyle: { color: "#263548", fontSize: 11 },
    extraCssText: "box-shadow:0 8px 24px rgba(31,45,61,.12);border-radius:4px"
  };

  const escapeHtml = (value) => String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");

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
          `<strong>${escapeHtml(data.name)}</strong>`,
          `负载：${data.loadRate.toFixed(1)}%`,
          `空闲：${data.idle}/${data.total} 台`,
          `今日营收：¥${data.revenue.toFixed(2)}`
        ].join("<br>")
      },
      grid: { left: 50, right: 16, top: 18, bottom: 28 },
      xAxis: {
        type: "value",
        scale: true,
        min: ({ min }) => min - 0.01,
        max: ({ max }) => max + 0.01,
        name: "经度",
        nameTextStyle: { color: labelColor },
        axisLabel: { color: labelColor, formatter: (value) => value.toFixed(2) },
        axisLine: { lineStyle: { color: axisColor } },
        splitLine: { lineStyle: { color: splitColor } }
      },
      yAxis: {
        type: "value",
        scale: true,
        min: ({ min }) => min - 0.01,
        max: ({ max }) => max + 0.01,
        name: "纬度",
        nameTextStyle: { color: labelColor },
        axisLabel: { color: labelColor, formatter: (value) => value.toFixed(2) },
        axisLine: { lineStyle: { color: axisColor } },
        splitLine: { lineStyle: { color: splitColor } }
      },
      series: [{
        type: "scatter",
        symbolSize: (value) => Math.max(11, Math.min(22, value[2] / 5)),
        data: stations.map((station) => ({
          name: station.name,
          value: [station.longitude, station.latitude, station.load_rate],
          loadRate: station.load_rate,
          idle: station.idle_chargers,
          total: station.total_chargers,
          revenue: station.today_revenue,
          itemStyle: {
            color: station.load_rate > thresholds.congested
              ? "#b86770"
              : station.load_rate >= thresholds.busy ? "#8697aa" : "#557da4"
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
      legend: { data: ["实际负荷", "预测负荷"], right: 4, top: 2, itemWidth: 14, itemHeight: 6, textStyle: { color: labelColor, fontSize: 9 } },
      grid: { left: 48, right: 10, top: 26, bottom: 22 },
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
          name: "置信区间下界",
          type: "line",
          data: byTime(forecast, "lower_kw"),
          stack: "confidence",
          symbol: "none",
          lineStyle: { opacity: 0 },
          tooltip: { show: false },
          emphasis: { disabled: true }
        },
        {
          name: "预测置信区间",
          type: "line",
          data: timeline.map((time) => {
            const point = forecast.find((item) => item.timestamp === time);
            return point ? Math.max(0, Number(point.upper_kw) - Number(point.lower_kw)) : null;
          }),
          stack: "confidence",
          symbol: "none",
          lineStyle: { opacity: 0 },
          areaStyle: { color: "rgba(83, 119, 155, .08)" },
          tooltip: { show: false },
          emphasis: { disabled: true }
        },
        {
          name: "实际负荷",
          type: "line",
          data: byTime(actual, "value_kw"),
          smooth: 0.25,
          showSymbol: false,
          lineStyle: { width: 2, color: "#4f789f" }
        },
        {
          name: "预测负荷",
          type: "line",
          data: byTime(forecast, "value_kw"),
          smooth: 0.25,
          showSymbol: false,
          connectNulls: true,
          lineStyle: { width: 1.5, type: "dashed", color: "#8c9bac" }
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
      grid: { left: 132, right: 18, top: 8, bottom: 22 },
      xAxis: {
        type: "value",
        max: 100,
        axisLabel: { color: labelColor, formatter: "{value}%" },
        splitLine: { lineStyle: { color: splitColor } }
      },
      yAxis: {
        type: "category",
        data: sorted.map((item) => item.station_name),
        axisLabel: { color: labelColor, width: 116, overflow: "truncate", ellipsis: "…" },
        axisLine: { show: false },
        axisTick: { show: false }
      },
      series: [{
        type: "bar",
        data: sorted.map((item) => Number(item.utilization_rate)),
        barWidth: 8,
        itemStyle: {
          borderRadius: 2,
          color: "#6687a8"
        }
      }]
    }, true);
  }

  function renderUsers(items) {
    const chart = chartFor("user-chart");
    if (!chart) return;
    chart.setOption({
      tooltip: { ...tooltip, trigger: "axis" },
      grid: { left: 42, right: 12, top: 12, bottom: 24 },
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
        lineStyle: { color: "#587fa5", width: 2 },
        itemStyle: { color: "#587fa5" }
      }]
    }, true);
  }

  function renderEnergy(energy) {
    const chart = chartFor("energy-chart");
    if (!chart) return;
    chart.setOption({
      tooltip: { ...tooltip, trigger: "item", formatter: "{b}<br>{c} kWh · {d}%" },
      legend: { bottom: 0, itemWidth: 12, itemHeight: 7, textStyle: { color: labelColor, fontSize: 10 } },
      series: [{
        type: "pie",
        radius: ["50%", "66%"],
        center: ["50%", "43%"],
        label: { color: "#697586", fontSize: 10, formatter: "{b}  {d}%" },
        labelLine: { length: 8, length2: 5, lineStyle: { color: "#d8dde3" } },
        itemStyle: { borderColor: "#ffffff", borderWidth: 2 },
        data: [
          { name: "谷时", value: energy.valley, itemStyle: { color: "#c6d0da" } },
          { name: "平时", value: energy.flat, itemStyle: { color: "#91a6ba" } },
          { name: "峰时", value: energy.peak, itemStyle: { color: "#587fa5" } }
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
