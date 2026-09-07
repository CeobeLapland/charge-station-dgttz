# Web 大屏服务端对接说明

版本：v2.0

适用模块：`screen/` 电动汽车充电运营数据大屏

阅读对象：服务端、数据库、Qt 管理端和大屏开发人员

本文是 `screen/` 目录唯一的接口与联调说明，覆盖通信规则、快照字段、实时推送、统计口径、当前差距和联调步骤。大屏是免登录、只读 Web 端，所有业务指标和聚合结果必须由服务端产生，前端只负责数据校验、展示级格式化、状态保存和交互渲染。

## 1. 当前结论

前端已经完成 WebSocket 连接、心跳、重连、全量快照、增量推送、筛选请求和 ECharts 渲染。

现有服务端已经支持 `screen.snapshot`、`system.ping`、部分 `push.charger_status` 和 `push.order_event`，但当前返回的还是旧版大屏结构。要让新版页面完整显示，服务端需要扩展快照字段、实现筛选、补充告警推送，并在业务推送中携带重新计算后的聚合结果。

### 实现状态

| 能力 | 前端 | 当前服务端 | 联调结论 |
| --- | --- | --- | --- |
| `screen.snapshot` | 已发送并渲染新版结构 | 已实现旧版结构 | 需要服务端升级 payload |
| `system.ping/pong` | 已实现 | 已实现 | 可直接联调 |
| `push.charger_status` | 已处理并做 200ms 渲染节流 | 已推送单桩状态 | 需要增加服务端聚合片段 |
| `push.alarm` | 已处理、按 ID 去重 | 未发现业务广播 | 需要服务端实现 |
| `push.order_event` | 已处理、按 ID 去重 | 开始和结算时已推送 | 需要增加聚合片段 |
| 日期、区域、充电站筛选 | 已发送筛选参数 | 只处理旧版 `hours/days` | 需要服务端实现 |
| 60 秒兜底快照 | 已实现 | 可以响应快照 | 可用，但不能替代实时推送 |

> 上表中的消息名称以第 4、5 节定义为准。

## 2. 连接与运行方式

| 项目 | 约定 |
| --- | --- |
| 协议 | WebSocket，UTF-8 JSON 文本 |
| 默认地址 | `ws://127.0.0.1:9000` |
| 权限 | 免登录、只读 |
| 页面联调入口 | `index.html?mode=live` |
| 页面演示入口 | `index.html`，使用 mock 数据 |
| 心跳周期 | 15 秒 |
| 断线判定 | 连续 3 次收不到 `system.pong` |
| 重连方式 | 有上限的指数退避 |
| 全量同步 | 首次连接、重连、筛选变化和 60 秒兜底刷新 |
| 日常更新 | 优先使用 `push.*` 增量推送 |

`127.0.0.1` 只适用于浏览器与服务端在同一台电脑的情况。跨机器部署时，应把 `screen/js/config.js` 中的地址改为服务端局域网 IP，并放行 TCP 9000 端口。HTTPS 页面必须使用 `wss://`，否则浏览器会阻止混合内容。

正式答辩环境应将 ECharts 固定版本放入本地 `screen/vendor/`，避免把 CDN 作为唯一来源。

## 3. 统一消息信封

### 3.1 请求

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
| `seq` | integer | 是 | 客户端递增请求序号 |
| `payload` | object | 是 | 无参数时传 `{}` |

### 3.2 响应

```json
{
  "type": "screen.snapshot_resp",
  "seq": 1,
  "code": 0,
  "message": "ok",
  "payload": {}
}
```

响应必须回填请求的 `seq`。成功使用 `code=0`；失败使用非零 `code`，并提供可读的 `message`。前端会忽略已经过期的快照响应，防止快速切换筛选时旧数据覆盖新数据。

### 3.3 服务端主动推送

```json
{
  "type": "push.alarm",
  "payload": {}
}
```

推送不要求 `seq`，但必须包含有效的 `type` 和对象类型的 `payload`。

### 3.4 数据通用规则

- 时间统一使用本地时间 `yyyy-MM-dd HH:mm:ss`，不使用 UTC 或带 `Z` 的字符串。
- 数值使用 JSON number，不发送带单位的字符串。
- 空列表返回 `[]`，空对象返回 `{}`，不要用字符串表示空值。
- `0` 是合法业务值，不能用缺失字段代替。
- 实体 ID 在同一环境内必须稳定唯一。
- 未知枚举不能默认映射为“正常”。

## 4. 心跳

客户端每 15 秒发送：

```json
{
  "type": "system.ping",
  "seq": 2,
  "payload": {
    "timestamp": "2026-09-07 14:30:00"
  }
}
```

服务端应立即返回完整响应信封：

