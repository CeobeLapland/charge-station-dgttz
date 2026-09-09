# machine_learning —— 机器学习引擎层

充电桩平台的机器学习能力层：Qt6/C++17 静态库 `ml_engine` + 独立自检程序 `ml_demo` + 仿真数据集生成脚本。算法规格唯一真相源是 `ML-Algorithm-Docs.md`，本目录严格照其实现。

## 一键构建与自检（在仓库根目录执行）

```bash
# 1. 生成仿真数据集 machine_learning/data/ml_sim.db（固定种子，可复现）
python3 machine_learning/tools/make_ml_data.py

# 2. 独立构建（不依赖仓库根 CMake）
cmake -S machine_learning -B machine_learning/build
cmake --build machine_learning/build -j

# 3. 运行自检（默认自动定位 data/ml_sim.db，也可显式传 db 路径）
./machine_learning/build/ml_demo [db路径]
```

退出码 0 = 全部自检通过。当前实测：负荷预测回测（前 13 天训练 → 预测第 14 天，小时级 MAPE）四站分别为 **4.71% / 6.50% / 5.99% / 4.78%**，最差 6.50%，满足 ≤15% 的验收标准。

数据集内容：4 个站（高新区/商业区/住宅区/工业区，设施含雨棚/地下停车差异）、每站 8–10 台桩（含 2 台故障桩）、14 天整点 charging_measure（基线 + 早晚双峰 + 噪声）、658 笔历史订单、排队 reservation、weather/price_rule/alarm/review/user/holiday 等，直接复用 `server/sql/schema.sql` 建库。

## 引擎一览（公式与参数见 ML-Algorithm-Docs.md）

| 引擎 | 一句话说明 | 对应协议消息 |
|------|-----------|--------------|
| LoadForecastingEngine | §2 PME 周期性移动平均外推（7×24 分桶、k=1 平滑、增长率 g、σ=0.05 乘性噪声、1.645σ 置信区间）+ §6 天气修正；数据不足走 §13.2 固定双峰模板 | `ml.forecast` / `ml.forecast_resp` / `push.forecast` |
| HealthAssessmentEngine | §3 扣分制健康度（温度/通信 15/功率波动 20·min(CV,2)/历史异常 5×次），R=clip((100−H)/100×0.8+N(0,0.02²),0,1)，算完写回 `charger.health_score` | `admin.fault_risk` / `admin.fault_risk_resp` |
| RecommendationEngine | §4 距离指数衰减(d0=5km)+价格 1/(1+2p)+空闲率+健康度加权，balanced/fastest/cheapest 三套权重，雨天有雨棚 ×1.15、极端天气室外 ×0.50 | `station.recommend` / `station.recommend_resp` |
| WaitTimeEngine | §5 W=Q×历史同期平均剩余时长，空闲桩>0 时 W=0，10/20 分钟后按负荷预测外推（η=0.9/0.8），四档建议文案 | 用户端站点详情/列表等待时间 |
| DemandForecastEngine | §7 星期×小时×区域订单聚合，Δ>0.5 触发激增预警，输出 top_surge_windows 与增配建议 | 大屏需求热力图 |
| DispatchEngine | §8 触发条件（空闲率<20%/排队>5/预测 1h 负荷>额定 90%），动作优先级 A4→A1→A3→A2 | 大屏/管理端调度提示 |
| RiskControlEngine | §9 规则 1（24h 预约≥10 且取消率≥0.6）+ 规则 2（<5 分钟充电≥3 次/天），50/30/20 加权评分，41–70 限预约、71–100 冻结 24h | 管理端风控列表 |
| WhatIfEngine | §10 四参数（add_chargers/price_delta/failure_scale/traffic_delta，clip 到文档范围）推演平均等待、峰值利用率、日订单、日营收 | `admin.whatif` / `admin.whatif_resp` |
| ReviewTagAnalyzer | §11 标签频次 + 30 天环比 MoM + TOP5 + 五维评分均值 | 管理端评价分析 |
| AssistantEngine | §12 关键词意图匹配（营收/最忙/故障/用户增长/负荷预测），模板填真实查询结果，未命中给 fallback 文案 | `admin.assistant_query` / `admin.assistant_query_resp` |

