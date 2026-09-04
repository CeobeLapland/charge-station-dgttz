# spec-数据库（冻结真相源）

**!!!不代表终版，只是参考!!!**

本文件是全仓库表结构、字段、枚举的唯一权威。任何端需要字段名、类型、枚举值时从这里取值，禁止各自硬编码。改动本文件属于"解冻"，须先改这里再通知全组同步（见 `AGENTS.md`）。全量字段的干净对照版见 `docs/content/DATA_STRUCTURE.md`。

## 总体约定

- 数据库采用 QSQLite 单文件部署，所有业务数据由服务端统一读写，用户端、管理端与 Web 大屏经服务端 WebSocket 间接访问，不直连数据库文件。
- 表名、字段名使用英文小写蛇形命名。
- 主键统一为自增整数 `id`；外键以 `xxx_id` 命名并指向对应表主键。
- 字段类型沿用 SQLite 三类：INTEGER、REAL、TEXT（日期时间与 JSON 文本均存 TEXT）。
- 时间统一格式：`YYYY-MM-DD HH:MM:SS`。
- JSON 数组/对象一律存 TEXT（如 `facilities`、`forecast`、`condition`、`action_params`）。
- 布尔一律用 INTEGER 0/1（如 `is_default`、`is_read`、`notified`、`enabled`、`has_swap`）。

## 实体关系

```mermaid
erDiagram
    USER ||--o{ CHARGING_ORDER : 产生
    USER ||--o{ RESERVATION : 预约
    USER ||--o{ POINT_RECORD : 获得
    USER ||--o{ WALLET_TRANSACTION : 有流水
    USER ||--o{ VEHICLE : 拥有
    USER ||--o{ REVIEW : 评价
    USER ||--o{ WORK_ORDER : 提交
    USER ||--o{ USER_COUPON : 持有
    USER ||--o{ USER_PLAN : 订阅
    USER ||--o{ NOTIFICATION : 接收
    USER ||--o{ FAVORITE : 收藏
    USER ||--o{ SWAP_ORDER : 换电(预留)
    COUPON ||--o{ USER_COUPON : 实例化
    MEMBER_PLAN ||--o{ USER_PLAN : 订阅
    STATION ||--o{ CHARGER : 拥有
    STATION ||--o{ CHARGING_ORDER : 承载
    STATION ||--o{ PRICE_RULE : 定价
    STATION ||--o{ REVIEW : 被评
    STATION ||--o{ COUPON : 适用
    STATION ||--o{ STATION_POST : 有帖
    STATION ||--o{ FAVORITE : 被藏
    STATION ||--o{ BATTERY : 换电(预留)
    MERCHANT ||--o{ STATION : 合作
    CHARGER ||--o{ CHARGING_ORDER : 执行
    CHARGER ||--o{ DEVICE_LOG : 记录
    CHARGER ||--o{ ALARM : 触发
    CHARGER ||--o{ CHARGING_MEASURE : 产出
    CHARGER ||--o{ WORK_ORDER : 关联
    CHARGING_ORDER ||--o{ ORDER_TIMELINE : 有节点
    VEHICLE ||--o{ CHARGING_ORDER : 关联
    COUPON ||--o{ CHARGING_ORDER : 核销
    STATION_POST ||--o{ POST_REPLY : 有回复
```

文字关系：

- 一座充电站拥有多台充电桩，可归属一个合作商户，可被多条价格规则、评价、优惠券、社区帖、收藏引用，可被多笔订单承载。
- 一个用户产生多笔充电订单、多条预约、多条积分记录、多条钱包流水、多辆车、多条评价、多张持有券、多个套餐订阅、多条通知、多条收藏，可提交多张工单。
- 一笔充电订单关联一个用户、一座电站、一台充电桩、一辆车（可空）、一张核销券（可空），并按时间顺序产生多条时间轴节点。
- 一台充电桩产生多条设备操作日志、多条告警、多条时序测量记录、多条关联工单。
- 一个券模板实例化为多个用户持有券；一个套餐模板被多个用户订阅为套餐记录。
- `station.owner_type` 为合作类型时关联 `merchant`；`station.has_swap=1` 表示该站支持换电，可挂载多块换电电池（belongs to 扩展表换电域）。

