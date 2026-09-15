(function () {
  "use strict";

  const query = new URLSearchParams(window.location.search);
  const requestedMode = query.get("mode");

  // 模式优先级：URL ?mode= > localStorage 记忆 > 默认 live（真实数据）
  let mode;
  if (requestedMode === "live" || requestedMode === "mock") {
    mode = requestedMode;
    try { localStorage.setItem("screen_mode", mode); } catch (e) { /* 忽略 */ }
  } else {
    let saved = null;
    try { saved = localStorage.getItem("screen_mode"); } catch (e) { /* 忽略 */ }
    mode = saved === "mock" ? "mock" : "live";
  }

  window.ScreenConfig = Object.freeze({
    mode: mode,
    websocketUrl: "ws://127.0.0.1:9000",
    heartbeatIntervalMs: 15000,
    heartbeatFailureLimit: 3,
    reconnectBaseDelayMs: 1000,
    reconnectMaxDelayMs: 15000,
    fallbackRefreshMs: 60000,
    pushThrottleMs: 200,
    staleAfterMs: 60000,
    maxAlarms: 20,
    maxEvents: 30,
    loadThresholds: Object.freeze({ busy: 60, congested: 85 })
  });
}());
