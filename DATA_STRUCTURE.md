# DATA_STRUCTURE（数据结构总览）

版本 v1 · 与 `spec-数据库.md`（冻结真相源）一对一映射。本文件是**干净的全量参考**：完整列出 v1 全部数据实体、字段、依赖与枚举；字段级定义、读写边界、种子数据以 `spec-数据库.md` 为准。

## 设计原则

1. **一份数据，多端视角**：同一台桩、同一笔订单、同一条告警在用户端/管理端/大屏以不同视图出现，全部落库由服务端统一读写，各端经 WebSocket 访问。
2. **状态与记录入库，指标不建表**：天气、时间轴、钱包流水等"跨端展示且验收要一致"的数据必须落库；画像、预测、等待时间、风险分等纯计算指标由源表聚合，不建冗余表（见「聚合口径」）。
3. **层叠式分层**：核心表（闭环必需）→ 支撑表（规格【增强】必需）→ 扩展表（增强功能池，含换电域）。
4. **命名约定**：表名与字段名统一小写蛇形；主键统一 `id` 自增；外键以 `xxx_id` 命名并指向目标表主键；时间统一 `YYYY-MM-DD HH:MM:SS`；JSON 存 TEXT；布尔存 INTEGER 0/1。

## 实体总览

共 **31 张表**：

```
核心表（16）            支撑表（5）             扩展表（10）
user                   order_timeline          merchant
admin                  wallet_transaction      rule_config
station                notification            member_plan
charger                weather                 user_plan
charging_order         holiday                 station_post
reservation                                     post_reply
price_rule                                      favorite
vehicle                                         faq
review                                          battery
work_order                                      swap_order
device_log
alarm
point_record
charging_measure
coupon
user_coupon
```

### 外键依赖关系（跨表外键一览）

| 外键字段 | 源表 | 目标表 | 说明 |
| ---- | ---- | ---- | ---- |
| station_id | charger / charging_order / reservation / price_rule / review / work_order / charging_measure / station_post / favorite / coupon | station | 电站的从属与关联 |
| charger_id | charging_order / reservation / work_order / device_log / alarm / charging_measure | charger | 桩的从属与关联 |
| user_id | 全部用户侧业务表 | user | 用户主体（订单/预约/积分/流水/车辆/评价/工单/券/订阅/通知/收藏/帖子） |
| vehicle_id | charging_order | vehicle | 订单关联车辆（可空） |
| coupon_id | user_coupon | coupon | 持券实例化 |
| coupon_id / used_order_id | charging_order / user_coupon | user_coupon / charging_order | 结算核销闭环 |
| plan_id | user_plan | member_plan | 套餐订阅 |
| merchant_id | station | merchant | 合作商户（自营为空） |
| order_id | order_timeline / review / wallet_transaction | charging_order | 订单溯源 |
| battery_in_id / battery_out_id | swap_order | battery | 换电闭环（归还 / 换入） |

## 表字段明细

> 标记：PK=主键自增；FK(name)=外键→目标表。

### 1. user（用户）—核心

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER · PK | 用户 ID |
| phone | TEXT | 手机号，11 位，唯一 |
| nickname | TEXT | 昵称，默认"用户+后四位" |
| avatar_path | TEXT | 头像路径，空=灰色占位 |
| balance | REAL | 余额（元） |
| points | INTEGER | 累计可用积分 |
| level | TEXT | normal / vip / enterprise |
| status | TEXT | normal / frozen（冻结拒登录/充电） |
| register_time | TEXT | 注册时间 |
| last_login_time | TEXT | 最近登录时间（可空） |

### 2. admin（管理员）—核心

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER · PK | 管理员 ID |
| account | TEXT | 账号，唯一，种子 `admin` |
| password | TEXT | 密码，种子 `123456` |

