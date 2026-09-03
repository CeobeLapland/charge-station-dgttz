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

| type | 说明 |
| --- | --- |
| `system.ping` | 心跳，回 `system.pong` |
| `user.login` | 手机号免密登录，不存在则自动注册 |
| `station.nearby` | 附近电站列表，按距离排序 |
| `station.detail` | 电站 + 电桩明细 |

其余消息按队友需求优先级陆续实现。

## 联调提示

- 各端连**本机** `ws://127.0.0.1:9000`。每人在自己虚拟机里跑一份服务端，几台虚拟机之间不互通，不要用 IP。
- 服务端日志会打印 `[连接]` / `[收到] <type>` / `[断开]`。**连不上时先看这里**：有 `[连接]` 说明请求到达了，问题在消息格式；一片安静说明根本没连上，问题在地址端口。
- 测试账号：用户 `13800000001` 起；管理员 `admin` / `123456`。

## 目录

```
server/
├── sql/schema.sql      建表脚本（31张表，冻结真相源）
├── sql/make_seed.py    建库 + 种子数据生成器
├── src/Database.*      打开数据库、开外键
├── src/UserDao.*       user 表数据访问（★ SQL 只写在 DAO 层）
├── src/StationDao.*    station / charger / price_rule 数据访问
├── src/WsServer.*      WebSocket 主循环：收发 JSON、按 type 分发（★ 不写 SQL）
└── src/main.cpp        入口：解析参数 → 开库 → 启动服务
```