## 表清单总览

共 31 张表，三类：

| 类别 | 表 | 说明 |
| ---- | ---- | ---- |
| 核心表（16） | user、admin、station、charger、charging_order、reservation、price_rule、vehicle、review、work_order、device_log、alarm、point_record、charging_measure、coupon、user_coupon | 基础 + 全规格【增强】闭环必需，随服务端建库脚本必建 |
| 支撑表（5） | order_timeline、wallet_transaction、notification、weather、holiday | 规格【增强】功能落库必需，必建 |
| 扩展表（10） | merchant、rule_config、member_plan、user_plan、station_post、post_reply、favorite、faq、battery、swap_order | 增强功能池提升项，随库建（可按裁剪停用） |

## 核心表

### user（用户）

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER | 主键，自增 |
| phone | TEXT | 手机号，11 位，唯一 |
| nickname | TEXT | 昵称，默认"用户+后四位" |
| avatar_path | TEXT | 头像文件路径，默认空（灰色占位图） |
| balance | REAL | 钱包余额（元） |
| points | INTEGER | 累计可用积分 |
| level | TEXT | 会员等级：normal / vip / enterprise |
| status | TEXT | 状态：normal / frozen |
| credit_score | INTEGER | 信用分（默认 100，违约负向扣减；低于阈值限制预约） |
| register_time | TEXT | 注册时间（首次登录自动创建） |
| last_login_time | TEXT | 最近登录时间（可空） |

### admin（管理员）

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER | 主键，自增 |
| account | TEXT | 管理员账号，唯一 |
| password | TEXT | 登录密码 |

默认初始记录：`admin / 123456`。

### station（充电站）

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER | 主键，自增（即充电站 ID） |
| name | TEXT | 站名 |
| address | TEXT | 详细地址 |
| area | TEXT | 所属区域（区县/商圈名，如"商业区"），供区域筛选、需求预测、画像聚合 |
| longitude | REAL | 经度（腾讯地图地理编码结果） |
| latitude | REAL | 纬度 |
| total_chargers | INTEGER | 总电桩数 |
| online_rate | REAL | 当前在线率，服务端动态计算，口径见文末待定项 |
| service_fee | REAL | 服务费（元/kWh） |
| parking_fee | REAL | 停车费（元/小时，0 表示免费） |
| business_hours | TEXT | 营业时间，如 "00:00–24:00" |
| facilities | TEXT | 设施清单（JSON 数组，取值见枚举：washroom / convenience_store / rest_area / wifi / rain_shelter / underground_parking，可空数组） |
| owner_type | TEXT | 站点归属类型：self_run / franchise / partner / third_party |
| merchant_id | INTEGER | 外键，合作商户（自营时为空，FK merchant.id） |
| has_swap | INTEGER | 是否支持换电（0/1，配合扩展表 battery/swap_order 的换电域，默认 0） |

### charger（充电桩）

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER | 主键，自增 |
| code | TEXT | 电桩编号（如 A-023），站内唯一 |
| station_id | INTEGER | 外键，所属充电站 |
| type | TEXT | 类型：fast / slow |
| power | REAL | 额定功率（kW） |
| status | TEXT | 状态：idle / charging / reserved / fault / offline / rebooting |
| voltage | REAL | 当前电压（V，实时参数） |
| current | REAL | 当前电流（A，实时参数） |
| temperature | REAL | 当前温度（℃） |
| fault_code | TEXT | 故障码（空串表示正常，数字孪生/诊断展示） |
| comm_status | TEXT | 通信状态：normal / abnormal |
| health_score | INTEGER | 设备健康度（0–100，规则公式见 spec-机器学习） |
| total_charge_count | INTEGER | 累计充电次数 |
| total_charge_duration | INTEGER | 累计充电时长（分钟） |
| created_time | TEXT | 建桩时间 |

### charging_order（充电订单）

