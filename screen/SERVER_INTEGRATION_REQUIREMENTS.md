# Web 大屏服务端接口交接说明

面向：服务端开发负责人  
更新时间：2026-09-04  
优先级说明：P0 为大屏完成联调必须具备，P1 为完整实时效果必须具备，P2 为预测增强功能。

## 1. 大屏接入方式

大屏是只读 Web 客户端，通过浏览器原生 WebSocket 连接服务端，不登录、不写业务数据、不直连数据库。

当前默认地址：

```text
ws://127.0.0.1:9000
```

消息使用 UTF-8 JSON 文本。请求、响应和推送必须遵守以下信封。

客户端请求：

```json
{
  "type": "screen.snapshot",
  "seq": 1,
  "payload": {}
}
```

服务端响应：

```json
{
  "type": "screen.snapshot_resp",
  "seq": 1,
  "code": 0,
  "message": "ok",
  "payload": {}
}
```

服务端推送：

```json
{
  "type": "push.alarm",
  "payload": {}
}
```

响应的 `seq` 必须原样回填客户端请求序号；成功时 `code` 为 `0`。推送不带 `seq`。

## 2. 服务端当前完成情况

| 能力 | 状态 |
| --- | --- |
| WebSocket 监听 9000 端口 | 已完成 |
| `screen.snapshot` | 已完成 |
| `system.ping/system.pong` | 已完成 |
| `push.charger_status` | 部分完成，仅管理端操作时广播 |
| `push.alarm` | 未完成 |
| `push.order_event` | 未完成 |
| 实际负荷实时更新 | 未完成 |
| `push.forecast` | 未完成 |
| `ml.forecast` | 当前固定返回 `5001` |

当前全量快照已经能被大屏正确解析。接下来服务端的重点是补齐业务事件推送，而不是重新设计快照字段。

## 3. P0：全量快照

### 3.1 请求

消息：`screen.snapshot`

发送时机：

- 大屏首次连接成功。
- 大屏断线重连成功。
- 收到不含聚合指标的设备状态推送后，大屏会按最多每 2 秒一次进行节流重拉。

请求示例：

```json
{
  "type": "screen.snapshot",
  "seq": 1,
  "payload": {}
}
```

