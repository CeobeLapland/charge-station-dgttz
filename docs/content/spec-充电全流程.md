# spec-充电全流程（预约 → 二维码 → 充电 → 结算）

> 由用户端提出。本文定义「充电全流程」的统一状态机、用户行为与违约规则、字段/协议增补、以及示例阶段（未接后端）的实现方案。属"解冻"改动的依据：落地前先同步 `spec-数据库.md` / `spec-协议.md` / `DATA_STRUCTURE.md`，再动代码（见 `AGENTS.md`）。
>
> 阈值均为**建议初值**，可调；正式值以服务端实现为准。

## 1. 目标与范围

- 把电站详情页的「立即充电」统一改造为「立即预约」，作为发起充电的唯一入口。
- 覆盖：预约（立即 / 定时）→ 验证未完成订单 → 匹配（有空位锁桩 / 无空位排队）→ 轮候通知 → 扫码验证 → 充电 → 完成提示 → 结算。
- 明确并处理「违约 / 失信」行为：预约不到店、超时未扫码、充满不挪车、充一半拔枪、排队轮到不响应、主动取消等，通过**违约金**与**信用分**形成闭环。

## 2. 统一两层模型

关键区分「尚未锁定实体桩的等待」与「已锁定桩的订单」，避免"占着坑位"问题：

| 层 | 表 | 状态 | 说明 |
| --- | --- | --- | --- |
| 等待层 | `reservation` | waiting / matched / cancelled | 不锁实体桩，负责排队与定时预约的匹配窗口 |
| 订单层 | `charging_order` | reserved / charging / pending_settle / completed / cancelled | 匹配到桩之后才创建，之后为充电主状态机 |

对应关系：

| 用户动作 | 变化 |
| --- | --- |
| 立即预约·有空位 | 直接 `reservation.matched` → 生成 `charging_order(reserved)` + `charger→reserved` |
| 立即预约·无空位 | `reservation(waiting, reserve_type=immediate_queue)` 排队 |
| 定时预约（未来） | `reservation(waiting, reserve_type=timed, expect_time=期望时刻)`，**不锁桩** |
| 定时预约到点前 N 分钟 | 尝试匹配空闲兼容桩 → 命中则 `matched`（**到点才锁桩**），未命中转排队 `waiting` |
| 排队轮到 | `push.reservation_notify` → `matched` 回填 `charger_id` → 生成订单 |
| 扫码校验通过 | `order.start` → `charging`（`reservation` 留 `matched` 记录供时间轴追溯） |

> 决策已定：**定时 / 排队都「到点/轮到才匹配、才分配桩号」**；扫码页的目标桩码只在 `matched` 之后才有。

## 3. 页面流转（状态机）

```
电站详情 → 【立即预约】
   ↓  预约表单：预约时间(立即/未来)、车辆、目标电量、快/慢偏好
   ↓
① 校验：
   - 存在未完成订单(reserved/charging/pending_settle 未过期) → 拦截(2001)，引导去结算/处理
   - 用户 frozen → 拦截(1003)
   ↓
② 匹配：
   有空闲兼容桩 → reservation.matched + charging_order(reserved) + 锁桩
   无空闲桩     → reservation.waiting（排队或定时等待）
   ↓
③ 到点/轮到通知：
   立即预约 → 直接进「扫码验证」提示页（"请前往 X 号桩扫码启动"）
   定时预约 → 到点前 push 提醒（错峰/谷电场景）
   排队轮到 → push.reservation_notify → 提示 → 扫码
   ↓
④ 扫码(形式) → 校验桩码与分配桩一致 → order.start → charging
   ↓
⑤ 充电进度页（实时 SOC/功率/费用/ETA）
   ↓
⑥ 充电完成提示（达目标电量 或 用户提前结束）
   ↓
⑦ order.finish → pending_settle → 结算页（选券/积分 → 扣余额）→ completed → 报告/评价
```

缺省但须显式定义的中间态：

1. **定时预约的"等待到点"态**：`reservation(waiting, reserve_type=timed, expect_time)`，与"立即预约"区分。
2. **"已匹配/已扫码、未按开始"态**：`charging_order(reserved)` 在 `scan_deadline` 前的保留窗口。
3. **"充满后无结算/未挪车"的占位态**：达 `target_soc` 起独立计时，产生占位费（见下）。
4. **扫码前桩码一致性校验**：扫的码与 `matched` 分配的桩不一致时拦截。

## 4. 行为与违约规则（阈值初值）

| 触发 | 规则 | 结果 |
| --- | --- | --- |
| 扫码超时（`scan_deadline` 前未启动） | 保留窗口 10 分钟 | `reserved→cancelled(no_show)` + 释放桩 + 违约金 ¥5 + 信用 −5 |
| 排队轮到超时未响应 | 响应窗口 5 分钟 | 顺延下一位 + 信用 −5（首次免罚） |
| 主动取消 | 提前 ≥15 分钟免费；临近取消 | 释放/出队，临近取消提示信用 −2 |
| 充满不拔枪 / 不挪车 | 达 `target_soc` 起计时，10 分钟宽限 | 按 `station.parking_fee` 累计 `occupy_fee` 计入结算 + push 提醒 |
| 充满后长时不结算 | 余额足→自动扣款 `completed`；不足→ `pending_settle` 阻断新单 | — |
| 充一半拔枪（未点结束） | 功率骤降/归零检测 | push「充电中断」+ `alarm(user_behavior)` → 按实际电量转 `pending_settle` |
| 功率异常下降 | 检测 | push「检测到功率异常，监控中」 |
| 设备离线/故障 | `charger→offline/fault` | push；订单挂起或转结算/退款 |
| 信用分低于阈值（如 <60） | — | 限制预约（只允许排队），过低冻结 |