记录"预约—充电—计费—结算"全流程，各端状态机统一引用本表 `status`。

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER | 主键，自增 |
| user_id | INTEGER | 外键，下单用户 |
| station_id | INTEGER | 外键，目标充电站 |
| charger_id | INTEGER | 外键，充电电桩 |
| vehicle_id | INTEGER | 外键，关联车辆（可空，FK vehicle.id，供推荐/报告追溯） |
| status | TEXT | reserved / charging / pending_settle / completed / cancelled |
| start_soc | REAL | 起始电量（%），开始充电时写入 |
| target_soc | REAL | 目标电量（%），由推荐/用户约束推算 |
| end_soc | REAL | 结束电量（%），结束后写入 |
| start_time | TEXT | 开始充电时间 |
| end_time | TEXT | 结束充电时间 |
| duration_min | INTEGER | 充电时长（分钟） |
| energy_kwh | REAL | 充电量（kWh） |
| price_level | TEXT | 结算电价档位：valley / flat / peak |
| amount | REAL | 应收费用（元，电价 × 电量） |
| discount_amount | REAL | 优惠抵扣（元，优惠券面额 + 积分抵扣合计，默认 0） |
| pay_amount | REAL | 实付金额（元，= amount − discount_amount，余额实际扣减额） |
| points_used | INTEGER | 本单积分抵扣数（默认 0） |
| coupon_id | INTEGER | 外键，核销优惠券（可空，FK user_coupon.id，结算时携带） |
| points_earned | INTEGER | 本单获得积分 |
| create_time | TEXT | 订单创建时间 |
| settle_time | TEXT | 结算时间（可空） |
| reserved_time | TEXT | 预约生效/期望开始时间（立即预约为匹配时刻，定时预约为期望开始时刻） |
| scan_deadline | TEXT | 扫码启动截止时间（匹配后 + 保留窗口，超时未扫码转 cancelled） |
| cancel_reason | TEXT | 取消原因：user_cancel / no_show / timeout / admin（可空） |
| occupy_fee | REAL | 占位费（元，充满后未挪车按停车费累计，默认 0） |
| occupy_min | INTEGER | 占位时长（分钟，默认 0） |
| penalty_fee | REAL | 违约金（元，违约场景扣款，默认 0） |

### reservation（预约/排队）

支撑智能排队。

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER | 主键，自增 |
| user_id | INTEGER | 外键，预约用户 |
| station_id | INTEGER | 外键，目标电站 |
| charger_id | INTEGER | 外键，轮到时匹配的空闲桩（可空，匹配后回填，`push.reservation_notify` 携带） |
| queue_no | INTEGER | 排队序号 |
| reserve_time | TEXT | 进入排队时间 |
| estimate_start_time | TEXT | 预计开始充电时间 |
| notified | INTEGER | 是否已推送"轮到你"通知（0/1，幂等防重复推送） |
| status | TEXT | waiting / matched / cancelled |
| reserve_type | TEXT | 预约类型：immediate_queue（即时排队）/ timed（定时预约） |
| expect_time | TEXT | 期望开始时间（定时预约时刻 / 排队预测开始） |
| matched_time | TEXT | 匹配到空闲桩的时间（可空，matched 后回填） |
| expire_time | TEXT | 响应截止时间（matched 后 + 轮候窗口，超时顺延） |
| cancel_reason | TEXT | 取消原因：user_cancel / timeout / no_show（可空） |

### price_rule（分时电价）

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER | 主键，自增 |
| station_id | INTEGER | 外键，适用电站（可空：空表示全站通用规则） |
| level | TEXT | valley / flat / peak |
| price | REAL | 单价（元/kWh） |
| time_range | TEXT | 对应时段范围，如 "00:00–08:00" |

### vehicle（我的车辆）

用户可添加多辆车，支撑车型兼容过滤与所需电量估算。

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER | 主键，自增 |
| user_id | INTEGER | 外键，车主 |
| name | TEXT | 车辆名称（如"我的小鹏"） |
| type | TEXT | 车型：car / light_truck / two_wheeler / three_wheeler |
| battery_kwh | REAL | 电池容量（kWh） |
| connector_type | TEXT | 接口类型：ac_gb / dc_gb / other |
| max_power_kw | REAL | 最大支持功率（kW） |
| is_default | INTEGER | 是否默认车辆（0/1，推荐/费用估算默认取用，每用户至多一辆） |
| created_time | TEXT | 添加时间 |