```json
{
  "type": "system.pong",
  "seq": 2,
  "code": 0,
  "message": "ok",
  "payload": {
    "timestamp": "2026-09-07 14:30:00"
  }
}
```

收到任意合法 `system.pong` 后，前端清零未响应次数。连续 3 次未收到时，前端主动关闭连接并重连；重连成功后重新请求全量快照。

## 5. 全量快照 `screen.snapshot`

### 5.1 请求参数

```json
{
  "type": "screen.snapshot",
  "seq": 3,
  "payload": {
    "date": "2026-09-07",
    "region": "浑南区",
    "station_id": "ST001"
  }
}
```

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `date` | string | 否 | `yyyy-MM-dd`；空字符串表示默认业务日期/不限 |
| `region` | string | 否 | 区域名称；空字符串表示全部区域 |
| `station_id` | string/integer | 否 | 充电站 ID；空字符串表示全部站点 |

三个筛选条件必须同时作用于六项指标、状态分布、趋势、排行、地图和告警。`filter_options` 本身应返回当前用户可以选择的完整有效选项，避免选择一次后候选项消失。

当前服务端只读取旧参数 `hours` 和 `days`，尚未处理上述三个筛选字段，需要新增实现。

### 5.2 完整响应

```json
{
  "type": "screen.snapshot_resp",
  "seq": 3,
  "code": 0,
  "message": "ok",
  "payload": {
    "metrics": {
      "station_count": 18,
      "charger_count": 326,
      "online_charger_count": 302,
      "today_energy_kwh": 28642.6,
      "today_orders": 1248,
      "today_revenue": 53680.20
    },
    "status_distribution": {
      "idle": 146,
      "charging": 156,
      "offline": 18,
      "fault": 6
    },
    "order_trend": [
      {"date": "2026-09-01", "order_count": 1068}
    ],
    "energy_revenue_trend": [
      {"date": "2026-09-01", "energy_kwh": 23680.5, "revenue": 44320.8}
    ],
    "station_rank": [
      {
        "station_id": "ST001",
        "station_name": "滨江充电站",
        "today_energy_kwh": 3860.2,
        "today_orders": 168
      }
    ],
    "stations": [
      {
        "id": "ST001",
        "name": "滨江充电站",
        "region": "浑南区",
        "longitude": 123.43,
        "latitude": 41.77,
        "status": "online",
        "status_text": "正常运行"
      }
    ],
    "alarms": [
      {
        "id": "A1001",
        "occur_time": "2026-09-07 14:28:11",
        "station_id": "ST001",
        "station_name": "滨江充电站",
        "charger_id": 18,
        "charger_code": "CH-018",
        "content": "枪线温度过高",
        "level": "warning"
      }
    ],
    "events": [
      {
        "id": "evt-o1001-order_started",
        "occur_time": "2026-09-07 14:25:10",
        "type": "order",
        "content": "用户 138****2301 开始充电"
      }
    ],
    "filter_options": {
      "regions": ["浑南区", "和平区"],
      "stations": [
        {"id": "ST001", "name": "滨江充电站"}
      ]
    }
  }
}
```

### 5.3 字段说明

#### 核心指标 `metrics`

| 字段 | 类型 | 单位 | 统计口径 |
| --- | --- | --- | --- |
| `station_count` | integer | 座 | 筛选范围内充电站总数 |
| `charger_count` | integer | 台 | 筛选范围内电桩总数 |
| `online_charger_count` | integer | 台 | 排除 `offline`、`fault` 后的在线桩数 |
| `today_energy_kwh` | number | kWh | 当日充电电量，具体订单状态范围与 Qt 端统一 |
| `today_orders` | integer | 笔 | 当日按 `create_time` 创建的订单总数，不区分最终状态 |
| `today_revenue` | number | 元 | 当日已结算订单 `pay_amount` 合计 |

如果 Qt 管理端另外展示在线率，由服务端按 `online_charger_count / charger_count * 100` 计算；前端不自行计算。

#### 设备状态 `status_distribution`

四个字段分别对应 `idle`、`charging`、`offline`、`fault`。服务端返回数量，前端图表可以进行纯展示所需的扇区绘制，但不得重新定义状态归类。四项合计应与同一筛选范围内的电桩总数一致。

#### 趋势数据

- `order_trend`：最近 7 个自然日，按日期升序；缺失日期由服务端补 0。
- `energy_revenue_trend`：最近 7 个自然日，按日期升序；电量为 kWh，收入为元。
- 所有趋势必须使用与指标一致的筛选范围和业务口径。

#### 站点排行 `station_rank`

服务端按照约定的当前排行口径降序返回，至少包含前 5 项。前端只截取前 5 条，不自行排序。当前页面默认展示 `today_energy_kwh`；同时保留 `today_orders`，便于后续切换展示口径。

