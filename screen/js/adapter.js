(function () {
  "use strict";

  const numberOr = (value, fallback = 0) => Number.isFinite(Number(value)) ? Number(value) : fallback;
  const nullableNumber = (value) => Number.isFinite(Number(value)) ? Number(value) : null;
  const arrayOrEmpty = (value) => Array.isArray(value) ? value : [];
  const metricKeys = ["today_revenue", "today_orders", "charging_count", "online_rate", "revenue_change_pct", "orders_change_pct", "current_power_kw", "active_station_count"];

  const normalizeMetrics = (metrics = {}) => Object.fromEntries(metricKeys.map((key) => [key, nullableNumber(metrics[key])]));
  const normalizePartialMetrics = (metrics = {}) => Object.fromEntries(metricKeys.filter((key) => Number.isFinite(Number(metrics[key]))).map((key) => [key, Number(metrics[key])]));
  const normalizePowerSeries = (items) => arrayOrEmpty(items).map((item) => ({
    timestamp: item.timestamp ?? item.measure_time ?? null,
    value_kw: numberOr(item.value_kw ?? item.power_kw ?? item.value),
    active_station_count: nullableNumber(item.active_station_count ?? item.active_stations)
  })).filter((item) => item.timestamp);

  const categoryFrom = (event = {}) => {
    const explicit = event.category;
    if (["system", "user", "hardware", "station", "alarm"].includes(explicit)) return explicit;
    const type = String(event.event_type ?? event.type ?? "");
    if (type.includes("alarm") || type.includes("fault") || type.includes("warning")) return "alarm";
    if (type.startsWith("order_") || type.startsWith("user_")) return "user";
    if (type.startsWith("charger_") || type.startsWith("device_")) return "hardware";
    if (type.startsWith("station_")) return "station";
    return "system";
  };

  const normalizeAlarm = (alarm = {}) => ({
    id: alarm.id ?? `${alarm.station_id ?? "unknown"}-${alarm.occur_time ?? Date.now()}`,
    station_id: alarm.station_id ?? null,
    station_name: String(alarm.station_name ?? "未知站点"),
    charger_id: alarm.charger_id ?? null,
    type: String(alarm.type ?? "unknown"),
    level: ["info", "warning", "critical"].includes(alarm.level) ? alarm.level : "unknown",
    occur_time: alarm.occur_time ?? new Date().toISOString()
  });

  const normalizeEvent = (event = {}) => ({
    id: event.id ?? `${event.event_time ?? Date.now()}-${event.text ?? "event"}`,
    event_time: event.event_time ?? event.occur_time ?? new Date().toISOString(),
    event_type: String(event.event_type ?? event.type ?? "system_event"),
    category: categoryFrom(event),
    text: String(event.text ?? event.content ?? "收到新的平台事件"),
    target: String(event.target ?? event.station_name ?? event.charger_code ?? "平台")
  });

  const alarmAsEvent = (alarm) => normalizeEvent({
    id: `alarm-${alarm.id}`,
    event_time: alarm.occur_time,
    event_type: `alarm_${alarm.type}`,
    category: "alarm",
    text: `${alarm.station_name}${alarm.charger_id == null ? "" : ` · 桩 ${alarm.charger_id}`}发生告警`,
    target: alarm.charger_id == null ? alarm.station_name : `电桩 ${alarm.charger_id}`
  });

  function normalizeSnapshot(payload = {}) {
    const stations = arrayOrEmpty(payload.stations).map((station) => ({
      id: station.id ?? station.station_id ?? null,
      name: String(station.name ?? station.station_name ?? "未命名站点"),
      longitude: numberOr(station.longitude), latitude: numberOr(station.latitude),
      load_rate: numberOr(station.load_rate ?? station.utilization_rate),
      idle_chargers: numberOr(station.idle_chargers), total_chargers: numberOr(station.total_chargers),
      today_revenue: numberOr(station.today_revenue),
      power_kw: nullableNumber(station.power_kw ?? station.current_power_kw),
      power_series: normalizePowerSeries(station.power_series)
    }));
    const actual = normalizePowerSeries(payload.load_series?.actual ?? payload.power_series);
    const metrics = normalizeMetrics(payload.metrics ?? payload);
    if (metrics.current_power_kw == null && actual.length) metrics.current_power_kw = actual.at(-1).value_kw;
    if (metrics.active_station_count == null) metrics.active_station_count = stations.filter((station) => station.load_rate > 0).length;
    const alarms = arrayOrEmpty(payload.alarms).map(normalizeAlarm);
    const events = arrayOrEmpty(payload.events).map(normalizeEvent);
    const eventIds = new Set(events.map((event) => event.id));
    alarms.map(alarmAsEvent).forEach((event) => { if (!eventIds.has(event.id)) events.push(event); });
    events.sort((a, b) => new Date(b.event_time) - new Date(a.event_time));
    return {
      metrics, stations, load_series: { actual },
      utilization_rank: arrayOrEmpty(payload.utilization_rank), alarms,
      user_growth: arrayOrEmpty(payload.user_growth),
      energy_by_price_level: {
        valley: numberOr(payload.energy_by_price_level?.valley),
        flat: numberOr(payload.energy_by_price_level?.flat),
        peak: numberOr(payload.energy_by_price_level?.peak)
      },
      events
    };
  }

  function normalizeMessage(rawMessage) {
    const message = typeof rawMessage === "string" ? JSON.parse(rawMessage) : rawMessage;
    if (!message || typeof message !== "object" || typeof message.type !== "string") throw new Error("服务端消息缺少合法的 type");
    return message;
  }

  window.ScreenAdapter = { normalizeSnapshot, normalizeMessage, normalizeAlarm, normalizeEvent, alarmAsEvent, normalizeMetrics, normalizePartialMetrics, normalizePowerSeries };
}());