### 3. station（充电站）—核心

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER · PK | 充电站 ID |
| name | TEXT | 站名 |
| address | TEXT | 详细地址 |
| area | TEXT | 所属区域（区县/商圈），区域聚合与筛选 |
| longitude / latitude | REAL | 经度 / 纬度（腾讯地图地理编码） |
| total_chargers | INTEGER | 总电桩数 |
| online_rate | REAL | 在线率（在线桩/总桩×100%，动态） |
| service_fee | REAL | 服务费（元/kWh） |
| parking_fee | REAL | 停车费（元/小时，0=免费） |
| business_hours | TEXT | 营业时间，如 "00:00–24:00" |
| facilities | TEXT | 设施 JSON 数组：washroom / convenience_store / rest_area / wifi / rain_shelter / underground_parking |
| owner_type | TEXT | 归属：self_run / franchise / partner / third_party |
| merchant_id | INTEGER · FK(merchant) | 合作商户，自营为空 |
| has_swap | INTEGER | 支持换电 0/1（预留标记，默认 0） |

### 4. charger（充电桩）—核心

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER · PK | 电桩 ID |
| code | TEXT | 站内编号，如 A-023 |
| station_id | INTEGER · FK(station) | 所属电站 |
| type | TEXT | fast / slow |
| power | REAL | 额定功率（kW） |
| status | TEXT | idle / charging / reserved / fault / offline / rebooting |
| voltage / current | REAL | 当前电压（V）/ 电流（A），数字孪生实时参数 |
| temperature | REAL | 当前温度（℃） |
| fault_code | TEXT | 故障码，空=正常 |
| comm_status | TEXT | normal / abnormal |
| health_score | INTEGER | 健康度 0–100（规则见 spec-机器学习） |
| total_charge_count | INTEGER | 累计充电次数 |
| total_charge_duration | INTEGER | 累计充电时长（分钟） |
| created_time | TEXT | 建桩时间 |

### 5. charging_order（充电订单）—核心

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER · PK | 订单 ID |
| user_id | INTEGER · FK(user) | 下单用户 |
| station_id | INTEGER · FK(station) | 目标电站 |
| charger_id | INTEGER · FK(charger) | 电桩 |
| vehicle_id | INTEGER · FK(vehicle) | 关联车辆（可空） |
| status | TEXT | reserved / charging / pending_settle / completed / cancelled |
| start_soc / target_soc / end_soc | REAL | 起始 / 目标 / 结束电量（%） |
| start_time / end_time | TEXT | 开始 / 结束充电时间 |
| duration_min | INTEGER | 充电时长（分钟） |
| energy_kwh | REAL | 充电量（kWh） |
| price_level | TEXT | 结算档位：valley / flat / peak |
| amount | REAL | 应收（电价×电量） |
| discount_amount | REAL | 抵扣合计（券+积分） |
| pay_amount | REAL | 实付 = amount − discount_amount（余额扣减额） |
| points_used | INTEGER | 积分抵扣数 |
| coupon_id | INTEGER · FK(user_coupon) | 核销券（可空） |
| points_earned | INTEGER | 本单获得积分 |
| create_time / settle_time | TEXT | 下单 / 结算时间 |

### 6. reservation（预约/排队）—核心

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER · PK | 预约 ID |
| user_id | INTEGER · FK(user) | 预约用户 |
| station_id | INTEGER · FK(station) | 目标电站 |
| charger_id | INTEGER · FK(charger) | 轮到时匹配的桩（可空） |
| queue_no | INTEGER | 排队序号 |
| reserve_time | TEXT | 进入排队时间 |
| estimate_start_time | TEXT | 预计开始时间 |
| notified | INTEGER | 已推送轮队通知 0/1 |
| status | TEXT | waiting / matched / cancelled |

### 7. price_rule（分时电价）—核心

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER · PK | 规则 ID |
| station_id | INTEGER · FK(station) | 适用电站（空=全站通用） |
| level | TEXT | valley / flat / peak |
| price | REAL | 单价（元/kWh） |
| time_range | TEXT | 时段，如 "00:00–08:00" |

### 8. vehicle（我的车辆）—核心

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER · PK | 车辆 ID |
| user_id | INTEGER · FK(user) | 车主 |
| name | TEXT | 车辆名称 |
| type | TEXT | car / light_truck / two_wheeler / three_wheeler |
| battery_kwh | REAL | 电池容量（kWh） |
| connector_type | TEXT | ac_gb / dc_gb / other |
| max_power_kw | REAL | 最大支持功率（kW） |
| is_default | INTEGER | 默认车辆 0/1，每用户至多一辆 |
| created_time | TEXT | 添加时间 |