### review（站点评价）

多维评分 + 标签 + 有用机制，供大屏/管理端反馈分析。

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER | 主键，自增 |
| user_id | INTEGER | 外键，评价用户 |
| station_id | INTEGER | 外键，被评电站 |
| order_id | INTEGER | 外键，关联订单（可空） |
| overall_score | REAL | 综合评分（0–5） |
| speed_score | REAL | 充电速度分 |
| device_score | REAL | 设备状况分 |
| parking_score | REAL | 停车便利分 |
| hygiene_score | REAL | 卫生环境分 |
| service_score | REAL | 服务态度分 |
| tags | TEXT | 标签列表（JSON 数组，取值见枚举，可含自由文本） |
| content | TEXT | 文字评价 |
| useful_count | INTEGER | 被点"有用"次数 |
| status | TEXT | 状态：normal / hidden |
| create_time | TEXT | 评价时间 |

### work_order（工单）

统一承载用户投诉、设备故障、退款、设备维护、异常订单等事件。

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER | 主键，自增 |
| type | TEXT | 类型：user_complaint / device_fault / refund / maintenance / abnormal_order |
| priority | TEXT | 优先级：low / medium / high |
| user_id | INTEGER | 外键，提交用户（设备/系统类可空） |
| station_id | INTEGER | 外键，关联电站（可空） |
| charger_id | INTEGER | 外键，关联电桩（可空） |
| title | TEXT | 标题 |
| description | TEXT | 描述 |
| status | TEXT | 状态：pending / processing / completed / closed |
| handler | TEXT | 处理管理员账号 |
| result | TEXT | 处理结果/备注 |
| create_time | TEXT | 创建时间 |
| handle_time | TEXT | 处理完成时间 |

### device_log（设备操作日志）

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER | 主键，自增 |
| charger_id | INTEGER | 外键，目标电桩 |
| action | TEXT | restart / pause / repair |
| operator | TEXT | 操作管理员账号 |
| op_time | TEXT | 操作时间 |
| result | TEXT | success / failed |

### alarm（告警）

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER | 主键，自增 |
| charger_id | INTEGER | 外键，告警电桩（可空，表示站点级告警） |
| station_id | INTEGER | 外键，告警电站 |
| type | TEXT | comm_abnormal / overheat / power_drop / offline / user_behavior |
| level | TEXT | info / warning / critical |
| occur_time | TEXT | 告警发生时间 |
| status | TEXT | open / handled |
| handle_action | TEXT | restart / pause / repair / ignore |

### point_record（积分记录）

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER | 主键，自增 |
| user_id | INTEGER | 外键，用户 |
| change | INTEGER | 积分变动（正为获得，负为抵扣） |
| reason | TEXT | charge / redeem |
| create_time | TEXT | 变动时间 |

### charging_measure（充电时序测量）

逐桩记录充电过程负荷与时序数据，是机器学习预测与健康度评估的数据源。

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER | 主键，自增 |
| charger_id | INTEGER | 外键，测量电桩 |
| station_id | INTEGER | 外键，所属电站 |
| measure_time | TEXT | 采样时间 |
| power_kw | REAL | 瞬时功率 |
| soc | REAL | 该桩当前 SOC |
| energy_delta_kwh | REAL | 本采样间隔充电量 |
| temperature | REAL | 温度（可空，供健康度温度趋势） |

### coupon（优惠券模板）

由管理端营销中心配置，可被多个用户领取后实例化为 user_coupon。

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER | 主键，自增 |
| title | TEXT | 券名（如"新人立减 ¥5"） |
| type | TEXT | 类型：new_user / full_reduction / night / site |
| discount_amount | REAL | 抵扣金额（元） |
| min_amount | REAL | 满减门槛（元，0 表示无门槛） |
| station_id | INTEGER | 适用电站（空为通用） |
| time_range | TEXT | 适用时段（如 "22:00–06:00"，空为不限；夜间券/时段券限定） |
| valid_days | INTEGER | 领取后有效天数 |
| total | INTEGER | 发放总数（-1 不限） |
| status | TEXT | 状态：active / inactive |
| create_time | TEXT | 创建时间 |