## 5. 字段增补（对称 `spec-数据库.md` / `DATA_STRUCTURE.md`）

| 表 | 新增字段 | 类型 | 说明 |
| --- | --- | --- | --- |
| `user` | `credit_score` | INTEGER | 信用分，默认 100，违约负向扣减；低于阈值限制预约 |
| `charging_order` | `reserved_time` | TEXT | 预约生效/期望开始时间（立即预约为匹配时刻，定时预约为期望开始时刻） |
| | `scan_deadline` | TEXT | 扫码启动截止时间（匹配后 + 保留窗口，超时未扫码转 cancelled） |
| | `cancel_reason` | TEXT | 取消原因：user_cancel / no_show / timeout / admin（可空） |
| | `occupy_fee` | REAL | 占位费（元，充满后未挪车按停车费累计，默认 0） |
| | `occupy_min` | INTEGER | 占位时长（分钟，默认 0） |
| | `penalty_fee` | REAL | 违约金（元，违约场景扣款，默认 0） |
| `reservation` | `reserve_type` | TEXT | 预约类型：immediate_queue（即时排队）/ timed（定时预约） |
| | `expect_time` | TEXT | 期望开始时间（定时预约时刻 / 排队预测开始） |
| | `matched_time` | TEXT | 匹配到空闲桩的时间（可空，matched 后回填） |
| | `expire_time` | TEXT | 响应截止时间（matched 后 + 轮候窗口，超时顺延） |
| | `cancel_reason` | TEXT | 取消原因：user_cancel / timeout / no_show（可空） |

枚举扩展：

| 枚举 | 新增取值 |
| --- | --- |
| 钱包流水类型（`wallet_transaction.type`） | `penalty`（违约金）、`occupy_fee`（占位费） |
| 预约类型（`reservation.reserve_type`） | `immediate_queue` / `timed` |
| 订单/预约取消原因（`cancel_reason`） | `user_cancel` / `no_show` / `timeout` / `admin` |

## 6. 协议消息增补（待同步 `spec-协议.md`）

| type | 方向 | 说明 |
| --- | --- | --- |
| `push.reservation_timeout` | S→用户端 | 排队轮到超时顺延通知 |
| `push.order_timeout` | S→用户端 | 预约扫码超时取消通知 |
| `order.create` | C→S | payload 增加 `reserved_time`（可空，定时预约时刻） |

示例阶段以上由前端 `Timer` 驱动；接后端时再落地到协议。

## 7. 示例数据阶段实现（ChargingFlow mock 单例）

- 新增前端内存态单例 **`ChargingFlow`**（同 `UserData`/`ExploreData` 注册为 QML singleton），集中管理整条状态机与各超时 `Timer`：创建预约/排队/匹配/扫码/进度/结束/结算、违约金与信用分累加。
- 页面只读写 `ChargingFlow`，不各自硬编码；将来接 WebSocket 时仅把 `ChargingFlow` 的动作替换为 `order.create / reservation.join / push.*` 等，QML 层基本不动。
- Timer 阈值（建议初值）：扫码保留 10 分钟、排队响应 5 分钟、提前取消免费 15 分钟、占位宽限 10 分钟、违约金 no_show ¥5、信用分阈值 60。

## 8. 页面改动清单

| 页面 | 动作 |
| --- | --- |
| `StationDetailPage.qml` | 「立即充电」→「立即预约」，弹预约表单（或进独立页） |
| 新增 `ReservePage.qml` | 预约表单 + 「有空位 / 进入排队 / 定时预约」三种结果提示 |
| 新增 `QueuePage.qml` | 排队列表 + 预计等待 + 取消 / 顺延 |
| 改造 `ScanPage.qml` | 接收「目标桩码」上下文，校验一致后才放行 |
| 新增 `ChargingPage.qml` | 实时进度（SOC/功率/费用/ETA）+ 异常 push 提示 |
| 新增 `SettlePage.qml` | 选券/积分抵扣 + 扣余额 + 占位费展示（完成态复用现有订单详情） |

## 9. 实现顺序

1. 改真相源：`spec-数据库.md`、`DATA_STRUCTURE.md`、`spec-协议.md`（本文件第 5、6 节）+ `充电全流程-变更同步.md`。
2. 新增 `ChargingFlow` 单例。
3. `StationDetailPage` 改「立即预约」+ 新增 `ReservePage`。
4. 新增 `QueuePage`。
5. 改造 `ScanPage`。
6. 新增 `ChargingPage`。
7. 新增 `SettlePage`。
8. 联调自测各违约分支。