#### 地图 `stations`

每个站点必须提供经纬度、站点名称和服务端判定的运行状态。点击和悬停只展示服务端返回的信息，前端不根据单桩数据推导站点状态。

#### 告警 `alarms`

- `id` 必须稳定唯一，用于去重。
- `charger_id` 对于站点级告警应显式传 JSON `null`。
- 推荐同时返回面向展示的 `charger_code` 和 `content`，避免前端维护业务枚举文案。
- `level` 只使用 `info`、`warning`、`critical`。

#### 事件 `events`

事件 ID 必须稳定唯一。手机号必须由服务端提前脱敏，前端直接使用安全文本节点展示，不进行二次脱敏，也不执行 HTML。

事件暂不占用当前首页独立模块，但保存在统一状态层中，供后续事件中心或业务扩展使用。

#### 筛选项 `filter_options`

`regions` 返回区域名称数组；`stations` 返回 ID 与名称。站点 ID 的 JSON 类型应在快照、筛选、排行和推送中保持一致。

## 6. 实时推送

推送中凡是会影响统计结果的业务变化，都应携带服务端重新计算后的完整聚合片段。前端不会对订单、电量、收入、在线数量或状态比例做加减运算。

### 6.1 电桩状态 `push.charger_status`

```json
{
  "type": "push.charger_status",
  "payload": {
    "charger_id": 18,
    "station_id": "ST001",
    "status": "charging",
    "soc": 42.5,
    "power_kw": 60.2,
    "temperature": 39.8,
    "metrics": {
      "online_charger_count": 302
    },
    "status_distribution": {
      "idle": 145,
      "charging": 157,
      "offline": 18,
      "fault": 6
    },
    "stations": []
  }
}
```

`metrics` 只需携带受影响的指标；`status_distribution` 和 `stations` 携带更新后的完整模块数据。前端对这类高频消息做 200ms 合并渲染，只采用窗口内最后一条消息，因此服务端每条消息必须包含可独立使用的最新聚合状态，不能只发送差值。

现有服务端只发送 `charger_id`、`station_id`、`status` 等单桩字段，无法立即更新核心指标和图表。若不扩展推送，页面只能等下一次 60 秒兜底快照。

### 6.2 新增告警 `push.alarm`

```json
{
  "type": "push.alarm",
  "payload": {
    "alarm": {
      "id": "A1002",
      "occur_time": "2026-09-07 14:31:08",
      "station_id": "ST001",
      "station_name": "滨江充电站",
      "charger_id": 6,
      "charger_code": "CH-006",
      "content": "设备离线",
      "level": "critical"
    },
    "metrics": {
      "online_charger_count": 301
    },
    "status_distribution": {
      "idle": 145,
      "charging": 156,
      "offline": 19,
      "fault": 6
    }
  }
}
```

同一告警 ID 重复推送时，前端更新已有记录，不新增重复行。新增告警入库成功后应立即推送；如果告警同时改变设备状态，应在同一 payload 提供受影响的聚合数据。

### 6.3 订单事件 `push.order_event`

```json
{
  "type": "push.order_event",
  "payload": {
    "event": {
      "id": "evt-o1002-order_completed",
      "occur_time": "2026-09-07 14:31:20",
      "type": "order_completed",
      "content": "用户 139****1028 完成充电并结算"
    },
    "metrics": {
      "today_orders": 1249,
      "today_energy_kwh": 28645.8,
      "today_revenue": 53686.40
    },
    "order_trend": [],
    "energy_revenue_trend": [],
    "station_rank": []
  }
}
```

订单创建、开始、完成、结算等会改变页面数据的节点均应推送。趋势和排行字段一旦携带，必须是更新后的完整数组，不是单个新增点或差值。

现有服务端事件字段使用 `event_time`、`event_type`、`text`，而当前前端规范使用 `occur_time`、`type`、`content`。服务端可以统一改为本文格式；如果为了兼容其他端必须保留旧格式，则需在 `screen/js/adapter.js` 增加明确映射。两种格式不能长期并存而不标版本。

### 6.4 预测接口

当前页面不展示预测数据，不请求 `ml.forecast`，也不消费 `push.forecast`。服务端无需为本次大屏验收实现预测能力。

## 7. 统计口径

| 数据 | 统一口径 |
| --- | --- |
| 今日时间范围 | 本地时间当天 `00:00:00` 至当前时刻 |
| 今日收入 | 已结算订单的 `pay_amount` 合计 |
| 今日订单 | 今日按 `create_time` 创建的全部订单，不区分最终状态 |
| 在线桩 | 排除 `offline` 和 `fault` 状态 |
| 在线率 | 在线桩数 / 总桩数 × 100，由服务端计算 |
| 今日充电量 | 由服务端与 Qt 端统一订单状态范围后返回 |
| 状态分布 | 同一筛选范围内按服务端标准状态归类 |
| 近 7 日趋势 | 最近 7 个本地自然日，缺失日期补 0 |
| TOP5 排行 | 服务端排序并返回，前端不重新排序 |
| 峰平谷电量 | 若以后恢复该模块，统计近 7 日已结算订单，不是仅统计当日 |