### user_coupon（用户持有券）

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER | 主键，自增 |
| user_id | INTEGER | 外键，持券用户 |
| coupon_id | INTEGER | 外键，券模板 |
| status | TEXT | 状态：unused / used / expired |
| receive_time | TEXT | 领取时间 |
| used_order_id | INTEGER | 核销订单（可空，FK charging_order.id） |

## 支撑表（规格【增强】落库必需）

### order_timeline（订单时间轴）

订单状态流转的节点式记录，支撑用户端「充电订单时间轴」与 `order.detail_resp.timeline[]`。

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER | 主键，自增 |
| order_id | INTEGER | 外键，关联订单 |
| node | TEXT | 节点类型：reserved / arrived / started / soc_50 / target_reached / finished / settled |
| label | TEXT | 展示文案（如"到达充电站"） |
| event_time | TEXT | 节点时间戳 |
| detail | TEXT | 补充信息（可空，如"目标电量 95%"） |

### wallet_transaction（钱包流水）

用户余额变动的逐笔流水，支撑充值记录、财务核查与池 P5「财务中心」。

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER | 主键，自增 |
| user_id | INTEGER | 外键，用户 |
| type | TEXT | recharge / consume / refund / other / penalty / occupy_fee |
| amount | REAL | 变动金额（元，正为入账，负为支出） |
| balance_after | REAL | 变动后余额 |
| order_id | INTEGER | 关联订单（可空，FK charging_order.id，消费/退款时回填） |
| remark | TEXT | 备注（如"余额充值""充电结算""退款"） |
| create_time | TEXT | 交易时间 |

### notification（站内消息）

用户端「我的 → 消息」的落库来源；`push.*` 通知可同时写库保证离线可查。

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER | 主键，自增 |
| user_id | INTEGER | 外键，接收用户 |
| type | TEXT | reservation / order / point / coupon / work_order / system |
| title | TEXT | 标题 |
| content | TEXT | 内容 |
| related_id | INTEGER | 关联业务 ID（订单/工单/券/预约等，可空） |
| is_read | INTEGER | 是否已读（0/1） |
| create_time | TEXT | 创建时间 |

### weather（模拟天气）

服务端定时器模拟的当前天气与短期预测，作为跨端一致展示（用户端天气推荐、大屏预测曲线、管理端告警）与负荷预测修正输入。

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER | 主键，自增 |
| area | TEXT | 区域（与 station.area 对齐；空表示全局默认天气） |
| condition | TEXT | 状况：sunny / cloudy / rain / hot / extreme |
| temperature | REAL | 温度（℃） |
| forecast | TEXT | 未来 N 小时预测（JSON 数组，可空） |
| update_time | TEXT | 最近刷新时间 |

### holiday（节假日）

负荷预测的外部维度输入（工作日/休息日/调休），服务端建库时按年份种子写入。

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER | 主键，自增 |
| date | TEXT | 日期（YYYY-MM-DD，唯一） |
| name | TEXT | 名称（如"春节"） |
| is_workday | INTEGER | 0=法定休息日，1=调休上班日 |
| create_time | TEXT | 记录时间 |

## 扩展表（增强功能池驱动，默认随库建）

### merchant（商户/合作站点管理）

池 P4「商户/站点合作管理」，配合 `station.owner_type`（池 P3）。自营站不产生商户记录。

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER | 主键，自增 |
| name | TEXT | 商户名称 |
| contact_name | TEXT | 联系人 |
| contact_phone | TEXT | 联系电话 |
| cooperation_type | TEXT | 合作类型：franchise / partner / third_party |
| status | TEXT | 状态：active / paused |
| order_count | INTEGER | 合作站累计订单数（汇总，可定时刷新） |
| settle_amount | REAL | 累计结算金额（元，汇总） |
| service_score | REAL | 服务评分均值（0–5，评价聚合） |
| remark | TEXT | 备注 |
| create_time | TEXT | 加入时间 |

### rule_config（规则引擎配置）

