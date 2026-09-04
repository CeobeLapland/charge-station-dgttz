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
      this.pendingSnapshotSeq = null;
      this.lastSnapshotRequestAt = 0;
      this.snapshotRefreshTimer = null;
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
          if (message.type === "screen.snapshot_resp") {
            if (!Number.isInteger(message.seq) || message.seq !== this.pendingSnapshotSeq) {
              throw new Error("快照响应序号与请求不匹配");
            }
            if (!Number.isInteger(message.code) || !message.payload || typeof message.payload !== "object") {
              throw new Error("快照响应信封不完整");
            }
            this.pendingSnapshotSeq = null;
          }
          this.store.applyMessage(message);
          if (message.type === "push.charger_status" && !message.payload?.metrics) {
            this.requestSnapshotThrottled();
          }
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
        return null;
      }

      this.sequence += 1;
      this.socket.send(JSON.stringify({ type, seq: this.sequence, payload }));
      return this.sequence;
    }

    requestSnapshot() {
      const seq = this.send("screen.snapshot", {});
      if (seq !== null) {
        this.pendingSnapshotSeq = seq;
        this.lastSnapshotRequestAt = Date.now();
      }
    }

    requestSnapshotThrottled() {
      const wait = Math.max(0, this.config.snapshotRefreshThrottleMs - (Date.now() - this.lastSnapshotRequestAt));
      if (this.snapshotRefreshTimer) return;
      this.snapshotRefreshTimer = window.setTimeout(() => {
        this.snapshotRefreshTimer = null;
        this.requestSnapshot();
      }, wait);
    }

    startHeartbeat() {
      this.stopHeartbeat();
      this.heartbeatTimer = window.setInterval(() => {
        if (this.missedHeartbeats >= this.config.heartbeatFailureLimit) {
          this.store.setError("服务端心跳超时，准备重新连接");
          this.socket?.close();
          return;
        }
        if (this.send("system.ping", { timestamp: new Date().toISOString() }) !== null) {
          this.missedHeartbeats += 1;
        }
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
      if (this.snapshotRefreshTimer) {
        window.clearTimeout(this.snapshotRefreshTimer);
      }
      this.socket?.close();
      this.socket = null;
      this.store.setConnection("offline");
    }
  }

  window.DashboardSocket = DashboardSocket;
}());