所有口径变更应先同步服务端与 Qt 管理端，再调整本文和前端适配层。禁止前端为了“对上数字”单独修改公式。

## 8. 空数据和异常响应

- 数据为空时仍返回完整 payload 结构：数值为 `0`、数组为 `[]`、对象保留规定字段。
- 查询失败不得返回部分成功数据并使用 `code=0`。
- 非零错误响应必须保留原请求 `seq`，前端保留上一份有效数据并显示错误提示。
- 未知消息可以返回 `code=9002`；格式错误可以返回 `code=9001`。
- 单个站点缺少经纬度时，服务端应明确为空或过滤，并记录日志，不能使用 `(0,0)` 假坐标。
- 服务端不得向页面返回完整手机号、密码、令牌、SQL 或内部堆栈。

## 9. 服务端需要完成的工作

按优先级排列：

1. 将 `screen.snapshot_resp.payload` 从旧字段升级为第 5 节的新版完整结构。
2. 实现 `date`、`region`、`station_id` 筛选，并让筛选同步作用于全部模块。
3. 增加六项核心指标、设备状态分布、两组近 7 日趋势、TOP5 排行和筛选选项的服务端查询及聚合。
4. 为地图站点补充 `region`、`status`、`status_text`；状态由服务端判定。
5. 告警补充 `charger_code`、可直接展示的 `content`，站点级告警显式返回 `charger_id:null`。
6. 新告警入库后广播 `push.alarm`。
7. 扩展 `push.charger_status`，携带最新 `metrics`、`status_distribution` 和必要的站点数据。
8. 扩展 `push.order_event`，携带最新指标、趋势和排行；统一事件字段命名。
9. 确保大屏与 Qt 管理端调用相同 DAO/统计函数，避免两端重复实现口径。
10. 确认推送范围，避免把只供大屏的大体积聚合数据广播给不需要的客户端；必要时记录客户端类型或提供订阅机制。

## 10. 联调步骤

### 10.1 启动服务端

按照服务端项目说明完成数据库初始化、编译并启动，确认服务端监听 TCP 9000。

### 10.2 启动页面

在 `screen/` 目录启动任意静态文件服务，然后访问：

```text
http://127.0.0.1:8080/index.html?mode=live
```

直接打开 `index.html` 或不带 `mode=live` 时使用 mock 数据，不连接服务端。

### 10.3 浏览器检查

- 页面连接状态显示“实时连接”。
- 服务端日志收到 `screen.snapshot`。
- 快照响应 `seq` 与请求一致，`code=0`。
- 页面显示最后更新时间，六项指标和所有图表均有数据或明确空状态。

## 11. 验收清单

- [ ] 六项核心指标与 Qt 管理端完全一致。
- [ ] 状态分布四项合计与电桩总数一致。
- [ ] 两组趋势均为最近 7 个自然日，日期顺序和空日期补零正确。
- [ ] TOP5 排名由服务端排序，页面顺序与服务端一致。
- [ ] 地图站点位置、名称、运行状态正确。
- [ ] 日期、区域、站点筛选能够同步刷新全部模块。
- [ ] 电桩状态变化后，无需刷新浏览器即可更新。
- [ ] 新告警入库后，大屏立即显示相同告警 ID。
- [ ] 订单创建、开始或结算后，指标和趋势按服务端结果更新。
- [ ] 重复事件与重复告警不会生成两行。
- [ ] 停止服务端后页面进入重连状态；恢复后自动重连并重新获取快照。
- [ ] 连续运行时没有因为高频推送造成明显卡顿。
- [ ] 1920×1080 和常用桌面分辨率下无重叠、截断或溢出。
- [ ] 断网环境下 ECharts 能从本地资源加载。

## 12. 双方最后需要确认

1. `today_energy_kwh` 包含哪些订单状态，以及与 Qt 管理端共用的统计函数。
2. `station_id` 最终统一使用 JSON integer 还是 string。
3. TOP5 默认按今日充电量还是今日订单数排序。
4. 筛选日期是单日还是后续扩展为起止日期。
5. 推送中采用本文事件字段，还是由前端兼容现有 `event_time/event_type/text`。
6. Web 页面与服务端是否同机部署；若不同，确认正式 WebSocket 地址、防火墙和 `ws/wss`。

以上六项确认后，应冻结为同一协议版本，并同步给大屏、服务端和 Qt 管理端负责人。