### 9. review（站点评价）—核心

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER · PK | 评价 ID |
| user_id | INTEGER · FK(user) | 评价用户 |
| station_id | INTEGER · FK(station) | 被评电站 |
| order_id | INTEGER · FK(charging_order) | 关联订单（可空） |
| overall_score / speed_score / device_score / parking_score / hygiene_score / service_score | REAL | 综合 + 5 维评分（0–5） |
| tags | TEXT | 标签 JSON 数组（枚举 + 自由文本） |
| content | TEXT | 文字评价 |
| useful_count | INTEGER | 被点"有用"次数 |
| status | TEXT | normal / hidden |
| create_time | TEXT | 评价时间 |

### 10. work_order（工单）—核心

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER · PK | 工单 ID |
| type | TEXT | user_complaint / device_fault / refund / maintenance / abnormal_order |
| priority | TEXT | low / medium / high |
| user_id | INTEGER · FK(user) | 提交用户（设备类可空） |
| station_id | INTEGER · FK(station) | 关联电站（可空） |
| charger_id | INTEGER · FK(charger) | 关联电桩（可空） |
| title / description | TEXT | 标题 / 描述 |
| status | TEXT | pending / processing / completed / closed |
| handler | TEXT | 处理管理员账号 |
| result | TEXT | 处理结果/备注 |
| create_time / handle_time | TEXT | 创建 / 处理完成时间 |

### 11. device_log（设备操作日志）—核心

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER · PK | 日志 ID |
| charger_id | INTEGER · FK(charger) | 目标电桩 |
| action | TEXT | restart / pause / repair |
| operator | TEXT | 操作管理员账号 |
| op_time | TEXT | 操作时间 |
| result | TEXT | success / failed |

### 12. alarm（告警）—核心

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER · PK | 告警 ID |
| charger_id | INTEGER · FK(charger) | 电桩（空=站点级） |
| station_id | INTEGER · FK(station) | 电站 |
| type | TEXT | comm_abnormal / overheat / power_drop / offline / user_behavior |
| level | TEXT | info / warning / critical |
| occur_time | TEXT | 发生时间 |
| status | TEXT | open / handled |
| handle_action | TEXT | restart / pause / repair / ignore |

### 13. point_record（积分记录）—核心

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER · PK | 记录 ID |
| user_id | INTEGER · FK(user) | 用户 |
| change | INTEGER | 变动（正获得 / 负抵扣） |
| reason | TEXT | charge / redeem |
| create_time | TEXT | 变动时间 |

### 14. charging_measure（充电时序测量）—核心

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER · PK | 测量 ID |
| charger_id | INTEGER · FK(charger) | 电桩 |
| station_id | INTEGER · FK(station) | 电站 |
| measure_time | TEXT | 采样时间 |
| power_kw | REAL | 瞬时功率 |
| soc | REAL | 当前 SOC（%） |
| energy_delta_kwh | REAL | 本采样间隔充电量 |
| temperature | REAL | 温度（可空，健康度趋势） |

### 15. coupon（优惠券模板）—核心

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER · PK | 模板 ID |
| title | TEXT | 券名 |
| type | TEXT | new_user / full_reduction / night / site |
| discount_amount | REAL | 抵扣金额（元） |
| min_amount | REAL | 门槛（0=无门槛） |
| station_id | INTEGER · FK(station) | 适用电站（空=通用） |
| time_range | TEXT | 适用时段（空=不限，夜间/时段券） |
| valid_days | INTEGER | 领取后有效天数 |
| total | INTEGER | 发放总数（-1 不限） |
| status | TEXT | active / inactive |
| create_time | TEXT | 创建时间 |

### 16. user_coupon（用户持有券）—核心

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER · PK | 持券 ID |
| user_id | INTEGER · FK(user) | 持券用户 |
| coupon_id | INTEGER · FK(coupon) | 券模板 |
| status | TEXT | unused / used / expired |
| receive_time | TEXT | 领取时间 |
| used_order_id | INTEGER · FK(charging_order) | 核销订单（可空） |

### 17. order_timeline（订单时间轴）—支撑

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER · PK | 节点 ID |
| order_id | INTEGER · FK(charging_order) | 关联订单 |
| node | TEXT | reserved / arrived / started / soc_50 / target_reached / finished / settled |
| label | TEXT | 展示文案（如"到达充电站"） |
| event_time | TEXT | 节点时间戳 |
| detail | TEXT | 补充（可空，如"目标电量 95%"） |

