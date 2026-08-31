# AGENTS.md

本文件是仓库内所有 AI 助手的全局操作规则。在任何代码或文档改动前，先读本文件与任务对应的 `docs/spec-*.md`。

## 项目一句话

基于 Ubuntu 22.04 + Qt 6 的电动汽车充电运营平台，由用户端（QML）、管理端（Widgets）、数据库（QSQLite）、Web 大屏（ECharts）、机器学习（C++ 规则+仿真）五个子系统组成，全端通过一个无界面 C++ 服务进程以 WebSocket + JSON 通信。

## 技术栈与构建

- 语言：C++17，界面用户端用 QML（Qt Quick），管理端用 Qt Widgets + QSS。
- 数据库：QSQLite 单文件，经 Qt SQL 访问。
- 通信：QWebSocketServer（服务端）/ QWebSocket（各端），JSON 载荷。
- 并发：QThread + QTimer，服务端用定时器线程模拟设备与采集数据。
- 构建：CMake（要求使用 CMake 而非 qmake）。

常用命令（在仓库根执行）：

```bash
cmake -S . -B build
cmake --build build -j
# 运行服务端
./build/server
# 运行用户端 / 管理端
./build/user_client
./build/admin_client
```

大屏为纯前端页面，用浏览器打开 `screen/index.html`，开发期可用假数据 JSON 独立运行。

## 目录结构约定

- `server/` —— 无界面服务进程（WebSocket 服务端 + SQLite 读写 + 设备模拟线程）。
- `user/` 用户端、`admin/` 管理端 —— Qt 桌面程序。
- `screen/` —— 大屏前端（HTML + JS + ECharts）。
- `docs/` —— 全部规格与文档（必要时可修改）。
- 公共代码（枚举字典、协议常量、消息封装）放 `common/`，各端 include 而非复制。

## 规范

- C++ 标识符：类/函数大驼峰，变量/成员小驼峰，常量全大写蛇形。
- 数据库表与字段：英文小写蛇形（见 `docs/spec-数据库.md`）。
- JSON 消息键：小写蛇形（见 `docs/spec-协议.md`）。
- 提交信息：`<端>: <动词+对象>`，如 `server: add device simulator thread`。
- 分支：功能分支按端命名（`user/xxx`、`admin/xxx`），合入前先同步 main。

## 共享真相源（必须遵守）

- **枚举、字段、表结构**：唯一权威是 `docs/spec-数据库.md`。任何端需要"状态值、类型值、字段名"时从这里取值，禁止在代码里自行硬编码一套。但注意：！！！后续可以添加更改！！！。
- **消息类型、错误码、JSON 结构**：唯一权威是 `docs/spec-协议.md`。新增消息必须先在协议里补条再实现。但注意：！！！后续可以添加更改！！！。
- **订单状态机、设备状态口径**：定义在 `docs/spec-用户端.md`、`docs/spec-管理端.md`，跨端展示必须一致。
- 修改 schema 或协议属于"解冻"操作，必须：改真相源文档 → 全组/全端同步 → 再动代码，禁止只改一处。

## 待定项处理

规格中出现的 `[待定]` 表示实现期需要人确认的参数（公式权重、阈值、端口号等）。遇到 `[待定]` 时：不编造、不跳过功能，采用该 spec 末尾「待定项清单」里给出的建议初值继续实现，并在提交信息里注明使用了哪项待定的初值。

## 禁忌

- 不要删除或降级任何 `【基础】` 功能。
- 不要把机器学习做成重的 Python 依赖链；主方案为 C++ 内规则+仿真，Python FastAPI 仅作可选的加分端点。
- 不要让用户端/管理端直连数据库文件，一律走服务端 WebSocket。
- 大屏不能作为数据源头，只做汇总呈现。
- 不要为了"看起来高级"引入规格之外的抽象层。

## Git 协作约定

- 环境与首次提交流程见 `DEPLOYMENT_STEP_3_Git.md`。
- 串行冲突点：`docs/spec-数据库.md`、`docs/spec-协议.md`、`common/` 目录改动前先拉取最新并通知全组。
- 提交前自查：枚举/协议是否与真相源一致、是否引入未在规格中的功能。
