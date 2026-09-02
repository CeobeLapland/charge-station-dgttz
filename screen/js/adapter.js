(function () {
  "use strict";

  const numberOr = (value, fallback = 0) => {
    const parsed = Number(value);
    return Number.isFinite(parsed) ? parsed : fallback;
  };

  const arrayOrEmpty = (value) => Array.isArray(value) ? value : [];

  const metricKeys = [
    "today_revenue",
    "today_orders",
    "charging_count",
    "online_rate",
    "revenue_change_pct",
    "orders_change_pct"
  ];

  const normalizeMetrics = (metrics = {}) => Object.fromEntries(
    metricKeys.map((key) => [key, numberOr(metrics[key])])
  );

  const normalizePartialMetrics = (metrics = {}) => Object.fromEntries(
    metricKeys
      .filter((key) => Number.isFinite(Number(metrics[key])))
      .map((key) => [key, Number(metrics[key])])
  );

  const normalizeActualSeries = (items) => arrayOrEmpty(items).map((item) => ({
    timestamp: item.timestamp ?? item.measure_time ?? null,
    value_kw: numberOr(item.value_kw ?? item.power_kw ?? item.value)
  }));

  const normalizeForecastSeries = (items) => arrayOrEmpty(items).map((item) => ({
    timestamp: item.timestamp ?? item.forecast_time ?? null,
    value_kw: numberOr(item.value_kw ?? item.predicted_kw ?? item.value),
    lower_kw: numberOr(item.lower_kw ?? item.lower_bound),
    upper_kw: numberOr(item.upper_kw ?? item.upper_bound)
  }));

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
    event_time: event.event_time ?? new Date().toISOString(),
    // 后续服务端如果返回 phone，必须由服务端脱敏或在适配器中显式脱敏。
    text: String(event.text ?? event.content ?? "收到新的平台事件")
  });

  function normalizeSnapshot(payload = {}) {
    // 服务端字段变化时，优先只修改本函数，不改图表与页面组件。
    return {
      metrics: normalizeMetrics(payload.metrics ?? payload),
      stations: arrayOrEmpty(payload.stations).map((station) => ({
        id: station.id ?? station.station_id ?? null,
        name: String(station.name ?? station.station_name ?? "未命名站点"),
        longitude: numberOr(station.longitude),
        latitude: numberOr(station.latitude),
        load_rate: numberOr(station.load_rate ?? station.utilization_rate),
        idle_chargers: numberOr(station.idle_chargers),
        total_chargers: numberOr(station.total_chargers),
        today_revenue: numberOr(station.today_revenue)
      })),
      load_series: {
        actual: normalizeActualSeries(payload.load_series?.actual),
        forecast: normalizeForecastSeries(payload.load_series?.forecast)
      },
      utilization_rank: arrayOrEmpty(payload.utilization_rank),
      alarms: arrayOrEmpty(payload.alarms).map(normalizeAlarm),
      user_growth: arrayOrEmpty(payload.user_growth),
      energy_by_price_level: {
        valley: numberOr(payload.energy_by_price_level?.valley),
        flat: numberOr(payload.energy_by_price_level?.flat),
        peak: numberOr(payload.energy_by_price_level?.peak)
      },
      events: arrayOrEmpty(payload.events).map(normalizeEvent)
    };
  }

  function normalizeMessage(rawMessage) {
    const message = typeof rawMessage === "string" ? JSON.parse(rawMessage) : rawMessage;
    if (!message || typeof message !== "object" || typeof message.type !== "string") {
      throw new Error("服务端消息缺少合法的 type");
    }
    return message;
  }

  window.ScreenAdapter = {
    normalizeSnapshot,
    normalizeMessage,
    normalizeAlarm,
    normalizeEvent,
    normalizeMetrics,
    normalizePartialMetrics,
    normalizeForecastSeries
  };
}());