### 3.2 响应

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
    "load_series": {
      "actual": [],
      "forecast": []
    },
    "utilization_rank": [],
    "alarms": [],
    "user_growth": [],
    "energy_by_price_level": {
      "valley": 3820,
      "flat": 5160,
      "peak": 2740
    },
    "events": []
  }
}
```

### 3.3 指标口径

| 字段 | 类型 | 单位 | 服务端统一口径 |
| --- | --- | --- | --- |
| `today_revenue` | number | 元 | 今日已结算订单 `pay_amount` 合计 |
| `today_orders` | integer | 笔 | 今日按 `create_time` 创建的全部订单 |
| `charging_count` | integer | 台 | 当前 `charger.status = charging` 的数量 |
| `online_rate` | number | % | 非 `offline/fault` 桩数 ÷ 总桩数 × 100，返回 0–100 |
| `revenue_change_pct` | number | % | 今日营收较昨日同期变化率 |
| `orders_change_pct` | number | % | 今日订单较昨日同期变化率 |

### 3.4 快照数组字段

`stations[]`：

```json
{
  "id": 1,
  "name": "软件园快充站",
  "longitude": 123.43,
  "latitude": 41.77,
  "load_rate": 88.0,
  "idle_chargers": 2,
  "total_chargers": 18,
  "today_revenue": 5260.0
}
```

`load_series.actual[]`：

```json
{
  "timestamp": "2026-09-04 18:00:00",
  "value_kw": 875.2
}
```

`load_series.forecast[]`：

```json
{
  "timestamp": "2026-09-04 19:00:00",
  "value_kw": 902.4,
  "lower_kw": 866.1,
  "upper_kw": 938.7
}
```

`utilization_rank[]`：

```json
{
  "station_id": 1,
  "station_name": "软件园快充站",
  "utilization_rate": 88.0
}
```

`alarms[]`：

```json
{
  "id": 101,
  "station_id": 1,
  "station_name": "软件园快充站",
  "charger_id": 7,
  "type": "overheat",
  "level": "critical",
  "status": "open",
  "occur_time": "2026-09-04 18:03:00"
}
```

站点级告警没有电桩时，`charger_id` 必须显式返回 JSON `null`，不要省略字段，也不要返回 `0`。

`user_growth[]`：

```json
{
  "date": "2026-09-04",
  "new_users": 96
}
```

`events[]`：

```json
{
  "id": "evt-o1002-order_started",
  "event_time": "2026-09-04 18:12:00",
  "event_type": "order_started",
  "text": "用户 189****4421 在软件园快充站 A03 开始充电"
}
```

事件文本中的手机号必须在服务端脱敏。`id` 必须稳定且可用于去重。

## 4. P0：心跳

大屏每 15 秒发送：

```json
{
  "type": "system.ping",
  "seq": 2,
  "payload": {
    "timestamp": "2026-09-04T18:00:00.000Z"
  }
}
```

服务端返回：

```json
{
  "type": "system.pong",
  "seq": 2,
  "code": 0,
  "message": "ok",
  "payload": {
    "timestamp": "2026-09-04 18:00:00"
  }
}
```

当前实现已经满足大屏要求。大屏连续 3 次未收到 pong 会断开并自动重连。

## 5. P1：电桩状态推送

消息：`push.charger_status`

触发时机：

- 电桩进入或结束充电。
- 电桩预约状态变化。
- 电桩故障、离线或恢复。
- 管理端执行重启或暂停。
- 重启结束并恢复正常状态。

建议完整结构：

```json
{
  "type": "push.charger_status",
  "payload": {
    "charger_id": 7,
    "station_id": 1,
    "status": "charging",
    "soc": 42.5,
    "power_kw": 60.2,
    "temperature": 39.8,
    "metrics": {
      "charging_count": 48,
      "online_rate": 96.2
    }
  }
}
```

`metrics` 强烈建议由服务端一并提供。如果不提供，大屏会收到推送后重新请求完整快照，这会增加数据库聚合查询次数。

合法状态：`idle`、`charging`、`reserved`、`fault`、`offline`、`rebooting`。

## 6. P1：告警推送

消息：`push.alarm`

触发时机：新告警成功写入数据库后立即发送。不要只写库而不推送。

```json
{
  "type": "push.alarm",
  "payload": {
    "alarm": {
      "id": 104,
      "station_id": 1,
      "station_name": "软件园快充站",
      "charger_id": 7,
      "type": "overheat",
      "level": "critical",
      "status": "open",
      "occur_time": "2026-09-04 18:10:00"
    }
  }
}
```

要求：

- 同一告警始终使用相同 `alarm.id`。
- 告警级别只使用 `info`、`warning`、`critical`。
- 建议同时提供 `station_name`，避免大屏再次查询。
- 告警推送需要同时到达管理端和大屏。

## 7. P1：订单事件推送

消息：`push.order_event`

触发时机：订单创建、开始、结束、结算、取消成功后发送。

```json
{
  "type": "push.order_event",
  "payload": {
    "event": {
      "id": "evt-o1002-order_completed",
      "event_time": "2026-09-04 18:12:00",
      "event_type": "order_completed",
      "text": "用户 189****4421 完成充电并结算"
    }
  }
}
```

建议事件类型：

- `order_created`
- `order_started`
- `order_finished`
- `order_completed`
- `order_cancelled`

手机号必须脱敏。推荐稳定 ID 格式：`evt-o<order_id>-<event_type>`。

## 8. P1：实际负荷更新

当前快照会返回近 24 小时实际负荷，但数据库新增 `charging_measure` 后，网页不会自动知道。

请在以下两种方案中选择一种：

### 方案 A：服务端推送，推荐

每个采样周期完成后推送新的实际负荷点。需要先在公共协议中新增明确消息，例如：

```json
{
  "type": "push.load_actual",
  "payload": {
    "timestamp": "2026-09-04 18:15:00",
    "value_kw": 886.5
  }
}
```

新增消息前必须先同步更新 `docs/content/spec-协议.md`，大屏随后补充对应处理。

### 方案 B：定时快照，临时方案

由大屏每 15–30 秒重新请求 `screen.snapshot`。该方案不需要新增消息，但会重复执行全部聚合查询，只建议联调阶段使用。

## 9. P2：预测推送

消息：`push.forecast`

机器学习模块产生新预测后发送：

```json
{
  "type": "push.forecast",
  "payload": {
    "station_id": null,
    "horizon": 6,
    "series": [
      {
        "timestamp": "2026-09-04 19:00:00",
        "value_kw": 930.5,
        "lower_kw": 890.0,
        "upper_kw": 970.0
      }
    ]
  }
}
```

约定：

- `station_id = null` 表示全网预测。
- `horizon` 单位为小时，可取 `1`、`6`、`24`。
- `value_kw`、`lower_kw`、`upper_kw` 单位全部为 kW。
- 预测不可用时返回错误码 `5001`，不得伪造预测数据。

## 10. 时间、单位和枚举

- 业务时间统一使用 `yyyy-MM-dd HH:mm:ss`；若改用 ISO 8601，所有接口需要统一。
- 功率使用 kW。
- 电量使用 kWh。
- 金额使用元。
- 百分比统一返回 0–100，不返回 0–1。
- 电价档位只使用 `valley`、`flat`、`peak`。
- 未知枚举不要映射成正常状态。

## 11. 广播范围

服务端当前 `broadcast()` 会把所有推送发送给用户端、管理端和大屏连接。建议后续给连接记录客户端角色或订阅范围：

- `push.alarm`：管理端、大屏。
- `push.order_event`：大屏。
- `push.forecast`：大屏及确实需要预测的客户端。
- `push.charger_status`：管理端、大屏，用户端按业务需要订阅。

联调阶段可以暂时全量广播，但客户端必须忽略不认识的推送，不能因此断开连接。

## 12. 服务端完成清单

- [x] `screen.snapshot` 返回完整快照。
- [x] `system.ping` 返回 `system.pong`。
- [ ] 电桩所有状态变化都触发 `push.charger_status`。
- [ ] `push.charger_status` 携带 `station_id` 和聚合 `metrics`。
- [ ] 新告警写库后发送 `push.alarm`。
- [ ] 订单关键状态变化后发送 `push.order_event`。
- [ ] 确定实际负荷采用推送还是临时定时快照。
- [ ] 机器学习完成后发送 `push.forecast`。
- [ ] 站点级告警明确输出 `"charger_id": null`。
- [ ] 确认跨机器访问的 IP、端口和防火墙。
- [ ] 后续按客户端角色限制广播范围。

## 13. 联调验收步骤

1. 启动服务端，确认监听 9000 端口。
2. 使用 `index.html?mode=live` 打开大屏。
3. 服务端日志应出现 `screen.snapshot`，大屏显示“实时连接”。
4. 核对四项指标与数据库查询结果。
5. 在管理端暂停或重启电桩，大屏应在约 2 秒内更新指标和事件。
6. 模拟一条新告警，大屏和管理端应同时显示相同 `alarm.id`。
7. 创建、开始和结算订单，事件流应自动出现对应事件。
8. 写入新的负荷采样点，曲线应按最终约定自动更新。
9. 停止服务端，大屏应显示重连状态；恢复服务端后应自动重连并重新拉取快照。
10. 预测模块不可用时，大屏只显示实际负荷，不出现虚假预测线。

完成第 1–7 项后，基础大屏即可达到可验收的实时联调状态。
