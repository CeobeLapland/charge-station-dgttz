(function () {
  "use strict";
  const byId = id => document.getElementById(id);
  const localTime = () => { const d=new Date(),p=n=>String(n).padStart(2,"0"); return `${d.getFullYear()}-${p(d.getMonth()+1)}-${p(d.getDate())} ${p(d.getHours())}:${p(d.getMinutes())}:${p(d.getSeconds())}`; };
  let socket=null;
  const dashboard=new DashboardComponents.MainDashboard(filters => { DataStore.setFilters(filters); if (ScreenConfig.mode === "live") socket.requestSnapshot(filters); });
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
  if (ScreenConfig.mode === "live") { socket=new WsAdapter(ScreenConfig,DataStore); socket.connect(); addEventListener("beforeunload",() => socket.disconnect()); }
  else DataStore.setSnapshot(ScreenMockSnapshot);
}());
