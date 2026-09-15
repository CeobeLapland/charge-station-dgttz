(function () {
  "use strict";
  var byId = function (id) { return document.getElementById(id); };
  var canvas = null, ctx = null, wrap = null, W = 0, H = 0;
  var stations = [];
  var mode = "map";                        // "map" | "dot"
  var view = { scale: 1, tx: 0, ty: 0 };   // 缩放/平移
  var hover = -1;
  var dragging = false, dragStart = null;

  // 伪城区底图：几条"道路" + 一条"河流"（世界坐标 lon/lat 折线，随缩放平移）
  var ROADS = [
    [[116.287, 39.968], [116.305, 39.976], [116.322, 39.970], [116.338, 39.976]],
    [[116.292, 39.988], [116.312, 39.982], [116.332, 39.988]],
    [[116.300, 39.965], [116.302, 39.978], [116.298, 39.992]]
  ];
  var RIVER = [[116.316, 39.964], [116.322, 39.976], [116.314, 39.988], [116.320, 39.996]];

  function bbox() {
    var minLon = Infinity, maxLon = -Infinity, minLat = Infinity, maxLat = -Infinity, n = 0;
    (stations || []).forEach(function (s) {
      var lo = Number(s.longitude), la = Number(s.latitude);
      if (!isFinite(lo) || !isFinite(la)) return;
      if (lo < minLon) minLon = lo; if (lo > maxLon) maxLon = lo;
      if (la < minLat) minLat = la; if (la > maxLat) maxLat = la;
      n++;
    });
    if (!n) { minLon = 116.289; maxLon = 116.332; minLat = 39.971; maxLat = 39.992; }
    var dL = (maxLon - minLon) || 0.01, dB = (maxLat - minLat) || 0.005;
    return { minLon: minLon - dL * 0.2, maxLon: maxLon + dL * 0.2, minLat: minLat - dB * 0.2, maxLat: maxLat + dB * 0.2 };
  }

  // 世界坐标 → 基础适配（fit 到面板），再叠加 view 缩放/平移
  function fit() {
    var b = bbox();
    var spanLon = (b.maxLon - b.minLon) || 0.01, spanLat = (b.maxLat - b.minLat) || 0.005;
    var k = Math.min(W / spanLon, H / spanLat) * 0.92;
    return { k: k, ox: W / 2 - k * (b.minLon + b.maxLon) / 2, oy: H / 2 + k * (b.minLat + b.maxLat) / 2 };
  }

  function toPx(lo, la, F) {
    var bx = F.k * lo + F.ox, by = -F.k * la + F.oy;
    var cx = W / 2, cy = H / 2;
    return { x: cx + (bx - cx) * view.scale + view.tx, y: cy + (by - cy) * view.scale + view.ty };
  }

  function layout() {
    if (!canvas) return;
    W = canvas.clientWidth || wrap.clientWidth || 300;
    H = canvas.clientHeight || wrap.clientHeight || 200;
    var dpr = window.devicePixelRatio || 1;
    canvas.width = W * dpr; canvas.height = H * dpr;
    canvas.style.width = W + "px"; canvas.style.height = H + "px";
    ctx = canvas.getContext("2d");
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  }

  function draw() {
    if (!ctx || mode !== "map") return;
    ctx.clearRect(0, 0, W, H);
    ctx.fillStyle = "#edf2f8"; ctx.fillRect(0, 0, W, H);
    // 网格（屏幕固定，淡）
    ctx.strokeStyle = "rgba(120,140,170,0.12)"; ctx.lineWidth = 1;
    ctx.beginPath();
    for (var gx = 0; gx <= W; gx += 48) { ctx.moveTo(gx + 0.5, 0); ctx.lineTo(gx + 0.5, H); }
    for (var gy = 0; gy <= H; gy += 48) { ctx.moveTo(0, gy + 0.5); ctx.lineTo(W, gy + 0.5); }
    ctx.stroke();
    var F = fit();
    // 河流
    ctx.strokeStyle = "rgba(120,170,205,0.45)"; ctx.lineWidth = 6; ctx.lineCap = "round"; ctx.lineJoin = "round";
    ctx.beginPath();
    RIVER.forEach(function (p, i) { var q = toPx(p[0], p[1], F); i ? ctx.lineTo(q.x, q.y) : ctx.moveTo(q.x, q.y); });
    ctx.stroke();
    // 道路
    ctx.strokeStyle = "rgba(160,175,195,0.6)"; ctx.lineWidth = 3.5; ctx.lineCap = "round"; ctx.lineJoin = "round";
    ROADS.forEach(function (road) {
      ctx.beginPath();
      road.forEach(function (p, i) { var q = toPx(p[0], p[1], F); i ? ctx.lineTo(q.x, q.y) : ctx.moveTo(q.x, q.y); });
      ctx.stroke();
    });
    // 站点
    var pts = [];
    stations.forEach(function (s, i) {
      var q = toPx(Number(s.longitude), Number(s.latitude), F);
      if (q.x < -40 || q.x > W + 40 || q.y < -40 || q.y > H + 40) { pts.push(null); return; }
      pts.push(q);
      var st = String(s.status_text || s.status || "normal");
      var color = (st.indexOf("故障") >= 0 || st === "fault") ? "#c0554f" : "#557da4";
      ctx.beginPath(); ctx.arc(q.x, q.y, 7, 0, Math.PI * 2);
      ctx.fillStyle = color; ctx.fill();
      ctx.strokeStyle = "#fff"; ctx.lineWidth = 2; ctx.stroke();
      ctx.font = "11px Inter, Microsoft YaHei, sans-serif";
      ctx.fillStyle = "#31405a";
      ctx.fillText(s.name, q.x + 12, q.y + 4);
    });
    // 悬停提示
    if (hover >= 0 && pts[hover]) {
      var s2 = stations[hover], q2 = pts[hover];
      var label = s2.name + " · " + (s2.status_text || s2.status || "正常");
      ctx.font = "11px Inter, Microsoft YaHei, sans-serif";
      var tw = ctx.measureText(label).width + 18;
      var bx = Math.min(Math.max(q2.x - tw / 2, 4), W - tw - 4), by = Math.max(q2.y - 36, 4);
      ctx.fillStyle = "rgba(255,255,255,0.96)"; ctx.strokeStyle = "#c8d4e0";
      ctx.beginPath();
      if (ctx.roundRect) ctx.roundRect(bx, by, tw, 22, 6); else ctx.rect(bx, by, tw, 22);
      ctx.fill(); ctx.stroke();
      ctx.fillStyle = "#31405a"; ctx.textAlign = "center";
      ctx.fillText(label, bx + tw / 2, by + 15);
      ctx.textAlign = "left";
    }
  }

  function hitTest(mx, my) {
    var F = fit();
    for (var i = stations.length - 1; i >= 0; i--) {
      var s = stations[i];
      var q = toPx(Number(s.longitude), Number(s.latitude), F);
      var dx = q.x - mx, dy = q.y - my;
      if (dx * dx + dy * dy <= 144) return i;
    }
    return -1;
  }

  function render() { layout(); draw(); }

  function syncMode() {
    var cv = byId("station-map-canvas"), ec = byId("station-map");
    if (!cv || !ec) return;
    if (mode === "map") { cv.hidden = false; ec.hidden = true; }
    else { cv.hidden = true; ec.hidden = false; window.ScreenCharts && window.ScreenCharts.map(stations); }
    var bm = byId("map-mode-map"), bd = byId("map-mode-dot");
    if (bm && bd) { bm.classList.toggle("on", mode === "map"); bd.classList.toggle("on", mode === "dot"); }
  }

  function init() {
    canvas = byId("station-map-canvas");
    if (!canvas) return;
    wrap = canvas.parentElement;
    if (window._pendingMap) { stations = window._pendingMap; delete window._pendingMap; }
    layout();
    // 保险：若此时容器尚未布局出尺寸，稍后重试
    if (W === 0 || H === 0) {
      setTimeout(function () { layout(); syncMode(); render(); }, 150);
    }
    var bm = byId("map-mode-map"), bd = byId("map-mode-dot");
    if (bm) bm.onclick = function () { mode = "map"; syncMode(); render(); };
    if (bd) bd.onclick = function () { mode = "dot"; syncMode(); };
    canvas.addEventListener("wheel", function (e) {
      e.preventDefault();
      var rect = canvas.getBoundingClientRect();
      var mx = e.clientX - rect.left, my = e.clientY - rect.top;
      var f = e.deltaY < 0 ? 1.12 : 1 / 1.12;
      var ns = Math.min(12, Math.max(0.4, view.scale * f));
      var k = ns / view.scale;
      view.tx = mx - (mx - view.tx) * k;
      view.ty = my - (my - view.ty) * k;
      view.scale = ns;
      draw();
    }, { passive: false });
    canvas.addEventListener("mousedown", function (e) {
      dragging = true; dragStart = { x: e.clientX, y: e.clientY, tx: view.tx, ty: view.ty };
      canvas.classList.add("dragging");
      hover = -1;
      e.preventDefault();
    });
    window.addEventListener("mousemove", function (e) {
      if (dragging && dragStart) {
        view.tx = dragStart.tx + (e.clientX - dragStart.x);
        view.ty = dragStart.ty + (e.clientY - dragStart.y);
        draw();
        return;
      }
      var rect = canvas.getBoundingClientRect();
      var h = hitTest(e.clientX - rect.left, e.clientY - rect.top);
      if (h !== hover) { hover = h; draw(); }
    });
    window.addEventListener("mouseup", function () {
      if (dragging) { dragging = false; canvas.classList.remove("dragging"); dragStart = null; }
    });
    canvas.addEventListener("dblclick", function () { view.scale = 1; view.tx = 0; view.ty = 0; draw(); });
    window.addEventListener("resize", render);
    syncMode();
    render();
  }

  window.ScreenMap = {
    setData: function (list) { stations = list || []; render(); },
    setMode: function (m) { mode = m; syncMode(); render(); },
    render: render
  };

  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", init);
  else init();
}());