### 18. wallet_transaction（钱包流水）—支撑

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER · PK | 流水 ID |
| user_id | INTEGER · FK(user) | 用户 |
| type | TEXT | recharge / consume / refund / other |
| amount | REAL | 变动（正入负出，元） |
| balance_after | REAL | 变动后余额 |
| order_id | INTEGER · FK(charging_order) | 关联订单（可空） |
| remark | TEXT | 备注 |
| create_time | TEXT | 交易时间 |

### 19. notification（站内消息）—支撑

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER · PK | 消息 ID |
| user_id | INTEGER · FK(user) | 接收用户 |
| type | TEXT | reservation / order / point / coupon / work_order / system |
| title / content | TEXT | 标题 / 内容 |
| related_id | INTEGER | 关联业务 ID（可空） |
| is_read | INTEGER | 已读 0/1 |
| create_time | TEXT | 时间 |

### 20. weather（模拟天气）—支撑

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER · PK | 天气 ID |
| area | TEXT | 区域（对齐 station.area，空=全局） |
| condition | TEXT | sunny / cloudy / rain / hot / extreme |
| temperature | REAL | 温度（℃） |
| forecast | TEXT | 未来 N 小时预测 JSON（可空） |
| update_time | TEXT | 刷新时间 |

### 21. holiday（节假日）—支撑

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER · PK | 记录 ID |
| date | TEXT | 日期（YYYY-MM-DD，唯一） |
| name | TEXT | 名称 |
| is_workday | INTEGER | 0=休息日 / 1=调休上班 |
| create_time | TEXT | 记录时间 |

### 22. merchant（商户/合作站点）—扩展

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER · PK | 商户 ID |
| name | TEXT | 商户名称 |
| contact_name / contact_phone | TEXT | 联系人 / 电话 |
| cooperation_type | TEXT | franchise / partner / third_party |
| status | TEXT | active / paused |
| order_count | INTEGER | 合作站累计订单数（汇总） |
| settle_amount | REAL | 累计结算金额（汇总） |
| service_score | REAL | 服务评分均值 0–5 |
| remark | TEXT | 备注 |
| create_time | TEXT | 加入时间 |

### 23. rule_config（规则引擎配置）—扩展

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER · PK | 规则 ID |
| name | TEXT | 规则名 |
| object_type | TEXT | station / charger / user |
| condition | TEXT | 触发条件 JSON（如 {"field":"idle_rate","op":"<","value":20}） |
| action | TEXT | redirect / issue_coupon / create_alarm / create_work_order / limit_reservation |
| action_params | TEXT | 动作参数 JSON（可空） |
| enabled | INTEGER | 启用 0/1 |
| priority | INTEGER | 匹配优先级高→低 |
| description | TEXT | 说明 |
| create_time / update_time | TEXT | 创建 / 更新时间 |

### 24. member_plan（套餐模板）—扩展

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER · PK | 套餐 ID |
| name | TEXT | 套餐名 |
| price | REAL | 售价（元/期） |
| valid_days | INTEGER | 生效天数 |
| service_fee_discount | REAL | 服务费折扣（0.8=8折，1=不折） |
| night_discount | REAL | 夜间折扣（可空） |
| points_multiplier | REAL | 积分倍率（可空） |
| status | TEXT | active / inactive |
| description | TEXT | 说明 |
| create_time | TEXT | 创建时间 |

### 25. user_plan（用户套餐订阅）—扩展

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER · PK | 订阅 ID |
| user_id | INTEGER · FK(user) | 用户 |
| plan_id | INTEGER · FK(member_plan) | 套餐 |
| start_time / end_time | TEXT | 生效起止 |
| status | TEXT | active / expired / cancelled |
| create_time | TEXT | 订阅时间 |

### 26. station_post（站点社区帖）—扩展

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER · PK | 帖子 ID |
| station_id | INTEGER · FK(station) | 电站 |
| user_id | INTEGER · FK(user) | 发帖用户 |
| content | TEXT | 内容 |
| like_count / reply_count | INTEGER | 点赞 / 回复数 |
| status | TEXT | normal / hidden |
| create_time | TEXT | 发帖时间 |

