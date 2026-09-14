(function () {
  "use strict";
  const byId = id => document.getElementById(id);
  const localTime = () => { const d=new Date(),p=n=>String(n).padStart(2,"0"); return `${d.getFullYear()}-${p(d.getMonth()+1)}-${p(d.getDate())} ${p(d.getHours())}:${p(d.getMinutes())}:${p(d.getSeconds())}`; };
  let socket=null;
  const dashboard=new DashboardComponents.MainDashboard(filters => { DataStore.setFilters(filters); if (ScreenConfig.mode === "live") { window.loadHiveData && window.loadHiveData(filters); } });
  DataStore.subscribe(state => {
    dashboard.render(state.data);
    byId("connection-status").textContent={live:"实时连接",connecting:"正在连接",reconnecting:"重新连接",offline:"离线演示"}[state.connection]??"连接异常";
    byId("last-updated").textContent=state.lastUpdated ? `更新 ${localTime().split(" ")[1]}` : "尚未更新";
    byId("error-banner").hidden=!state.error;
    byId("error-banner").textContent=state.error??"";
  });
  byId("clock").textContent=localTime();
  setInterval(() => byId("clock").textContent=localTime(),1000);
  byId("fullscreen-button").onclick=() => document.fullscreenElement ? document.exitFullscreen() : document.documentElement.requestFullscreen();
  if (ScreenConfig.mode === "live") {
    // 从网关 /api/snapshot 拉 Hive 聚合数据（WebSocket 服务端未运行，改用 HTTP），支持筛选
    const loadHiveData = (filters) => {
      const params = new URLSearchParams();
      const f = filters || {};
      if (f.date) params.set("date", f.date);
      if (f.region) params.set("region", f.region);
      if (f.station_id) params.set("station_id", f.station_id);
      const qs = params.toString();
      return fetch("/api/snapshot" + (qs ? "?" + qs : "")).then(r => r.json()).then(payload => {
        DataStore.setConnection("live");
        DataStore.setSnapshot(payload);
      }).catch(e => { DataStore.setError("加载 Hive 数据失败: " + e); });
    };
    window.loadHiveData = loadHiveData;
    loadHiveData();
    // 机器学习：未来24h负荷预测（PME）
    fetch("/api/forecast").then(r => r.json()).then(fc => { if (fc && !fc.error && window.ScreenCharts) window.ScreenCharts.forecast(fc); }).catch(()=>{});
  }
  else DataStore.setSnapshot(ScreenMockSnapshot);
}());
