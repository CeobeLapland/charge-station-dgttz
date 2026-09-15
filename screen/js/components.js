(function () {
    "use strict";
    const $ = id => document.getElementById(id), fmt = (v, d = 0) => Number(v ?? 0).toLocaleString("zh-CN", { maximumFractionDigits: d });
    class MetricCards {
        constructor() { this._vals = {} }
        render(m) {
            const defs = [
                ["充电站总数", "station_count", "座", 0],
                ["充电桩总数", "charger_count", "台", 0],
                ["在线桩数", "online_charger_count", "台", 0],
                ["今日充电量", "today_energy_kwh", "kWh", 1],
                ["今日订单数", "today_orders", "笔", 0],
                ["今日收入", "today_revenue", "元", 2]]
            const that = this;
            $("metric-cards").replaceChildren(...defs.map(([n, k, u, d]) => {
                const a = document.createElement("article")
                a.className = "metric-card";
                const l = document.createElement("span"), v = document.createElement("strong"),
                    s = document.createElement("small");
                l.className = "metric-label";
                v.className = "metric-value";
                s.className = "metric-unit";
                l.textContent = n;
                s.textContent = u;
                // 数字滚动动画：从旧值缓动到新值
                const target = Number(m[k] ?? 0), start = that._vals[k] ?? 0;
                that._vals[k] = target;
                v.textContent = fmt(start, d);
                const t0 = performance.now(), dur = 750;
                (function step(t) {
                    const p = Math.min(1, (t - t0) / dur), eased = 1 - Math.pow(1 - p, 3);
                    v.textContent = fmt(start + (target - start) * eased, d);
                    if (p < 1) requestAnimationFrame(step)
                })(t0);
                a.append(l, v, s);
                return a
            }))
        }
    }
    class StatusPieChart {
        render(d) { ScreenCharts.status(d) }
    }
    class OrderTrendChart {
        render(d) { ScreenCharts.orders(d) }
    } class EnergyRevenueChart { render(d) { ScreenCharts.energyRevenue(d) } } class StationRankChart { render(d) { ScreenCharts.rank(d) } } class StationMap { render(d) { ScreenCharts.map(d) } }
    class AlarmList {
        render(items) {
            const el = $("alarm-list");
            el.replaceChildren();
            if (!items.length) { const e = document.createElement("div"); e.className = "empty"; e.textContent = "暂无告警"; el.append(e); return } items.forEach(a => { const r = document.createElement("div"); r.className = `alarm-row alarm-${a.level}`; const t = document.createElement("time"), target = document.createElement("span"), text = document.createElement("span"); t.textContent = a.occur_time.slice(11); target.className = "alarm-target"; target.textContent = `${a.station_name} / ${a.charger_code}`; text.className = "alarm-text"; text.textContent = a.content; r.append(t, target, text); el.append(r) })
        }
    }
    class FilterPanel {
        constructor(onSubmit) {
            this.onSubmit = onSubmit;
            $("filter-submit").onclick = () => this.submit();
            $("filter-reset").onclick = () => { ["filter-date", "filter-region", "filter-station"].forEach(id => $(id).value = ""); this.submit() }
        } submit() { this.onSubmit({ date: $("filter-date").value, region: $("filter-region").value, station_id: $("filter-station").value }) } render(options) { const fill = (id, items, label, value) => { const el = $(id), current = el.value; el.replaceChildren(new Option(label, ""), ...items.map(x => new Option(x.name ?? x, x.id ?? x))); el.value = current }; fill("filter-region", options.regions, "全部区域"); fill("filter-station", options.stations, "全部电站") }
    }
    class EventList {
        render(items) {
            const el = $("event-list");
            el.replaceChildren();
            if (!items.length) {
                const e = document.createElement("div");
                e.className = "empty";
                e.textContent = "暂无事件";
                el.append(e);
                return
            }
            items.forEach(ev => {
                const r = document.createElement("div");
                r.className = "alarm-row";
                const t = document.createElement("time"), text = document.createElement("span");
                t.textContent = (ev.occur_time ?? "").slice(11);
                text.className = "alarm-text";
                text.textContent = `${ev.type === "alarm" ? "[告警]" : "[订单]"} ${ev.content}`;
                r.append(t, text);
                el.append(r)
            })
        }
    }
    class UserTrendChart {
        render(d) {
            ScreenCharts.userTrend(d)
        }
    }
    class PeakValleyChart {
        render(d) {
            ScreenCharts.peakValley(d)
        }
    } class CarbonChart { render(d) { ScreenCharts.carbon(d) } }
    class MainDashboard {
        constructor(onFilter) {
            this.metrics = new MetricCards;
            this.status = new StatusPieChart;
            this.orders = new OrderTrendChart;
            this.energy = new EnergyRevenueChart;
            this.rank = new StationRankChart;
            this.map = new StationMap;
            this.alarms = new AlarmList;
            this.filters = new FilterPanel(onFilter);
            this.userTrend = new UserTrendChart;
            this.peakValley = new PeakValleyChart;
            this.carbon = new CarbonChart;
            this.events = new EventList
        } render(data) {
            this.metrics.render(data.metrics);
            this.status.render(data.status_distribution);
            this.orders.render(data.order_trend);
            this.energy.render(data.energy_revenue_trend);
            this.rank.render(data.station_rank);
            this.map.render(data.stations);
            this.alarms.render(data.alarms);
            this.filters.render(data.filter_options);
            this.userTrend.render(data.user_trend || []);
            this.peakValley.render(data.peak_valley || { valley: 0, flat: 0, peak: 0 });
            this.carbon.render(data.carbon || { green_index: 0 });
            this.events.render(data.events || [])
        }
    }
    window.DashboardComponents = {
        MainDashboard, MetricCards, StatusPieChart, OrderTrendChart, EnergyRevenueChart, StationRankChart, StationMap, AlarmList, FilterPanel, UserTrendChart, PeakValleyChart, CarbonChart, EventList
    };
}());