### 27. post_reply（帖子回复）—扩展

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER · PK | 回复 ID |
| post_id | INTEGER · FK(station_post) | 帖子 |
| user_id | INTEGER · FK(user) | 回复用户 |
| parent_id | INTEGER | 0=直接回复帖，否则指向某回复（两级） |
| content | TEXT | 内容 |
| status | TEXT | normal / hidden |
| create_time | TEXT | 回复时间 |

### 28. favorite（站点收藏）—扩展

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER · PK | 收藏 ID |
| user_id | INTEGER · FK(user) | 用户 |
| station_id | INTEGER · FK(station) | 电站 |
| create_time | TEXT | 收藏时间 |

### 29. faq（客服机器人问答库）—扩展

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER · PK | 条目 ID |
| category | TEXT | 分类（对齐工单/问题分类） |
| question | TEXT | 问题模板/关键词 |
| answer | TEXT | 应答 |
| sort | INTEGER | 排序 |
| enabled | INTEGER | 启用 0/1 |
| create_time | TEXT | 创建时间 |

### 30. battery（换电电池包）—扩展

> 挂靠在 `station.has_swap=1` 的换电站下，随扩展表建库。

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER · PK | 电池 ID |
| code | TEXT | 电池编号（站内唯一） |
| station_id | INTEGER · FK(station) | 所属换电站 |
| soc | REAL | 电量（%） |
| health_score | INTEGER | 健康度 0–100 |
| temperature | REAL | 温度（℃） |
| status | TEXT | idle / swapping / charging / fault / offline |
| swap_count | INTEGER | 累计换电次数 |
| create_time | TEXT | 入库时间 |

### 31. swap_order（换电订单）—扩展

> 换电结算同时写 `wallet_transaction`（type=consume，remark="换电"）。

| 字段 | 类型 | 说明 |
| ---- | ---- | ---- |
| id | INTEGER · PK | 订单 ID |
| user_id | INTEGER · FK(user) | 用户 |
| station_id | INTEGER · FK(station) | 换电站 |
| battery_in_id / battery_out_id | INTEGER · FK(battery) | 归还 / 换入电池 |
| amount | REAL | 费用（元） |
| status | TEXT | completed / cancelled |
| create_time | TEXT | 时间 |

## 枚举字典

| 枚举 | 取值 | 使用字段 |
| ---- | ---- | ---- |
| 用户状态 | normal / frozen | user.status |
| 会员等级 | normal / vip / enterprise | user.level |
| 订单状态 | reserved / charging / pending_settle / completed / cancelled | charging_order.status |
| 电桩状态 | idle / charging / reserved / fault / offline / rebooting | charger.status |
| 电桩类型 | fast / slow | charger.type |
| 电价档位 | valley / flat / peak | price_rule.level、charging_order.price_level |
| 告警级别 | info / warning / critical | alarm.level |
| 告警类型 | comm_abnormal / overheat / power_drop / offline / user_behavior | alarm.type |
| 设备操作 | restart / pause / repair | device_log.action、alarm.handle_action |
| 车型类型 | car / light_truck / two_wheeler / three_wheeler | vehicle.type |
| 接口类型 | ac_gb / dc_gb / other | vehicle.connector_type |
| 评价标签 | fast_charge / spacious / quiet_night / high_parking_fee / old_device / good_fast_charge / heavy_queue / good_service（+自由文本） | review.tags |
| 评价状态 | normal / hidden | review.status |
| 工单类型 | user_complaint / device_fault / refund / maintenance / abnormal_order | work_order.type |
| 工单优先级 | low / medium / high | work_order.priority |
| 工单状态 | pending / processing / completed / closed | work_order.status |
| 券类型 | new_user / full_reduction / night / site | coupon.type |
| 券模板状态 | active / inactive | coupon.status、member_plan.status |
| 券状态 | unused / used / expired | user_coupon.status |
| 站点归属 | self_run / franchise / partner / third_party | station.owner_type |
| 商户状态 | active / paused | merchant.status |
| 商户合作类型 | franchise / partner / third_party | merchant.cooperation_type |
| 站点设施 | washroom / convenience_store / rest_area / wifi / rain_shelter / underground_parking | station.facilities |
| 天气状况 | sunny / cloudy / rain / hot / extreme | weather.condition |
| 钱包流水类型 | recharge / consume / refund / other | wallet_transaction.type |
| 通知类型 | reservation / order / point / coupon / work_order / system | notification.type |
| 时间轴节点 | reserved / arrived / started / soc_50 / target_reached / finished / settled | order_timeline.node |
| 积分原因 | charge / redeem | point_record.reason |
| 规则对象 | station / charger / user | rule_config.object_type |
| 规则动作 | redirect / issue_coupon / create_alarm / create_work_order / limit_reservation | rule_config.action |
| 订阅状态 | active / expired / cancelled | user_plan.status |
| 帖子/回复状态 | normal / hidden | station_post.status、post_reply.status |
| 换电电池状态 | idle / swapping / charging / fault / offline | battery.status |

