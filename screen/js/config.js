(function () {
  "use strict";

  const query = new URLSearchParams(window.location.search);
  const requestedMode = query.get("mode");

  window.ScreenConfig = Object.freeze({
    // 服务端完成前默认 mock。联调时使用 index.html?mode=live。
    mode: requestedMode === "live" ? "live" : "mock",
    websocketUrl: "ws://127.0.0.1:9000",
    heartbeatIntervalMs: 15000,
    heartbeatFailureLimit: 3,
    reconnectBaseDelayMs: 1000,
    reconnectMaxDelayMs: 15000,
    staleAfterMs: 60000,
    maxAlarms: 20,
    maxEvents: 30,
    loadThresholds: Object.freeze({ busy: 60, congested: 85 })
  });
}());
