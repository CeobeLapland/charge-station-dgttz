(function () {
  "use strict";
  const $ = (id) => document.getElementById(id);
  const pageNames = new Set(["overview", "stations", "analytics", "events"]);
  const categoryLabels = { system: "系统", user: "用户", hardware: "硬件", station: "电站", alarm: "警告" };
  let selectedStationId = null;
  let stationKeyword = "";
  let eventFilter = "all";

  const formatNumber = (value, digits = 0) => value == null ? "--" : Number(value).toLocaleString("zh-CN", { minimumFractionDigits: digits, maximumFractionDigits: digits });
  const formatClock = (date) => date.toLocaleString("zh-CN", { month: "2-digit", day: "2-digit", hour: "2-digit", minute: "2-digit", second: "2-digit", hour12: false });
  const formatDateTime = (value) => { const date = new Date(value); return Number.isNaN(date.getTime()) ? "--" : date.toLocaleString("zh-CN", { month: "2-digit", day: "2-digit", hour: "2-digit", minute: "2-digit", second: "2-digit", hour12: false }); };
  const setText = (id, value) => { const element = $(id); if (element) element.textContent = value; };

  function renderConnection(state) {
    const element = $("connection-status");
    const effective = state.stale && state.connection === "live" ? "stale" : state.connection;
    const labels = { live: "实时连接", connecting: "正在连接", reconnecting: "重新连接中", offline: "未连接", stale: "数据已过期" };
    element.textContent = labels[effective] ?? "连接异常";
    element.className = `connection connection-${effective === "live" ? "live" : effective === "connecting" || effective === "reconnecting" ? "connecting" : "offline"}`;
  }

  function renderMetrics(metrics) {
    setText("today-revenue", formatNumber(metrics.today_revenue, 2)); setText("today-orders", formatNumber(metrics.today_orders));
    setText("charging-count", formatNumber(metrics.charging_count)); setText("online-rate", formatNumber(metrics.online_rate, 1));
    setText("current-power", formatNumber(metrics.current_power_kw, 1)); setText("active-station-count", formatNumber(metrics.active_station_count));
    setText("revenue-trend", `较昨日 ${metrics.revenue_change_pct >= 0 ? "+" : ""}${formatNumber(metrics.revenue_change_pct, 1)}%`);
    setText("orders-trend", `较昨日 ${metrics.orders_change_pct >= 0 ? "+" : ""}${formatNumber(metrics.orders_change_pct, 1)}%`);
  }

  function renderStations(stations) {
    const filtered = stations.filter((station) => station.name.toLocaleLowerCase("zh-CN").includes(stationKeyword.toLocaleLowerCase("zh-CN")));
    if (selectedStationId == null || !stations.some((station) => station.id === selectedStationId)) selectedStationId = stations[0]?.id ?? null;
    setText("station-result-count", `${filtered.length} 座`);
    const list = $("station-list"); list.replaceChildren();
    filtered.forEach((station) => {
      const button = document.createElement("button"); button.type = "button"; button.className = `station-list-item${station.id === selectedStationId ? " active" : ""}`;
      const name = document.createElement("strong"); name.textContent = station.name;
      const meta = document.createElement("span"); meta.textContent = `负载 ${formatNumber(station.load_rate, 1)}% · 空闲 ${station.idle_chargers}/${station.total_chargers}`;
      button.append(name, meta); button.addEventListener("click", () => { selectedStationId = station.id; render(window.ScreenStore.getState()); }); list.appendChild(button);
    });
    if (!filtered.length) { const empty = document.createElement("div"); empty.className = "compact-empty"; empty.textContent = "未找到匹配电站"; list.appendChild(empty); }
  }

  function renderStationDetail(station) {
    setText("station-detail-name", station?.name ?? "请选择电站");
    setText("station-detail-meta", station ? `电站编号 ${station.id} · 共 ${station.total_chargers} 台充电桩` : "");
    setText("station-power", formatNumber(station?.power_kw, 1)); setText("station-load", formatNumber(station?.load_rate, 1));
    setText("station-idle", formatNumber(station?.idle_chargers)); setText("station-revenue", formatNumber(station?.today_revenue, 2));
  }

  function renderEvents(events) {
    const filtered = eventFilter === "all" ? events : events.filter((event) => event.category === eventFilter);
    setText("event-count", String(filtered.length));
    const list = $("platform-event-list"); list.replaceChildren();
    if (!filtered.length) { const empty = document.createElement("div"); empty.className = "empty-state"; empty.textContent = "当前分类暂无事件"; list.appendChild(empty); return; }
    filtered.forEach((event) => {
      const row = document.createElement("div"); row.className = `platform-event-row event-category-${event.category}`;
      const time = document.createElement("time"); time.textContent = formatDateTime(event.event_time);
      const category = document.createElement("span"); category.className = "event-category"; category.textContent = categoryLabels[event.category] ?? "其他";
      const text = document.createElement("span"); text.className = "event-description"; text.textContent = event.text;
      const target = document.createElement("span"); target.className = "event-target"; target.textContent = event.target;
      row.append(time, category, text, target); list.appendChild(row);
    });
  }

  function renderError(error) { const banner = $("error-banner"); banner.hidden = !error; banner.textContent = error ?? ""; }
  function render(state) {
    renderConnection(state); renderMetrics(state.data.metrics); renderStations(state.data.stations);
    const selected = state.data.stations.find((station) => station.id === selectedStationId) ?? null;
    renderStationDetail(selected); renderEvents(state.data.events); renderError(state.error);
    setText("last-updated", state.lastUpdated ? `${state.stale ? "数据已过期 · " : "最近更新 "}${formatClock(state.lastUpdated)}` : "尚未更新");
    window.ScreenCharts.render(state.data, selected);
  }

  function showPage(pageName, updateHash = true) {
    const safePage = pageNames.has(pageName) ? pageName : "overview";
    document.querySelectorAll("[data-page]").forEach((page) => { const active = page.dataset.page === safePage; page.hidden = !active; page.classList.toggle("active", active); });
    document.querySelectorAll("[data-page-target]").forEach((button) => { button.classList.toggle("active", button.dataset.pageTarget === safePage); button.setAttribute("aria-current", button.dataset.pageTarget === safePage ? "page" : "false"); });
    if (updateHash && window.location.hash !== `#${safePage}`) window.history.replaceState(null, "", `#${safePage}`);
    window.requestAnimationFrame(() => window.ScreenCharts.resize());
  }

  function setupInteractions() {
    document.querySelectorAll("[data-page-target]").forEach((button) => button.addEventListener("click", () => showPage(button.dataset.pageTarget)));
    document.querySelectorAll("[data-event-filter]").forEach((button) => button.addEventListener("click", () => { eventFilter = button.dataset.eventFilter; document.querySelectorAll("[data-event-filter]").forEach((item) => item.classList.toggle("active", item === button)); renderEvents(window.ScreenStore.getState().data.events); }));
    $("station-search").addEventListener("input", (event) => { stationKeyword = event.target.value.trim(); renderStations(window.ScreenStore.getState().data.stations); });
    window.addEventListener("hashchange", () => showPage(window.location.hash.slice(1), false)); showPage(window.location.hash.slice(1) || "overview", false);
  }

  function start() {
    setupInteractions(); window.setInterval(() => setText("clock", formatClock(new Date())), 1000); setText("clock", formatClock(new Date())); window.ScreenStore.subscribe(render);
    window.setInterval(() => { const state = window.ScreenStore.getState(); if (window.ScreenConfig.mode === "live" && state.lastUpdated) window.ScreenStore.setStale(Date.now() - state.lastUpdated.getTime() > window.ScreenConfig.staleAfterMs); }, 5000);
    if (window.ScreenConfig.mode === "live") { const client = new window.DashboardSocket(window.ScreenConfig, window.ScreenStore); client.connect(); window.addEventListener("beforeunload", () => client.disconnect()); return; }
    window.ScreenStore.setConnection("offline"); window.ScreenStore.setSnapshot(window.ScreenMockSnapshot);
  }
  start();
}());