池 P7「规则引擎」：把 if-else 判断（空闲率低、健康度过低、排队过长等）配置化，规则命中后触发既定动作。

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER | 主键，自增 |
| name | TEXT | 规则名（如"空闲率低自动分流"） |
| object_type | TEXT | 对象类型：station / charger / user |
| condition | TEXT | 触发条件（JSON，如 {"field":"idle_rate","op":"<","value":20}） |
| action | TEXT | 动作：redirect / issue_coupon / create_alarm / create_work_order / limit_reservation |
| action_params | TEXT | 动作参数（JSON，发送券模板、告警级别等，可空） |
| enabled | INTEGER | 是否启用（0/1） |
| priority | INTEGER | 优先级（同对象多规则时按高→低匹配） |
| description | TEXT | 说明 |
| create_time | TEXT | 创建时间 |
| update_time | TEXT | 更新时间 |

### member_plan（会员套餐/月卡模板）

池「月卡/套餐」：畅充月卡等服务费减免型订阅产品。

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER | 主键，自增 |
| name | TEXT | 套餐名（如"畅充月卡"） |
| price | REAL | 售价（元/期） |
| valid_days | INTEGER | 生效天数 |
| service_fee_discount | REAL | 服务费折扣系数（0.8 表示 8 折，1 表示不折） |
| night_discount | REAL | 夜间充电额外折扣（可空，1 表示无） |
| points_multiplier | REAL | 积分倍率（可空，1 表示常规） |
| status | TEXT | 状态：active / inactive |
| description | TEXT | 说明 |
| create_time | TEXT | 创建时间 |

### user_plan（用户套餐订阅）

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER | 主键，自增 |
| user_id | INTEGER | 外键，订阅用户 |
| plan_id | INTEGER | 外键，套餐模板 |
| start_time | TEXT | 生效开始时间 |
| end_time | TEXT | 生效结束时间 |
| status | TEXT | 状态：active / expired / cancelled |
| create_time | TEXT | 订阅时间 |

### station_post（站点社区帖）

池「社区（轻量版）」：站点问答/动态，第一版只做只读展示。

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER | 主键，自增 |
| station_id | INTEGER | 外键，关联电站 |
| user_id | INTEGER | 外键，发帖用户 |
| content | TEXT | 内容 |
| like_count | INTEGER | 点赞数 |
| reply_count | INTEGER | 回复数 |
| status | TEXT | 状态：normal / hidden |
| create_time | TEXT | 发帖时间 |

### post_reply（帖子回复）

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER | 主键，自增 |
| post_id | INTEGER | 外键，所属帖子 |
| user_id | INTEGER | 外键，回复用户 |
| parent_id | INTEGER | 0=直接回复帖子；否则指向某条回复 id（两级即可） |
| content | TEXT | 内容 |
| status | TEXT | 状态：normal / hidden |
| create_time | TEXT | 回复时间 |

### favorite（站点收藏）

`我的 → 收藏` 数据源，也是画像「常去站/常去区域」与猜你喜欢的重要输入。

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER | 主键，自增 |
| user_id | INTEGER | 外键，用户 |
| station_id | INTEGER | 外键，收藏电站 |
| create_time | TEXT | 收藏时间 |

### faq（客服机器人问答库）

池「客服机器人」：按问题分类的固定应答，命中规则返回；查单/建工单走现有接口。

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER | 主键，自增 |
| category | TEXT | 分类（与工单分类/用户端问题分类对齐） |
| question | TEXT | 问题模板/关键词 |
| answer | TEXT | 应答内容 |
| sort | INTEGER | 排序 |
| enabled | INTEGER | 是否启用（0/1） |
| create_time | TEXT | 创建时间 |

## 扩展表（续）——换电域（P1 叙事配套）

> 换电（battery 换电 + swap_order 换电订单）随增强功能池 P1「定位升级」落地，挂靠在 `station.has_swap=1` 的换电站下。与充电域并行，实现时需在 `spec-协议.md` 补对应消息。

### battery（换电电池包）

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER | 主键，自增 |
| code | TEXT | 电池编号（站内唯一） |
| station_id | INTEGER | 外键，所属换电站 |
| soc | REAL | 当前电量（%） |
| health_score | INTEGER | 健康度（0–100） |
| temperature | REAL | 当前温度（℃） |
| status | TEXT | idle / swapping / charging / fault / offline |
| swap_count | INTEGER | 累计换电次数 |
| create_time | TEXT | 入库时间 |

