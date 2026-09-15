# 后续扩充计划（Big Demo 增强路线图）

> 目的：把「接下来还能加什么」完整记录下来，避免在长对话里遗忘。每条含「为什么做、怎么做、优先级、需不需要接到 Hive」。
> 现状基线：大屏已接 Hive 真实数据并秒回缓存；Spark-SQL 8维分析可跑；PME 负荷预测已上大屏；/hadoop 查询页有 SPARK-SQL 板块；30 表在 Hive `chargestation`；浅色主题已完成。
> ✅ 重大历史 Bug 均已修复：/api/snapshot 卡顿(改走 dash_cache.json Spark 预聚合缓存)、"近7天电量收入""负荷预测"空白(energyRevenue 双Y轴崩溃→单次 setOption)。筛选做了保底(见 §七)。

---

## 0. 交接速查（新会话/新组员必读，快速上手）

**演示环境**：CentOS7 虚拟机 `192.168.176.100`(node100)，用户/密码 `hadoop/hadoop`。SecureCRT 连接。
**三入口**：
- 大屏：`http://192.168.176.100:8080/index.html`（默认 mock；带 `?mode=live` 走 Hive 真实数据）
- Hadoop 查询页：`http://192.168.176.100:8080/hadoop`（含 SPARK-SQL 多维分析按钮；已并入大屏弹窗，此页保留）
- 开发者模式：`http://192.168.176.100:8080/admin`（数据表行数 / CSV·TSV 导入 / 手动添加 / 清空 / 重建大屏缓存）
- Hadoop 官方 UI：HDFS `:9870`、YARN `:8088`

**一键启动（关机重启后）**：见《Hadoop环境操作与重启恢复手册》§3：
```bash
start-all.sh          # 启动 HDFS+YARN
nohup hiveserver2 > ~/hs2.log 2>&1 &    # 启动 Hive
cd ~/screen && nohup python3 app.py 8080 /home/hadoop/screen > ~/gateway.log 2>&1 &  # 启动网关
# 网关读 ~/screen/dash_cache.json 秒回；若删除它需重跑:
# spark-submit --master local[2] /home/hadoop/screen/dash_engine.py /home/hadoop/screen/dash_cache.json
```

**关键文件（服务器 ~/screen/）**：
- `app.py` 网关（/api/snapshot 读 dash_cache 秒回、/api/forecast PME、/api/spark、/api/hive、/hadoop 页）
- `dash_engine.py`  + `dash_cache.json`  Spark 预聚合缓存（大屏秒回的核心）
- `index.html` / `css/` / `js/`  大屏前端
- 仓库同步源在 Windows：`screen/`、`machine_learning/`、`docs/`

**改前必知**：
- 数据都在 Hive（spark 直连 MySQL metastore，已配好）。
- Spark 查询每条 ~15-30s（起 JVM），**别在大屏热路径逐条 spark**——用了 dash_cache 缓存回避。
- git 提交前 `git status` 确认。

---

## 一、有待处理的 Bug / 已知限制
- ✅ 历史大屏 Bug 已全修复（卡顿、空白图）。
- ⚠️ 筛选联动：界面可用、秒回不崩，但**暂按全量展示**（未真正按所选范围重算）。原因：spark 单条启动过慢。若要做真筛选，需 Spark 常驻会话(保活 pyspark)方案。

## 二、机器学习扩充（面子工程，可逐个加）

现有 C++ 引擎都在 `machine_learning/`，核心是可解释白盒 → 用 Python 复刻上大屏。按「演示最有感」排序：

| 引擎 | 上大屏成什么面板 | 优先级 | 数据来源(Hive) |
|---|---|---|---|
| PME 负荷预测 | ✅ 已有（需修 Bug） | — | charging_measure |
| **DemandForecast 需求热力** | 未来时段/区域"需求热力"图（小时×星期×区域聚合，激增预警）| ⭐高 | charging_order |
| **HealthAssessment 健康度** | 设备健康度分布/高风险桩标识 | 高 | charger.health_score |
| **Recommendation 推荐** | 站点评分/相似站（可选） | 中 | station+charger |
| **WhatIf 推演** | "增 N 桩|降 X 价 → 预计订单/营收"推演面板 | 高(演示很唬人) | charging_order 聚合 |
| ReviewTag 评价分析 | 标签 TOP5 + 五维评分 | 中 | review |
| WaitTime 等待预估 | 各站未来 N 分钟等待 | 低 | reservation+order |

**建议做成**：一个 `/api/ml` 聚合接口返回多项，大屏增 1~3 个面板（需求热力、健康度、whatif 最出彩）。其余按时间挑。

---

## 三、网页功能扩充（美观 + 交互）

