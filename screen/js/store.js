(function () {
  "use strict";

  const listeners = new Set();
  let state = {
    data: window.ScreenAdapter.normalizeSnapshot({}),
    connection: "offline",
    stale: false,
    lastUpdated: null,
    error: null
  };

  const emit = () => listeners.forEach((listener) => listener(state));
  const update = (patch) => {
    state = { ...state, ...patch };
    emit();
  };

  function setSnapshot(payload) {
    update({
      data: window.ScreenAdapter.normalizeSnapshot(payload),
      lastUpdated: new Date(),
      stale: false,
      error: null
    });
  }

  function applyMessage(message) {
    const data = state.data;

    if (message.type === "screen.snapshot_resp") {
      if (message.code !== undefined && message.code !== 0) {
        throw new Error(message.message || `服务端返回错误码 ${message.code}`);
      }
      setSnapshot(message.payload);
      return;
    }

    if (message.type === "push.alarm") {
      const alarm = window.ScreenAdapter.normalizeAlarm(message.payload?.alarm ?? message.payload);
      const alarms = [alarm, ...data.alarms.filter((item) => item.id !== alarm.id)]
        .slice(0, window.ScreenConfig.maxAlarms);
      const alarmEvent = window.ScreenAdapter.normalizeEvent({
        id: `alarm-${alarm.id}`,
        event_time: alarm.occur_time,
        text: `${alarm.station_name}${alarm.charger_id == null ? "" : ` · 桩 ${alarm.charger_id}`}发生${alarm.type}告警`
      });
      const events = [alarmEvent, ...data.events.filter((item) => item.id !== alarmEvent.id)]
        .slice(0, window.ScreenConfig.maxEvents);
      update({ data: { ...data, alarms, events }, lastUpdated: new Date(), stale: false });
      return;
    }

    if (message.type === "push.order_event") {
      const event = window.ScreenAdapter.normalizeEvent(message.payload?.event ?? message.payload);
      const events = [event, ...data.events.filter((item) => item.id !== event.id)]
        .slice(0, window.ScreenConfig.maxEvents);
      update({ data: { ...data, events }, lastUpdated: new Date(), stale: false });
      return;
    }

    if (message.type === "push.forecast") {
      const forecast = window.ScreenAdapter.normalizeForecastSeries(message.payload?.series);
      update({
        data: { ...data, load_series: { ...data.load_series, forecast } },
        lastUpdated: new Date(), stale: false
      });
      return;
    }

    if (message.type === "push.charger_status") {
      // 目前协议未说明该推送是否携带聚合指标。若有则局部更新；否则由连接层节流重拉快照。
      if (message.payload?.metrics) {
        update({
          data: {
            ...data,
            metrics: {
              ...data.metrics,
              ...window.ScreenAdapter.normalizePartialMetrics(message.payload.metrics)
            }
          },
          lastUpdated: new Date(), stale: false
        });
      }
      const chargerId = message.payload?.charger_id;
      if (chargerId != null && message.payload?.status) {
        const statusLabels = { idle: "空闲", charging: "充电中", reserved: "已预约", fault: "故障", offline: "离线", rebooting: "重启中" };
        const event = window.ScreenAdapter.normalizeEvent({
          id: `charger-${chargerId}-${message.payload.status}-${Date.now()}`,
          event_time: new Date().toISOString(),
          text: `电桩 ${chargerId} 状态更新为${statusLabels[message.payload.status] ?? "未知"}`
        });
        const events = [event, ...state.data.events].slice(0, window.ScreenConfig.maxEvents);
        update({ data: { ...state.data, events }, lastUpdated: new Date(), stale: false });
      }
    }
  }

  window.ScreenStore = {
    subscribe(listener) {
      listeners.add(listener);
      listener(state);
      return () => listeners.delete(listener);
    },
    getState: () => state,
    setSnapshot,
    applyMessage,
    setConnection(connection) { update({ connection }); },
    setStale(stale) {
      const next = Boolean(stale);
      if (state.stale !== next) update({ stale: next });
    },
    setError(error) { update({ error: error ? String(error.message ?? error) : null }); }
  };
}());