### swap_order（换电订单）

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER | 主键，自增 |
| user_id | INTEGER | 外键，用户 |
| station_id | INTEGER | 外键，换电站 |
| battery_in_id | INTEGER | 外键，归还的旧电池 |
| battery_out_id | INTEGER | 外键，换入的新电池 |
| amount | REAL | 费用（元） |
| status | TEXT | completed / cancelled |
| create_time | TEXT | 时间 |

> 结算扣款同样写入 `wallet_transaction`（type=consume，remark="换电"）。

## 枚举与一致性约定

跨表复用的枚举统一取值，各端引用本表而非各自硬编码：

| 枚举 | 取值 |
| ---- | ---- |
| 用户状态 | normal / frozen |
| 会员等级 | normal / vip / enterprise |
| 订单状态 | reserved / charging / pending_settle / completed / cancelled |
| 电桩状态 | idle / charging / reserved / fault / offline / rebooting |
| 电桩类型 | fast / slow |
| 电价档位 | valley / flat / peak |
| 告警级别 | info / warning / critical |
| 告警类型 | comm_abnormal / overheat / power_drop / offline / user_behavior |
| 设备操作 | restart / pause / repair |
| 车型类型 | car / light_truck / two_wheeler / three_wheeler |
| 接口类型 | ac_gb / dc_gb / other |
| 评价标签 | fast_charge / spacious / quiet_night / high_parking_fee / old_device / good_fast_charge / heavy_queue / good_service（可扩展为自由文本） |
| 评价状态 | normal / hidden |
| 工单类型 | user_complaint / device_fault / refund / maintenance / abnormal_order |
| 工单优先级 | low / medium / high |
| 工单状态 | pending / processing / completed / closed |
| 券类型 | new_user / full_reduction / night / site |
| 券模板状态 | active / inactive |
| 券状态 | unused / used / expired |
| 站点归属类型 | self_run / franchise / partner / third_party |
| 商户状态 | active / paused |
| 商户合作类型 | franchise / partner / third_party |
| 站点设施 | washroom / convenience_store / rest_area / wifi / rain_shelter / underground_parking |
| 天气状况 | sunny / cloudy / rain / hot / extreme |
| 钱包流水类型 | recharge / consume / refund / other / penalty / occupy_fee |
| 通知类型 | reservation / order / point / coupon / work_order / system |
| 时间轴节点 | reserved / arrived / started / soc_50 / target_reached / finished / settled |
| 积分原因 | charge / redeem |
| 预约类型 | immediate_queue / timed |
| 订单/预约取消原因 | user_cancel / no_show / timeout / admin |
| 规则对象类型 | station / charger / user |
| 规则动作 | redirect / issue_coupon / create_alarm / create_work_order / limit_reservation |
| 套餐模板状态 | active / inactive |
| 订阅状态 | active / expired / cancelled |
| 帖子/回复状态 | normal / hidden |
| 换电电池状态 | idle / swapping / charging / fault / offline |

## 聚合口径说明（不入库，按需实时计算）

以下指标由源表聚合得出，不建冗余表，各端如需缓存自行处理：

| 指标 | 口径 | 消费方 |
| ---- | ---- | ---- |
| 用户充电画像 | `charging_order` + `station` + `price_rule` 聚合（月均次数/平均电量/常去站/常用时段/偏好类型） | 用户端画像、猜你喜欢 |
| 管理端用户画像 | 该用户历史订单聚合（月均次数/平均金额/常用站/生命周期/价值等级） | 管理端 |
| 等待时间/三档预测 | `reservation` 队列长度 × 历史同期平均剩余时长 + 负荷预测外推 | 用户端、管理端 |
| 负荷/故障/需求预测 | `charging_measure`、`charger.health_score`、订单、`weather`、`holiday` | 大屏、用户端、管理端 |
| 绿色减碳 | 充电量 × 碳排因子（初值 0.7 kg/kWh） | 用户端报告、管理端、大屏 |
| 用户增长 | `user.register_time` 按日聚合 | 大屏、驾驶舱 |
| 评价标签分析 | `review.tags` 频次与环比 | 大屏、管理端 |