## 依赖与一致性要点

- **订单闭环**：charging_order 是用户侧核心，挂 user / station / charger / vehicle / coupon，配套 order_timeline（过程节点）、wallet_transaction（金额流水）、point_record（积分）、review（评价）。结算时同时更新 user.balance、user.points、user_coupon.status。
- **设备闭环**：charger ↔ charging_measure（时序）↔ device_log（运维）↔ alarm（告警）↔ work_order（检修），故障联动以 `charger.status` 为唯一状态口径。
- **价格与计费**：动态电价取 `price_rule`（按当前时间命中 time_range）；应收 = price_level × energy_kwh，实付 = 应收 −（券面额 + 积分抵扣）。
- **排队闭环**：reservation.queue_no 站内自增，轮到时回填 charger_id、置 notified=1，再写 notification 并 push。
- **营销闭环**：coupon（模板）→ user_coupon（实例）→ charging_order.coupon_id（核销）→ 管理端统计。
- **跨端一致**：weather、alarm、charger 实时状态由服务端单一写入并广播，各端只读呈现。

## 聚合口径（不入库，实时计算）

| 指标 | 口径 | 消费方 |
| ---- | ---- | ---- |
| 用户充电画像 | charging_order + station + price_rule 聚合 | 用户端「我的」底部、猜你喜欢 |
| 管理端用户画像 | 该用户历史订单聚合（次数/金额/常用站/生命周期/价值等级） | 管理端用户画像 |
| 智能推荐 / 一键找桩 | 距离、空闲数、排队、价格、负荷预测、健康度加权 | 用户端推荐 |
| 预计等待（三档） | 排队人数 × 历史同期平均剩余时长 + 预测外推 | 用户端列表/详情/排队 |
| 负荷/故障/需求预测 | charging_measure、health_score、订单、weather、holiday | 大屏、用户端避峰、管理端 |
| 绿色减碳 | 充电量 × 碳排因子（初值 0.7 kg/kWh） | 报告、管理端、大屏 |
| 用户增长 | user.register_time 按日聚合 | 大屏、驾驶舱 |
| 评价标签分析 | review.tags 频次与环比、反馈 TOP5 | 大屏、管理端 |

## 种子数据建议

- admin：`admin / 123456`。
- 电站 4–5 座（覆盖不同 area，含雨棚/地下库、合作商户站、换电预留标记各 1），每站含费用、营业时间、设施。
- 每站 6–20 桩，快慢充混合，含 2 台故障桩；分时电价 ≥1 套谷/平/峰 + 1–2 个专属站规则。
- 测试用户 3–5 个（`13800000001`…），含余额、积分与历史订单；每人 1–3 辆车（含默认车）。
- 券模板：新人立减（active）、满减、夜间各 1；历史评价若干、处理中工单 1–2 张。
- 天气：全局晴 + 逐区域行；节假日：当年主要节日与调休。
- 可选：1 家合作商户、1 个套餐模板、若干 FAQ。
- 换电电池：为支持换电的电站配 10–20 块不同电量的 `battery`。
- charging_measure：模拟线程按 `基线 + 早晚双峰正弦 + 随机噪声` 生成。

## 待定与可选说明

| 项 | 建议初值 | 说明 |
| --- | ---- | ---- |
| 种子规模与桩号 | 4 站、每站 10 桩、2 台故障 | DB 负责人实现时定稿 |
| online_rate 口径 | 在线桩数 / 总桩数 × 100% | station.online_rate |
| 扩展表建库 | 全部随库建（含换电域 battery / swap_order） | 可按裁剪停用 |
| 商户分成费率 | 待定 | merchant.settle_amount |