# Web 大屏接口契约（服务端联调版）

状态：**v1 联调草案**。2026-09-05 按最新产品要求移除大屏预测展示；旧文中预测章节仅保留为历史协议参考，不属于当前页面范围。当前新增字段与事件分类以 `SERVER_INTEGRATION_REQUIREMENTS.md` 为准。

本文只整理 `screen/` 大屏使用的接口。消息名称来自统一协议，具体 payload 是大屏当前实现所需的最小结构。服务端确认后，应把最终结构同步到 `docs/content/spec-协议.md`，再更新本文件和 `js/adapter.js`。

## 1. 连接信息

| 项目 | 当前约定 |
| --- | --- |
| 协议 | WebSocket |
| 地址 | `ws://127.0.0.1:9000` |
| 编码 | UTF-8 |
| 数据格式 | JSON 文本 |
| 大屏权限 | 免登录、只读 |
| 心跳建议 | 15 秒一次，连续 3 次失败断开重连 |
| 大屏配置位置 | `js/config.js` |

`127.0.0.1` 表示服务端和浏览器在同一台电脑。若服务端部署到其他电脑，需要改为服务端实际 IP，并确认防火墙与 WebSocket 监听地址。

## 2. 统一消息信封

### 2.1 客户端请求

```json
{
  "type": "screen.snapshot",
  "seq": 1,
  "payload": {}
}
```

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | string | 是 | 消息类型 |
| `seq` | integer | 是 | 大屏自增请求序号 |
| `payload` | object | 是 | 请求参数，无参数时为 `{}` |

### 2.2 服务端响应

```json
{
  "type": "screen.snapshot_resp",
  "seq": 1,
  "code": 0,
  "message": "ok",
  "payload": {}
}
```

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `type` | string | 是 | 响应消息类型 |
| `seq` | integer | 是 | 必须回填对应请求的 `seq` |
| `code` | integer | 是 | `0` 成功，非 0 为错误 |
| `message` | string | 是 | 可读错误或成功信息 |
| `payload` | object | 是 | 成功时的数据对象 |

### 2.3 服务端主动推送

```json
{
  "type": "push.alarm",
  "payload": {}
}
```

推送不需要 `seq`。大屏只读取推送，不回复业务处理结果。

## 3. 接口总表

| 消息 | 方向 | 必需 | 用途 |
| --- | --- | --- | --- |
| `screen.snapshot` | 大屏 → 服务端 | 是 | 请求全部页面初始数据 |
| `screen.snapshot_resp` | 服务端 → 大屏 | 是 | 返回全部页面初始数据 |
| `system.ping` | 大屏 → 服务端 | 是 | 心跳检测 |
| `system.pong` | 服务端 → 大屏 | 是 | 心跳响应 |
| `push.charger_status` | 服务端 → 大屏 | 是 | 电桩实时状态变化 |
| `push.alarm` | 服务端 → 大屏 | 是 | 新设备或站点告警 |
| `push.order_event` | 服务端 → 大屏 | 是 | 实时平台事件流 |
| `push.forecast` | 服务端 → 大屏 | 是 | 更新负荷预测序列 |
| `ml.forecast` | 大屏 → 服务端 | 可选 | 主动查询指定预测范围 |
| `ml.forecast_resp` | 服务端 → 大屏 | 可选 | 返回主动查询的预测结果 |

## 4. 全量快照

### 4.1 请求：`screen.snapshot`

发送时机：

1. WebSocket 第一次连接成功。
2. 断线重连成功。
3. 服务端明确要求客户端重新同步。

```json
{
  "type": "screen.snapshot",
  "seq": 1,
  "payload": {}
}
```

### 4.2 响应：`screen.snapshot_resp`

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

### 4.3 `metrics` 核心指标

| 字段 | 类型 | 单位 | 口径 |
| --- | --- | --- | --- |
| `today_revenue` | number | 元 | 今日已结算订单 `pay_amount` 合计 |
| `today_orders` | integer | 笔 | 今日订单数，需服务端确认包含哪些状态 |
| `charging_count` | integer | 台 | 当前 `charger.status = charging` 的桩数 |
| `online_rate` | number | % | 在线桩数 / 总桩数 × 100，当前按 0–100 返回 |
| `revenue_change_pct` | number | % | 今日营收较昨日同期变化，可为负数 |
| `orders_change_pct` | number | % | 今日订单较昨日同期变化，可为负数 |

待确认：`today_orders` 是全部创建订单、已开始订单，还是已完成订单。服务端和管理端必须采用同一口径。

### 4.4 `stations[]` 站点分布

