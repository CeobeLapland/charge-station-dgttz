(function () {
    "use strict";
    const pool = new Map(), blue = "#557da4", red = "#aa6269", gray = "#9aa8b5", axis = "#edf0f3", label = "#7b8794";
    const get = id => {
        const el = document.getElementById(id);
        if (!el || !window.echarts) return null;
        if (!pool.has(id)) pool.set(id, echarts.init(el));
        return pool.get(id)
    };
    const base = {
        textStyle: { color: label }, tooltip: {
            trigger: "axis", backgroundColor: "#fff", borderColor: "#ddd", textStyle: {
                color: "#333"
            }
        }, grid: { left: 46, right: 18, top: 20, bottom: 28 }, xAxis: {
            axisLine: { lineStyle: { color: axis } }, axisLabel: {
                color: label, fontSize: 10
            }
        }, yAxis: {
            axisLine: { show: false }, axisLabel: { color: label, fontSize: 10 }, splitLine: {
                lineStyle: { color: "#f3f5f7" }
            }
        }
    };
    const line = (id, dates, series, yName) => get(id)?.setOption({
        ...base, xAxis: { ...base.xAxis, type: "category", data: dates, boundaryGap: false },
        yAxis: { ...base.yAxis, type: "value", name: yName, nameTextStyle: { color: label } }, legend: {
            right: 4, top: 0, itemWidth: 12,
            textStyle: { fontSize: 9, color: label }
        }, series
    }, true);
    window.ScreenCharts = {
        status(data) {
            get("status-chart")?.setOption({
                tooltip: { trigger: "item" }, legend: {
                    bottom: 0, textStyle: { fontSize: 10, color: label }
                }, series: [{
                    type: "pie", radius: ["50%", "68%"], center: ["50%", "43%"],
                    label: { formatter: "{b} {c}", fontSize: 10 },
                    data: [{ name: "空闲", value: data.idle, itemStyle: { color: blue } }, { name: "充电中", value: data.charging, itemStyle: { color: "#7898b6" } }, { name: "离线", value: data.offline, itemStyle: { color: gray } }, { name: "故障", value: data.fault, itemStyle: { color: red } }]
                }]
            }, true)
        }, orders(items) {
            line("order-trend-chart", items.map(x => x.date), [{
                name: "订单数", type: "line", data: items.map(x => x.order_count),
                showSymbol: false, lineStyle: { color: blue, width: 2 }
            }], "笔")
        }, energyRevenue(items) {
            items = items || [];
            const dates = items.map(x => x.date);
            get("energy-revenue-chart")?.setOption({
                ...base,
                xAxis: {
                    ...base.xAxis, type: "category",
                    data: dates, boundaryGap: false
                },
                yAxis: [{
                    type: "value", name: "kWh",
                    axisLine: { show: false },
                    axisLabel: { color: label, fontSize: 10 },
                    splitLine: { lineStyle: { color: "#f3f5f7" } }
                },
                {
                    type: "value", name: "元",
                    splitLine: { show: false },
                    axisLabel: { color: label, fontSize: 10 }
                }],
                legend: {
                    right: 4, top: 0, itemWidth: 12,
                    textStyle: { fontSize: 9, color: label }
                },
                series: [{ name: "充电量(kWh)", type: "bar", data: items.map(x => x.energy_kwh), itemStyle: { color: "#8ca5bb" }, barWidth: 14 }, { name: "收入(元)", type: "line", yAxisIndex: 1, data: items.map(x => x.revenue), showSymbol: false, lineStyle: { color: red, width: 2 } }]
            }, true)
        }, rank(items) {
            const a = [...items].slice(0, 5).reverse();
            get("station-rank-chart")?.setOption({
                ...base,
                grid: {
                    left: 115, right: 18, top: 12, bottom: 20
                },
                xAxis: { ...base.yAxis, type: "value" },
                yAxis: {
                    ...base.xAxis, type: "category",
                    data: a.map(x => x.station_name),
                    axisLabel: {
                        color: label, width: 100,
                        overflow: "truncate"
                    }
                },
                series: [{
                    type: "bar",
                    data: a.map(x => x.today_energy_kwh ?? x.today_orders),
                    barWidth: 9, itemStyle: { color: blue }
                }]
            }, true)
        }, map(items) { get("station-map")?.setOption({ ...base, tooltip: { trigger: "item", formatter: p => `${p.data.name}<br>状态：${p.data.status}` }, xAxis: { ...base.xAxis, type: "value", scale: true, name: "经度" }, yAxis: { ...base.yAxis, type: "value", scale: true, name: "纬度" }, series: [{ type: "scatter", symbolSize: 13, data: items.map(x => ({ name: x.name, status: x.status_text ?? x.status ?? "未知", value: [x.longitude, x.latitude] })), itemStyle: { color: blue } }] }, true) }, userTrend(items) { line("user-trend-chart", items.map(x => x.date), [{ name: "新增用户", type: "line", areaStyle: { color: "rgba(85,125,164,0.2)" }, data: items.map(x => x.user_count), showSymbol: true, lineStyle: { color: blue, width: 2 } }], "人") }, peakValley(pv) {
            const labels = ["谷时", "平时", "峰时"], vals = [pv.valley, pv.flat, pv.peak];
            get("peak-valley-chart")?.setOption({
                tooltip: { trigger: "item" },
                legend: { bottom: 0, textStyle: { fontSize: 10, color: label } },
                series: [{
                    type: "pie", radius: ["45%", "65%"], center: ["50%", "43%"],
                    label: { formatter: "{b} {c} kWh", fontSize: 10 },
                    data: [{
                        name: "谷时", value: vals[0],
                        itemStyle: { color: "#6f9f7f" }
                    },
                    {
                        name: "平时", value: vals[1],
                        itemStyle: { color: blue }
                    },
                    { name: "峰时", value: vals[2], itemStyle: { color: red } }]
                }]
            }, true)
        }, carbon(c) { get("carbon-chart")?.setOption({ series: [{ type: "gauge", center: ["50%", "55%"], min: 0, max: 100, radius: "80%", progress: { show: true, width: 14, itemStyle: { color: "#6f9f7f" } }, axisLine: { lineStyle: { width: 14 } }, axisLabel: { distance: 20, fontSize: 9, color: label }, pointer: { show: false }, data: [{ value: c.green_index, name: "指数", detail: { fontSize: 18, color: "#263443", formatter: "{value}" } }], detail: { fontSize: 18, color: "#263443" }, title: { fontSize: 10, color: label } }] }, true) }, forecast(fc) {
            if (!fc || fc.error) { return }
            const labels = [...fc.hours.map(h => String(h).padStart(2, "0") + ":00")]
            const c = get("forecast-chart");
            c?.setOption({
                tooltip: {
                    trigger: "axis", backgroundColor: "#fff", borderColor: "#ddd", textStyle: { color: "#333" }
                }, legend: { right: 4, top: 0, itemWidth: 12, textStyle: { fontSize: 9, color: label } },
                grid: { left: 46, right: 18, top: 24, bottom: 26 }, xAxis: { type: "category", data: labels, axisLabel: { color: label, fontSize: 10 } }, yAxis: { type: "value", name: "kW", nameTextStyle: { color: label }, axisLine: { show: false }, axisLabel: { color: label, fontSize: 10 }, splitLine: { lineStyle: { color: "#f3f5f7" } } }, series: [{
                    name: "预测区间", type: "line", data: fc.upper.map((u, i) => [i, u]), markArea: undefined, lineStyle: { opacity: 0 },
                    stack: "conf", symbol: "none", areaStyle: { color: "rgba(120,152,182,0.25)" }, tooltip: { show: false }
                }, {
                    name: "置信下限", type: "line", data: fc.lower.map((l, i) => [i, l]),
                    stack: "conf", symbol: "none", lineStyle: { opacity: 0 }, tooltip: { show: false }
                }, { name: "历史实际(均值)", type: "line", data: fc.actual.map((a, i) => [i, a]), showSymbol: false, lineStyle: { color: "#557da4", width: 2 } }, { name: "未来预测", type: "line", data: fc.forecast.map((f, i) => [i, f]), showSymbol: false, lineStyle: { color: "#aa6269", width: 2, type: "dashed" } }]
            }, true)
            const el = document.getElementById("forecast-chart");
            /* 置信区间用两条stack线在图表上叠加阴影(简化:用第二条覆盖) */
        },
        demandHeat(rows) {
            rows = rows || [];
            const yData = ["周一", "周二", "周三", "周四", "周五", "周六", "周日"];
            const yIdx = { 2: 0, 3: 1, 4: 2, 5: 3, 6: 4, 7: 5, 1: 6 };
            const hours = Array.from({ length: 24 }, (_, h) => h + "时");
            const data = rows.filter(x => yIdx[x.dow] != null && x.hour >= 0 && x.hour <= 23).map(x => [x.hour, yIdx[x.dow], x.value || 0]); const max = Math.max(1, ...data.map(d => d[2])); get("demand-heat-chart")?.setOption({
                tooltip: {
                    position: "top", backgroundColor: "#fff", borderColor: "#ddd",
                    textStyle: { color: "#333" }, formatter: p => { const idx = p.data[1]; return `${yData[idx]} ${p.data[0]}时<br>订单需求：${p.data[2]}` }
                }, grid: { left: 40, right: 56, top: 8, bottom: 46 }, xAxis: { type: "category", data: hours, axisLabel: { color: label, fontSize: 9, interval: 2 }, splitArea: { show: true } }, yAxis: {
                    type: "category", data: yData,
                    axisLabel: { color: label, fontSize: 10 }, splitArea: { show: true }
                }, visualMap: {
                    min: 0, max: max, calculable: false, orient: "horizontal",
                    left: "center", bottom: 2, itemWidth: 120,
                    itemHeight: 10, textStyle: { fontSize: 9, color: label }, inRange: { color: ["#eef4fa", "#8ca5bb", "#aa6269"] }
                },
                series: [{
                    type: "heatmap", data: data, label: { show: false },
                    itemStyle: { borderColor: "#fff", borderWidth: 1 }
                }]
            }, true)
        }, healthDist(dist) {
            dist = dist || [0, 0, 0, 0, 0];
            get("health-chart")?.setOption({
                ...base,
                xAxis: {
                    ...base.xAxis, type: "category",
                    data: ["<60", "60-70", "70-80", "80-90", "90-100"],
                    name: "分", nameTextStyle: { color: label }
                },
                yAxis: { ...base.yAxis, type: "value", name: "台" },
                series: [{
                    type: "bar", data: dist, barWidth: 16,
                    itemStyle: { color: "#8ca5bb" },
                    label: { show: true, position: "top", fontSize: 9, color: label }
                }]
            },
                true)
        }, resize() { pool.forEach(c => c.resize()) }
    }; window.addEventListener("resize", () => window.ScreenCharts.resize());
}());
