# 大屏服务端联调说明

当前页面默认使用 mock 数据，服务端完成前可独立开发和演示。

完整请求、响应、推送、字段、单位和枚举见 `API_CONTRACT.md`。本文件只保留快速联调步骤。

## 启动方式

直接打开 `index.html`，或在 `screen/` 目录启动简单静态服务器。

- mock 模式：`index.html`
- WebSocket 联调模式：`index.html?mode=live`

联调模式默认连接 `ws://127.0.0.1:9000`。地址在 `js/config.js` 中集中配置。

## 服务端完成后通常只改两处

1. `js/config.js`：修改 WebSocket 地址、心跳和重连参数。
2. `js/adapter.js` 的 `normalizeSnapshot()`：把服务端真实字段映射成页面内部字段。

页面组件、图表和 mock 数据不应直接跟随服务端字段变化。

## 需要服务端确认的快照结构

大屏连接后发送：

```json
{ "type": "screen.snapshot", "seq": 1, "payload": {} }
```

当前页面期望：

```json
{
  "type": "screen.snapshot_resp",
  "seq": 1,
  "code": 0,
  "message": "ok",
  "payload": {
    "metrics": {
      "today_revenue": 18260.5,
      "today_orders": 386,
      "charging_count": 48,
      "online_rate": 96.2,
      "revenue_change_pct": 12.8,
      "orders_change_pct": 8.4
    },
    "stations": [],
    "load_series": { "actual": [], "forecast": [] },
    "utilization_rank": [],
    "alarms": [],
    "user_growth": [],
    "energy_by_price_level": { "valley": 0, "flat": 0, "peak": 0 },
    "events": []
  }
}
```

完整示例参见 `mock/snapshot.js`。

## 需要服务端提供的推送

- `push.charger_status`
- `push.alarm`
- `push.order_event`
- `push.forecast`
- `system.pong`

必须确认每种消息的完整 JSON、字段类型、单位、时间格式和唯一 ID。

## 联调时重点确认

- `online_rate` 返回 0–1 还是 0–100；当前页面按 0–100 处理。
- 金额使用 `charging_order.pay_amount` 的已结算口径，不使用应收金额或充值额。
- 负荷单位是 kW，电量单位是 kWh。
- 实际/预测序列字段、时间戳以及置信区间字段。
- 告警对象是否同时给出 `station_name`；若只给 `station_id`，应由服务端快照提供可查询的站点映射。
- `push.order_event` 必须提供脱敏后的展示文字，或提供结构化字段供适配器安全拼接。
- `push.charger_status` 是否携带新的聚合指标；若没有，需要约定节流重拉快照的频率。
- 心跳时间戳格式和服务端断开判定。

## ECharts 离线注意

当前 `index.html` 暂时从 CDN 加载 ECharts。正式答辩环境如果不能联网，应将固定版本的 `echarts.min.js` 放入 `screen/vendor/`，然后把脚本地址改成本地路径。不要等到联调结束才验证离线运行。
