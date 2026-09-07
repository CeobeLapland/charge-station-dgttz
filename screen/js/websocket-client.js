(function () {
  "use strict";
  const localTime = () => { const d = new Date(), p = n => String(n).padStart(2, "0"); return `${d.getFullYear()}-${p(d.getMonth()+1)}-${p(d.getDate())} ${p(d.getHours())}:${p(d.getMinutes())}:${p(d.getSeconds())}`; };
  class WsAdapter {
    constructor(config, store) { Object.assign(this, { config, store, ws:null, seq:0, snapshotSeq:null, missedPongs:0, reconnectAttempts:0, timers:[], reconnectTimer:null, pushTimer:null, pendingStatus:null, stopped:false }); }
    connect() {
      if (this.ws && this.ws.readyState < WebSocket.CLOSING) return;
      this.store.setConnection(this.reconnectAttempts ? "reconnecting" : "connecting");
      this.ws = new WebSocket(this.config.websocketUrl);
      this.ws.onopen = () => { this.reconnectAttempts=0; this.missedPongs=0; this.store.setConnection("live"); this.requestSnapshot(); this.startTimers(); };
      this.ws.onmessage = event => this.onMessage(event.data);
      this.ws.onerror = () => this.store.setError("WebSocket 连接异常");
      this.ws.onclose = () => { this.clearTimers(); this.ws=null; if (!this.stopped) this.reconnect(); };
    }
    onMessage(raw) {
      try {
        const message = window.ScreenAdapter.normalizeMessage(raw);
        if (message.type === "system.pong") { this.missedPongs=0; return; }
        if (message.type === "screen.snapshot_resp" && this.snapshotSeq !== null && message.seq !== undefined && message.seq !== this.snapshotSeq) return;
        if (message.type === "push.charger_status") {
          this.pendingStatus=message;
          if (!this.pushTimer) this.pushTimer=setTimeout(() => { this.store.applyMessage(this.pendingStatus); this.pendingStatus=null; this.pushTimer=null; }, this.config.pushThrottleMs);
          return;
        }
        this.store.applyMessage(message);
      } catch (error) { this.store.setError(error); }
    }
    send(type, payload={}) { if (!this.ws || this.ws.readyState !== WebSocket.OPEN) return null; this.seq+=1; this.ws.send(JSON.stringify({type,seq:this.seq,payload})); return this.seq; }
    requestSnapshot(filters=this.store.getState().filters) { this.snapshotSeq=this.send("screen.snapshot",filters); return this.snapshotSeq; }
    startTimers() {
      this.clearTimers();
      this.timers.push(setInterval(() => { if (this.missedPongs >= this.config.heartbeatFailureLimit) { this.ws?.close(); return; } if (this.send("system.ping",{timestamp:localTime()}) !== null) this.missedPongs+=1; }, this.config.heartbeatIntervalMs));
      this.timers.push(setInterval(() => this.requestSnapshot(), this.config.fallbackRefreshMs));
    }
    clearTimers() { this.timers.forEach(clearInterval); this.timers=[]; }
    reconnect() { this.reconnectAttempts+=1; this.store.setConnection("reconnecting"); clearTimeout(this.reconnectTimer); const delay=Math.min(this.config.reconnectBaseDelayMs*2**(this.reconnectAttempts-1),this.config.reconnectMaxDelayMs); this.reconnectTimer=setTimeout(() => this.connect(),delay); }
    disconnect() { this.stopped=true; this.clearTimers(); clearTimeout(this.pushTimer); clearTimeout(this.reconnectTimer); this.ws?.close(); }
  }
  window.WsAdapter=window.DashboardSocket=WsAdapter;
}());