| 项 | 内容 | 优先级 |
|---|---|---|
| 数字滚动动画 | 指标卡数字滚动 | 中 |
| 站点下钻 | 点击地图站点 → 看该站明细（桩、负荷、订单）| 中 |
| 实时功率滚动 | 全网功率实时曲线滚动（秒级）| 中 |
| 筛选联动 | 日期/区域/站点筛选 → 大屏全部模块联动刷新（目前筛选只是摆设）| 高 |
| 告警/事件 置顶高亮 | 新告警闪烁 | 低 |
| forecast 置信区间开关 | 预测/实际曲线 Toggle | 低 |

**注意**：所有加的功能优先基于现有 Hive 数据，若字段缺用"样例/假数据"兜底（spec-大屏 已认可）。

---

## 四、开发者模式 / 数据导入（对接大数据）

**痛点**：现在增删数据要手写 SQL、上传 tsv 重灌，不便。若老师给几万条数据更是灾难。

### 方案 A：Web 数据导入接口（推荐）
- 在网关加 `POST /api/import`：前端选一个 CSV/TSV 文件 + 指定表名 → 上传 → 服务端解析 → `load data inpath` 进对应 Hive 表。
- 配套一个 `/admin` 页面：列出 30 表、显示行数、选文件导入、清空表。
- 价值：**老师给数据时直接传文件就进 Hive**，最省事，演示也能现场"导入→查询"。

### 方案 B：造大数据脚本
- 一个 Python 脚本 `gen_bigdata.py`：批量生成任意行数的 CSV（如 100 万行 charge_log）→ 灌进 Hive → 演示"海量数据处理"。
- 价值：证明能处理大数据量；配合 spark 分析很唬人。

### 方案 C：SQL 执行面板
- /hadoop 页支持直接执行任意 SQL（已有此能力），可当"开发者控制台"。

**建议**：做 A（导入接口）+ B（造数脚本），C 已有。

---

## 五、`/hadoop` 页与大屏的关系（你问的）

- `/hadoop` 页是**自建演示工具，不是 hadoop 必需**。当前价值：单独强调"Hadoop 查询/SPark 分析"。
- ✅ 已合并：大屏顶部新增「**大数据分析**」按钮 → 弹窗内嵌 SPARK-SQL / Hive 控制台（快捷分析同款 8 维查询，可切引擎）。`/hadoop` 独立页保留不删。
- 大屏顶部另有「**开发者**」按钮 → `/admin` 开发者模式（导入/手动加数/重建缓存）。

---

## 六、推荐执行顺序

1. **修 `/api/snapshot` 卡顿 Bug**（不修，下面全白搭）
2. **深色主题 + 筛选联动**（大屏观感与交互第一步）
3. **开发者模式的数据导入接口**（对接"老师喂大数据"）
4. **机器学习 1~2 个面板**（需求热力或 whatif，唬人加分）
5. **美化细节**（动画、下钻、告警高亮）

> 每次做一块都同步：Windows 改 → scp 传 → 重启网关 → 浏览器强刷验证 → 更新本文 + git。

---

## 七、待办坑位（防遗忘）

- [x] 修 /api/snapshot 超时/卡顿（改走 dash_cache.json Spark 预聚合缓存，秒回）
- [x] 修"近7天电量收入"空白（energyRevenue 分两次 setOption 双Y轴崩溃 → 单次 setOption）
- [x] 修"未来24h负荷预测"空白（同上 + 读缓存）
- [x] 浅色主题质感提升（dashboard.css 重写：圆角/阴影/渐变指标卡）
- [~] 筛选联动：界面可用；实时按筛选重算因 spark 单条启动过慢(~30s)，已回退为"筛选时仍展示全量(秒回不崩)"。**若要真正重算**，需 Spark 会话保活(常驻 pyspark)方案——后续可选优化
- [x] 数据导入接口 /admin（文件导入 TSV/CSV + 手动填表加数 + 表行数 + 清空 + 一键重建大屏缓存；需 SecureCRT 传新文件后验证）
- [x] /admin 行数提速：dash_engine 单 Spark 会话顺带算全表行数进 dash_cache（秒回），缓存缺失走 spark 单会话兜底（~30~60s），不再 beeline 串行 30 个 MR（原来七八分钟）
- [x] hadoop 并入大屏（顶部「大数据分析」弹窗内嵌 SPARK-SQL/Hive 控制台；「开发者」按钮进 /admin；顶栏新增「Hadoop 查询」按钮，/hadoop 页加返回大屏导航）
- [ ] 造大数据脚本 gen_bigdata.py
- [ ] ML: 需求热力面板
- [ ] ML: whatif 推演面板
- [ ] 站点下钻 / 地图浮窗
- [ ] 指标数字滚动动画