(function () {
  "use strict";

  class DashboardSocket {
    constructor(config, store) {
      this.config = config;
      this.store = store;
      this.socket = null;
      this.sequence = 0;
      this.reconnectAttempt = 0;
      this.reconnectTimer = null;
      this.heartbeatTimer = null;
      this.missedHeartbeats = 0;
      this.stopped = false;
    }

    connect() {
      if (this.socket && [WebSocket.OPEN, WebSocket.CONNECTING].includes(this.socket.readyState)) {
        return;
      }

      this.stopped = false;
      this.store.setConnection(this.reconnectAttempt > 0 ? "reconnecting" : "connecting");

      try {
        this.socket = new WebSocket(this.config.websocketUrl);
      } catch (error) {
        this.handleFailure(error);
        return;
      }

      this.socket.addEventListener("open", () => {
        this.reconnectAttempt = 0;
        this.missedHeartbeats = 0;
        this.store.setConnection("live");
        this.store.setError(null);
        this.requestSnapshot();
        this.startHeartbeat();
      });

      this.socket.addEventListener("message", (event) => {
        try {
          const message = window.ScreenAdapter.normalizeMessage(event.data);
          if (message.type === "system.pong") {
            this.missedHeartbeats = 0;
            return;
          }
          this.store.applyMessage(message);
        } catch (error) {
          this.store.setError(`无法处理服务端消息：${error.message}`);
        }
      });

      this.socket.addEventListener("error", () => {
        this.store.setError(`无法连接 ${this.config.websocketUrl}`);
      });

      this.socket.addEventListener("close", () => {
        this.stopHeartbeat();
        this.socket = null;
        if (!this.stopped) {
          this.scheduleReconnect();
        }
      });
    }

    send(type, payload = {}) {
      if (!this.socket || this.socket.readyState !== WebSocket.OPEN) {
        return false;
      }

      this.sequence += 1;
      this.socket.send(JSON.stringify({ type, seq: this.sequence, payload }));
      return true;
    }

    requestSnapshot() {
      this.send("screen.snapshot", {});
    }

    startHeartbeat() {
      this.stopHeartbeat();
      this.heartbeatTimer = window.setInterval(() => {
        this.missedHeartbeats += 1;
        if (this.missedHeartbeats >= this.config.heartbeatFailureLimit) {
          this.store.setError("服务端心跳超时，准备重新连接");
          this.socket?.close();
          return;
        }
        this.send("system.ping", { timestamp: new Date().toISOString() });
      }, this.config.heartbeatIntervalMs);
    }

    stopHeartbeat() {
      if (this.heartbeatTimer) {
        window.clearInterval(this.heartbeatTimer);
        this.heartbeatTimer = null;
      }
    }

    scheduleReconnect() {
      this.reconnectAttempt += 1;
      this.store.setConnection("reconnecting");
      const delay = Math.min(
        this.config.reconnectBaseDelayMs * (2 ** (this.reconnectAttempt - 1)),
        this.config.reconnectMaxDelayMs
      );
      this.reconnectTimer = window.setTimeout(() => this.connect(), delay);
    }

    handleFailure(error) {
      this.store.setError(error);
      this.scheduleReconnect();
    }

    disconnect() {
      this.stopped = true;
      this.stopHeartbeat();
      if (this.reconnectTimer) {
        window.clearTimeout(this.reconnectTimer);
      }
      this.socket?.close();
      this.socket = null;
      this.store.setConnection("offline");
    }
  }

  window.DashboardSocket = DashboardSocket;
}());