```json
{
  "id": 1,
  "name": "软件园快充站",
  "longitude": 123.43,
  "latitude": 41.77,
  "load_rate": 88,
  "idle_chargers": 2,
  "total_chargers": 18,
  "today_revenue": 5260
}
```

| 字段 | 类型 | 单位 | 说明 |
| --- | --- | --- | --- |
| `id` | integer | — | `station.id`，唯一 |
| `name` | string | — | 站点名称 |
| `longitude` | number | 度 | 经度 |
| `latitude` | number | 度 | 纬度 |
| `load_rate` | number | % | 当前站点负载，范围 0–100 |
| `idle_chargers` | integer | 台 | 空闲桩数 |
| `total_chargers` | integer | 台 | 总桩数 |
| `today_revenue` | number | 元 | 当前站点今日已结算营收 |

负载颜色建议：`<60%` 绿色、`60%–85%` 黄色、`>85%` 红色。

### 4.5 `load_series.actual[]` 实际负荷

```json
{
  "timestamp": "2026-09-02 18:00:00",
  "value_kw": 875.2
}
```

| 字段 | 类型 | 单位 | 说明 |
| --- | --- | --- | --- |
| `timestamp` | string | — | `YYYY-MM-DD HH:MM:SS` 或双方确认的 ISO 8601 |
| `value_kw` | number | kW | 全网或所选站点实际功率 |

### 4.6 `load_series.forecast[]` 预测负荷

```json
{
  "timestamp": "2026-09-02 18:05:00",
  "value_kw": 902.4,
  "lower_kw": 866.1,
  "upper_kw": 938.7
}
```

| 字段 | 类型 | 单位 | 说明 |
| --- | --- | --- | --- |
| `timestamp` | string | — | 预测时间点 |
| `value_kw` | number | kW | 预测功率 |
| `lower_kw` | number | kW | 置信区间下界 |
| `upper_kw` | number | kW | 置信区间上界 |

### 4.7 `utilization_rank[]` 利用率排名

```json
{
  "station_id": 1,
  "station_name": "软件园快充站",
  "utilization_rate": 88
}
```

服务端按统一口径计算利用率；大屏只负责降序显示前 10 项，不自行从原始订单重算。

### 4.8 `alarms[]` 告警

```json
{
  "id": 101,
  "station_id": 1,
  "station_name": "软件园快充站",
  "charger_id": 7,
  "type": "overheat",
  "level": "critical",
  "status": "open",
  "occur_time": "2026-09-02 18:03:00"
}
```

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `id` | integer | 是 | `alarm.id`，用于去重 |
| `station_id` | integer | 是 | 关联电站 |
| `station_name` | string | 建议 | 避免大屏额外查名称 |
| `charger_id` | integer/null | 是 | 空表示站点级告警 |
| `type` | string | 是 | 告警类型枚举 |
| `level` | string | 是 | `info / warning / critical` |
| `status` | string | 建议 | `open / handled` |
| `occur_time` | string | 是 | 告警发生时间 |

### 4.9 `user_growth[]` 用户增长

```json
{
  "date": "2026-09-02",
  "new_users": 96
}
```

按 `user.register_time` 聚合；日期缺失时服务端或适配器补 0。

### 4.10 `energy_by_price_level` 峰平谷电量

| 字段 | 类型 | 单位 | 说明 |
| --- | --- | --- | --- |
| `valley` | number | kWh | 谷时充电量 |
| `flat` | number | kWh | 平时充电量 |
| `peak` | number | kWh | 峰时充电量 |

三项必须属于同一统计时间范围，合计等于该范围总充电量。

### 4.11 `events[]` 实时事件历史

```json
{
  "id": "evt-1001",
  "event_time": "2026-09-02 18:05:00",
  "event_type": "order_started",
  "text": "用户 138****8241 在软件园快充站 A03 开始充电"
}
```

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `id` | string/integer | 稳定唯一 ID，用于去重 |
| `event_time` | string | 事件发生时间 |
| `event_type` | string | 可选的结构化事件类型 |
| `text` | string | 已脱敏的展示文案 |

服务端不得发送完整手机号。大屏使用 `textContent` 展示，不执行消息中的 HTML。

## 5. 心跳接口

### 5.1 `system.ping`

```json
{
  "type": "system.ping",
  "seq": 2,
  "payload": {
    "timestamp": "2026-09-02T18:00:00.000Z"
  }
}
```

### 5.2 `system.pong`

协议文档把 ping/pong 定义为双向系统消息。当前大屏兼容推送式 pong：

```json
{
  "type": "system.pong",
  "payload": {
    "timestamp": "2026-09-02T18:00:00.000Z"
  }
}
```

