(function () {
  "use strict";

  const $ = (id) => document.getElementById(id);
  const alarmLabels = {
    comm_abnormal: "通信异常",
    overheat: "设备温度过高",
    power_drop: "功率异常下降",
    offline: "设备离线",
    user_behavior: "用户行为异常",
    unknown: "未知告警"
  };

  const formatNumber = (value, digits = 0) => Number(value).toLocaleString("zh-CN", {
    minimumFractionDigits: digits,
    maximumFractionDigits: digits
  });

  const formatClock = (date) => date.toLocaleString("zh-CN", {
    month: "2-digit",
    day: "2-digit",
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
    hour12: false
  });

  const formatTime = (value) => {
    const date = new Date(value);
    return Number.isNaN(date.getTime()) ? "--:--:--" : date.toLocaleTimeString("zh-CN", { hour12: false });
  };

  function setText(id, value) {
    const element = $(id);
    if (element) element.textContent = value;
  }

  function renderConnection(state) {
    const element = $("connection-status");
    const labels = {
      live: "实时连接",
      connecting: "正在连接",
      reconnecting: "重新连接中",
      offline: "未连接"
    };
    element.textContent = labels[state.connection] ?? "连接异常";
    element.className = `connection connection-${state.connection === "live" ? "live" : state.connection === "connecting" || state.connection === "reconnecting" ? "connecting" : "offline"}`;

    const modeBadge = $("mode-badge");
    const liveMode = window.ScreenConfig.mode === "live";
    modeBadge.textContent = liveMode ? "真实数据" : "演示数据";
    modeBadge.className = `badge ${liveMode ? "badge-live" : "badge-demo"}`;
  }

  function renderMetrics(metrics) {
    setText("today-revenue", formatNumber(metrics.today_revenue, 2));
    setText("today-orders", formatNumber(metrics.today_orders));
    setText("charging-count", formatNumber(metrics.charging_count));
    setText("online-rate", formatNumber(metrics.online_rate, 1));
    setText("revenue-trend", `较昨日 ${metrics.revenue_change_pct >= 0 ? "+" : ""}${formatNumber(metrics.revenue_change_pct, 1)}%`);
    setText("orders-trend", `较昨日 ${metrics.orders_change_pct >= 0 ? "+" : ""}${formatNumber(metrics.orders_change_pct, 1)}%`);
  }

  function renderAlarms(alarms) {
    const list = $("alarm-list");
    setText("alarm-count", String(alarms.length));
    list.replaceChildren();

    if (!alarms.length) {
      const empty = document.createElement("div");
      empty.className = "empty-state";
      empty.textContent = "当前没有未处理告警";
      list.appendChild(empty);
      return;
    }

    alarms.slice(0, 8).forEach((alarm) => {
      const item = document.createElement("div");
      item.className = `alarm-item alarm-${alarm.level}`;

      const dot = document.createElement("span");
      dot.className = "alarm-dot";

      const content = document.createElement("div");
      const title = document.createElement("strong");
      title.className = "alarm-title";
      title.textContent = alarmLabels[alarm.type] ?? `未知告警（${alarm.type}）`;
      const meta = document.createElement("div");
      meta.className = "alarm-meta";
      meta.textContent = `${alarm.station_name}${alarm.charger_id == null ? "" : ` · 桩 ${alarm.charger_id}`}`;
      content.append(title, meta);

      const time = document.createElement("time");
      time.className = "alarm-time";
      time.textContent = formatTime(alarm.occur_time);
      item.append(dot, content, time);
      list.appendChild(item);
    });
  }

  function renderEvents(events) {
    const stream = $("event-stream");
    stream.replaceChildren();

    if (!events.length) {
      const empty = document.createElement("div");
      empty.className = "empty-state";
      empty.textContent = "暂无实时事件";
      stream.appendChild(empty);
      return;
    }

    events.slice(0, 6).forEach((event) => {
      const item = document.createElement("div");
      item.className = "event-item";
      item.title = event.text;

      const time = document.createElement("span");
      time.className = "event-time";
      time.textContent = formatTime(event.event_time);

      const text = document.createElement("span");
      text.textContent = event.text;
      item.append(time, text);
      stream.appendChild(item);
    });
  }

  function renderError(error) {
    const banner = $("error-banner");
    if (!error) {
      banner.hidden = true;
      banner.textContent = "";
      return;
    }
    banner.textContent = error;
    banner.hidden = false;
  }

  function render(state) {
    renderConnection(state);
    renderMetrics(state.data.metrics);
    renderAlarms(state.data.alarms);
    renderEvents(state.data.events);
    renderError(state.error);
    setText("last-updated", state.lastUpdated ? `最近更新 ${formatClock(state.lastUpdated)}` : "尚未更新");
    window.ScreenCharts.render(state.data);
  }

  function start() {
    window.setInterval(() => setText("clock", formatClock(new Date())), 1000);
    setText("clock", formatClock(new Date()));
    window.ScreenStore.subscribe(render);

    if (window.ScreenConfig.mode === "live") {
      const client = new window.DashboardSocket(window.ScreenConfig, window.ScreenStore);
      client.connect();
      window.addEventListener("beforeunload", () => client.disconnect());
      return;
    }

    window.ScreenStore.setConnection("offline");
    window.ScreenStore.setSnapshot(window.ScreenMockSnapshot);
  }

  start();
}());