## 数据读写边界

| 数据 | 负责写入的一端 | 读用方 |
| ---- | -------------- | ------ |
| 用户、余额、积分 | 用户端（登录/注册/充值/结算） | 管理端、服务端 |
| 钱包流水 | 用户端（充值/结算/退款动作统一落流水） | 管理端、财务分析 |
| 订单、预约、时间轴 | 用户端（下单/充电/结算）、服务端定时任务 | 管理端、大屏 |
| 电桩实时状态、测量数据 | 服务端设备模拟/采集线程 | 用户端、大屏、机器学习 |
| 电桩/电站/电价/管理员 | 管理端（新增/维护） | 用户端、大屏 |
| 站点费用/设施/归属 | 管理端（电站管理扩展表单） | 用户端、推荐引擎 |
| 告警、设备日志 | 服务端故障监测、管理端运维 | 大屏、AI 助手 |
| 车辆 | 用户端（添加/修改） | 服务端（兼容过滤）、管理端 |
| 评价 | 用户端（提交/点有用） | 大屏、管理端、机器学习 |
| 工单 | 用户端（提交）、管理端（处理）、服务端（设备类自动建） | 用户端、管理端、大屏 |
| 券模板 | 管理端（营销中心配置） | 用户端、服务端 |
| 用户持有券 | 用户端（领券/结算核销） | 管理端（营销统计） |
| 天气 | 服务端（定时模拟/手动设置） | 用户端、大屏、机器学习 |
| 节假日 | 服务端（建库种子/按年维护） | 机器学习 |
| 通知 | 服务端（push 落库）、用户端（已读标记） | 用户端消息中心 |
| 商户、规则、套餐、帖子、收藏、FAQ | 管理端（扩展配置页，帖子/收藏由用户端写） | 各端按功能读取 |
| 换电电池、换电订单 | 用户端（换电结算）、服务端（电池状态采集） | 用户端、管理端、大屏 |

## 种子数据约定

建库脚本随服务端一起执行，保证演示环境一致。至少包含：

- 管理员：`admin / 123456`。
- 充电站：4–5 座，覆盖不同区域（含 1 座有雨棚/地下停车场、1 座合作商户、1 座支持换电），每站含地址、区域与经纬度、服务费、停车费、营业时间、设施。
- 充电桩：每站 6–20 台，快慢充混合，初始状态按需分布（含 2 台故障桩用于演示）。
- 换电电池：为支持换电的电站配 10–20 块不同电量的 `battery`（演示换电闭环，随扩展表建库一起）。
- 分时电价：至少一套谷/平/峰三档规则，含 1–2 个专属站规则。
- 用户：3–5 个测试手机号（如 `13800000001`），含一定余额、积分、历史订单（供画像、营收、负荷预测演示）。
- 车辆：为测试用户各配 1–3 辆车（含默认车辆）。
- 优惠券模板：新人立减券（active）、满减券、夜间券各一。
- 评价/工单/告警：少量历史评价与 1–2 张处理中的工单，供分析页演示。
- 天气：一条全局当前天气（晴）与逐区域行。
- 节假日：当年主要法定节假日与调休日。
- 商户/套餐/FAQ：1 家合作商户、1 个套餐模板、若干 FAQ 条目（可选，随扩展表建库一起）。
- 历史 `charging_measure`：服务端模拟线程按 `负荷 = 基线 + 早晚双峰正弦 + 随机噪声` 生成，让预测曲线有峰谷规律。

## 待定项清单

| 项 | 位置 | 说明 |
| --- | ---- | ---- |
| 种子数据具体规模与示例桩号 | 本文件「种子数据约定」 | 由 DB 负责人实现时确定，建议初值：4 站、每站 10 桩、含 2 台故障桩 |
| `online_rate` 计算口径 | station 表 | 建议初值：在线桩数 / 总桩数 × 100% |
| 扩展表建库范围 | 扩展表 | 建议初值：全部随服务端建库（含换电域 battery / swap_order） |
| 商户结算口径与费率 | merchant 表 | `settle_amount` 建议初值：合作站订单实付金额 × 分成比例，比例待定 |