待确认：服务端是否返回 `seq/code/message`。无论采用哪种形式，收到 pong 后必须清零心跳失败计数。

## 6. 实时推送接口

### 6.1 `push.charger_status`

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

已在协议确定的字段：`charger_id`、`status`、`soc`、`power_kw`、`temperature`。

大屏建议服务端额外提供：`station_id` 和最新 `metrics`。如果不提供聚合指标，双方必须约定收到设备状态后重新请求快照的节流频率，否则核心卡无法准确同步。

### 6.2 `push.alarm`

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
      "occur_time": "2026-09-02 18:10:00"
    }
  }
}
```

同一 `alarm.id` 重复推送时，大屏更新原记录，不新增重复行。

### 6.3 `push.order_event`

```json
{
  "type": "push.order_event",
  "payload": {
    "event": {
      "id": "evt-1002",
      "event_time": "2026-09-02 18:12:00",
      "event_type": "order_completed",
      "text": "用户 189****4421 完成充电并结算"
    }
  }
}
```

待服务端确认 `event` 是结构化对象还是单纯字符串。推荐采用上述结构化对象。

### 6.4 `push.forecast`

```json
{
  "type": "push.forecast",
  "payload": {
    "station_id": null,
    "horizon": 6,
    "series": [
      {
        "timestamp": "2026-09-02 19:00:00",
        "value_kw": 930.5,
        "lower_kw": 890.0,
        "upper_kw": 970.0
      }
    ]
  }
}
```

`station_id = null` 建议表示全网预测。`horizon` 单位为小时，可取 `1 / 6 / 24`。

## 7. 可选预测查询

### 7.1 `ml.forecast`

```json
{
  "type": "ml.forecast",
  "seq": 3,
  "payload": {
    "station_id": null,
    "horizon": 6
  }
}
```

### 7.2 `ml.forecast_resp`

```json
{
  "type": "ml.forecast_resp",
  "seq": 3,
  "code": 0,
  "message": "ok",
  "payload": {
    "station_id": null,
    "horizon": 6,
    "forecast": []
  }
}
```

如果快照和 `push.forecast` 已满足大屏需求，可以暂不实现主动查询，以减少接口数量。

## 8. 大屏使用的枚举

### 8.1 电桩状态

| 值 | 中文 | 颜色 |
| --- | --- | --- |
| `idle` | 空闲 | 绿色 |
| `charging` | 充电中 | 蓝色 |
| `reserved` | 已预约 | 黄色 |
| `fault` | 故障 | 红色 |
| `offline` | 离线 | 灰色 |
| `rebooting` | 重启中 | 过渡状态 |

### 8.2 告警级别

`info / warning / critical`

### 8.3 告警类型

`comm_abnormal / overheat / power_drop / offline / user_behavior`

### 8.4 电价档位

`valley / flat / peak`

未知枚举不得默认显示为正常状态，应显示“未知”并记录开发期警告。

## 9. 大屏可能收到的错误码

| code | 含义 | 大屏处理 |
| --- | --- | --- |
| `0` | 成功 | 正常更新 |
| `4001` | 数据不存在 | 显示空状态 |
| `5001` | 预测/模型不可用 | 保留实际曲线，显示预测不可用 |
| `9001` | 消息格式错误 | 显示接口错误，不破坏已有数据 |
| `9002` | 未知消息类型 | 记录错误，检查两端协议版本 |

其他业务错误码原则上不应由只读大屏触发。

## 10. 服务端交付前必须回答的问题

1. WebSocket 最终地址、端口和监听网卡是什么？
2. `screen.snapshot_resp.payload` 是否采用本文结构？
3. `today_orders` 的状态统计范围是什么？
4. `online_rate` 返回 0–1 还是 0–100？
5. 站点 `load_rate` 和 `utilization_rate` 的公式与时间窗口是什么？
6. 所有时间字段采用本地时间还是 UTC，格式是什么？
7. `push.charger_status` 是否提供聚合后的核心指标？
8. `push.order_event.event` 是字符串还是结构化对象？唯一 ID 如何生成？
9. 预测序列字段、置信区间和 `station_id = null` 的含义是否确认？
10. `system.pong` 是否带 `seq/code/message`？
11. 大屏首次连接和重连后是否允许立即请求快照？
12. 服务端是否会对大屏连接做来源限制或鉴权？

这些问题确认后，通常只需修改 `js/config.js` 和 `js/adapter.js`；如果消息名称或生命周期发生变化，才修改 `js/websocket-client.js` 与 `js/store.js`。