门面 `MlEngine` 组合全部引擎，对外提供与协议对齐的 `QJsonObject` 接口，JSON 键一律小写蛇形；数据不足时走 §13 兜底并在 JSON 中带 `"fallback": true`。

## 给 server 组的接入说明

1. **链接库**：server 的 CMakeLists 任选其一：
   ```cmake
   add_subdirectory(machine_learning)          # 或直接
   target_link_libraries(server PRIVATE ml_engine)
   ```
   `ml_engine` 公开 include 目录并链接 Qt6::Core Qt6::Sql，C++17。

2. **打开数据库**（用 server 的同一个 db 文件即可）：
   ```cpp
   ml::MlEngine mlEngine;
   mlEngine.openDatabase("/path/to/charge.db");
   ```

3. **替换 5001**：在 `handleMlForecast` 里把"模型未接入"的 5001 返回换成：
   ```cpp
   QJsonObject resp = mlEngine.forecast(stationId, horizon);
   // resp 即 ml.forecast_resp：code/station_id/horizon/generated_at/history[]/forecast[]
   // history[] 为大屏"实线"actual 序列（time/power），forecast[] 为虚线+置信区间
   ```
   其余入口同理：`mlEngine.recommend(lon, lat, category)`（station.recommend）、`mlEngine.faultRisk()`（admin.fault_risk）、`mlEngine.whatIf(scenario)`（admin.whatif）、`mlEngine.assistantQuery(question)`（admin.assistant_query）、另有 `waitEstimate/demandHeatmap/dispatchCheck/riskUsers/reviewTags` 可供管理端与大屏取用。

4. **数据库连接已隔离**：ML 层使用具名连接 `QSqlDatabase::addDatabase("QSQLITE", "ml_engine")`，绝不占用默认连接，与 server 自身的默认连接可打开同一 db 文件并存，互不干扰。注意 SQLite 同文件多连接并发写有限制，ML 层只写 `charger.health_score` 一个字段（聚合口径要求），建议由 server 串行调用 ML 接口。

## 参数速查表（附录 A）与实际取值

| 参数名 | 建议初值 | 可调范围 | 本实现取值 |
|--------|----------|----------|-----------|
| 负荷预测历史天数 D | 7 | 3–14 | **7** |
| 移动平均窗口 k | 1 | 0–3 | **1** |
| 增长系数 g | 0.02 | 0–0.1 | clip 到 **[0, 0.1]**（负增长率截断为 0） |
| 乘性噪声 σ_ε | 0.05 | 0.01–0.1 | **0.05** |
| 温度惩罚阈值 | 50℃ | 40–60℃ | **50℃** |
| 健康度告警阈值 | 60 | 50–70 | **60**（H<60 视为告警，管理端标红口径） |
| 推荐权重 w_d/w_p/w_w/w_h | 0.3/0.3/0.2/0.2 | 总和=1 | **按文档三套权重原值** |
| 风控预约次数阈值 | 10 | 5–20 | **10** |
| 风控取消率阈值 | 0.6 | 0.5–0.8 | **0.6** |
| 调度空闲率阈值 | 0.2 | 0.15–0.3 | **0.2** |

MAPE 回测一次通过（最差 6.50% ≤ 15%），未做超出附录 A 范围的调参。可复现性：预测引擎默认种子 42（`setSeed()` 可注入），数据脚本种子 20260909；相同小时内重新生成数据并运行 demo，结果逐位一致。

实现口径备注：§6 天气修正系数表按"室外站/室内（有雨棚或地下停车）站"两列取单一乘数 γ；§5 历史同期平均剩余时长取最近 7 天同小时已完成订单的平均时长 ÷ 2；§10 的 p̄ 取历史订单平均实付/平均电量（元/kWh）。
