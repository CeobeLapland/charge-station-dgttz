(function () {
  "use strict";

  const listeners = new Set();
  let state = {
    data: window.ScreenAdapter.normalizeSnapshot({}),
    connection: "offline",
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
      update({ data: { ...data, alarms }, lastUpdated: new Date() });
      return;
    }

    if (message.type === "push.order_event") {
      const event = window.ScreenAdapter.normalizeEvent(message.payload?.event ?? message.payload);
      const events = [event, ...data.events.filter((item) => item.id !== event.id)]
        .slice(0, window.ScreenConfig.maxEvents);
      update({ data: { ...data, events }, lastUpdated: new Date() });
      return;
    }

    if (message.type === "push.forecast") {
      const forecast = window.ScreenAdapter.normalizeForecastSeries(message.payload?.series);
      update({
        data: { ...data, load_series: { ...data.load_series, forecast } },
        lastUpdated: new Date()
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
          lastUpdated: new Date()
        });
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
    setError(error) { update({ error: error ? String(error.message ?? error) : null }); }
  };
}());
