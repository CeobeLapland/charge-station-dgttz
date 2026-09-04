# server — 服务端 / 数据库端

无界面 C++ 进程，是全系统**唯一**读写 `charge.db` 的程序。
各端不直连数据库，一律通过 WebSocket `ws://127.0.0.1:9000` 向它请求数据。
协议见 `docs/content/spec-协议.md`，表结构见 `DATA_STRUCTURE.md`。

## 在自己机器上跑起来（4 步）

环境：Ubuntu 22.04 + Qt 6.2(apt 版) + CMake 3.22 + g++ 11

### 1. 装依赖
```bash
sudo apt install cmake g++ python3 qt6-base-dev libqt6websockets6-dev
```
> WebSockets 的包名是 `libqt6websockets6-dev`

### 2. 生成数据库
```bash
cd server/sql
python3 make_seed.py
```
`charge.db` 不在仓库里（`.gitignore` 忽略 `*.db`），得自己生成。
脚本固定了随机种子，**所有人生成的数据完全一致**，方便联调对数。
改了 `schema.sql` 之后重跑一次即可（会删掉旧库重建）。

### 3. 编译
```bash
cd server
cmake -B build
cmake --build build
```

### 4. 启动
```bash
cd build
./charge_server
```
看到 `服务端运行中, 等待各端连接...` 即成功，`Ctrl+C` 停止。

**必须在 `build/` 目录下启动**——默认库路径 `../sql/charge.db` 是相对路径。
换路径或端口：`./charge_server --db /绝对路径/charge.db --port 9000`
只测数据库不启网络：`./charge_server --selftest`

## 目前已实现的协议消息

**系统 / 用户端**

| type | 说明 |
| --- | --- |
| `system.ping` | 心跳，回 `system.pong`（注意不是 `_resp`） |
| `user.login` | 手机号免密登录，不存在则自动注册；成功后把身份绑到这条连接 |
| `user.info` | 当前登录用户信息，**不传 user_id** |
| `user.recharge` | 余额充值（模拟支付），事务内写余额 + 钱包流水 |
| `station.nearby` | 附近电站列表，按距离排序 |
| `station.detail` | 电站 + 电桩明细 |

**订单流程**

| type | 说明 |
| --- | --- |
| `order.create` | 预约下单，电桩置 reserved |
| `order.start` | 开始充电，**同时交给仿真线程** |
| `order.finish` | 结束充电，转待结算 |
| `order.settle` | 结算：一个事务写 5 张表 |
| `order.cancel` | 取消（仅 reserved 可取消） |
| `order.list` / `order.detail` | 我的订单 / 详情（含时间轴） |

**管理端**

`admin.login` / `revenue` / `station_status` / `station_list` / `station_detail` / `station_add` /
`charger_list` / `charger_restart` / `charger_pause` / `user_list` / `user_toggle_status` /
`device_log` / `fault_risk`

**大屏**

`screen.snapshot`（免登录只读）、`ml.forecast`（当前返回 5001，等机器学习模块接入）

**服务端推送**

`push.order_progress`（定向发给该用户）、`push.charger_status`、`push.device_log`、`push.order_event`

## 充电仿真与多线程

演示时不可能真等一小时把车充满，所以服务端内置了一个**充电仿真器**：

- `order.start` 之后，仿真线程每 **1 秒**推进 **1 分钟**的充电过程（60 倍速）
- 每秒把 SOC / 电量 / 费用 / 剩余时间通过 `push.order_progress` **定向推给该用户**
- 每 15 秒往 `charging_measure` 落一条时序点，喂大屏的负荷曲线
- 充到目标电量自动结束充电，转待结算
- 涓流模拟：SOC ≥ 80% 功率减半，≥ 95% 再减半

### 线程模型

```
主线程 (WsServer)                  工作线程 (ChargeSimulator)
  · WebSocket 收发                    · QTimer 每秒 tick
  · 所有数据库读写                     · 纯计算：推进 SOC / 电量 / 费用
  · 广播与定向推送                     · 一行 SQL 都不碰
        ↑                                    │
        └──────── Qt 跨线程信号槽(队列连接) ────┘
```

**为什么数据库只留在主线程？** 因为 SQLite 的连接绑定在创建它的线程上，`QSqlDatabase`
的连接不能跨线程共享。把所有写操作收在一个线程里串行执行，比到处加锁更简单、也更不容易错；
工作线程只做计算，通过信号把结果拷贝给主线程，全程不需要互斥锁。

验证有两个线程在跑：

```bash
./charge_server &
ls /proc/$(pgrep -x charge_server)/task | while read t; do cat /proc/$(pgrep -x charge_server)/task/$t/comm; done
# charge_server   ← 主线程
# QThread         ← 充电仿真线程
```

（Qt 的 `QThread` 在 Linux 上底层就是 pthread。）

## 联调提示

- 各端连**本机** `ws://127.0.0.1:9000`。每人在自己虚拟机里跑一份服务端，几台虚拟机之间不互通，不要用 IP。
- 服务端日志会打印 `[连接]` / `[收到] <type>` / `[断开]`。**连不上时先看这里**：有 `[连接]` 说明请求到达了，问题在消息格式；一片安静说明根本没连上，问题在地址端口。
- 测试账号：用户 `13800000001` 起；管理员 `admin` / `123456`。
- **订单相关消息不要传 `user_id`**：服务端从 WebSocket 连接上取当前登录用户，客户端上报的一律忽略。
- `order.finish` 不传 `end_soc` 时，服务端会用仿真器算到的当前电量结算。

## 目录

```
server/
├── sql/schema.sql          建表脚本（31 张表）
├── sql/make_seed.py        建库 + 种子数据生成器（随机种子固定）
├── src/Database.*          打开数据库、开外键、校验库是好的
├── src/UserDao.*           user 表 + 充值            ★ SQL 只写在 DAO 层
├── src/StationDao.*        station / charger / price_rule
├── src/OrderDao.*          订单状态机、计费、结算五表事务
├── src/AdminDao.*          管理端统计与运维操作
├── src/ScreenDao.*         大屏聚合查询
├── src/ChargeSimulator.*   充电仿真（工作线程，纯计算，不碰 SQL）
├── src/WsServer.*          WebSocket 主循环：收发 JSON、按 type 分发  ★ 不写 SQL
└── src/main.cpp            入口：解析参数 → 开库 → 启动服务
```

分层规矩只有两条，但必须守住：

- **只有 `*Dao` 允许写 SQL**，改表结构时只动 DAO，`WsServer` 一个字不用改
- **`WsServer` 不碰 SQL、`Dao` 不碰 JSON**，所以 `--selftest` 能在完全不启动网络的情况下测数据库逻辑
