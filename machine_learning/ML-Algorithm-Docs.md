# 东软电动汽车充电桩应用管理平台 —— 机器学习算法文档

> 版本：v1.0  
> 编制日期：2026-09-03  
> 对应仓库：https://github.com/CeobeLapland/charge-station-dgttz  
> 依据文档：`docs/spec-机器学习.md`、`docs/spec-数据库.md`

---

## 目录

1. [概述](#一概述)
2. [充电负荷智能预测算法](#二充电负荷智能预测算法)
3. [设备健康度与故障预测算法](#三设备健康度与故障预测算法)
4. [智能推荐引擎算法](#四智能推荐引擎算法)
5. [等待时间预估算法](#五等待时间预估算法)
6. [天气维度修正模型](#六天气维度修正模型)
7. [用户需求预测算法](#七用户需求预测算法)
8. [智能调度算法](#八智能调度算法)
9. [智能风控算法](#九智能风控算法)
10. [what-if 决策仿真算法](#十what-if-决策仿真算法)
11. [评价标签分析算法](#十一评价标签分析算法)
12. [AI 运营助手算法](#十二ai-运营助手算法)
13. [模型评估与兜底策略](#十三模型评估与兜底策略)

---

## 一、概述

### 1.1 文档目的

本文档详细定义东软电动汽车充电桩应用管理平台中机器学习子系统所涉及的全部算法的数学模型、输入输出规格、实现逻辑与验收标准。作为开发人员的实现依据与测试人员的验收基准。

### 1.2 算法设计原则

| 原则 | 说明 |
|------|------|
| 可解释性优先 | 所有算法采用白盒规则或简单统计模型，确保输出可被追溯和复算 |
| 零外部依赖 | 主方案不依赖 Python/TensorFlow/sklearn 等外部库，全部在 C++ 内实现 |
| 实时性 | 单次预测/评分计算延迟控制在 100ms 以内 |
| 可降级 | 任何算法在数据不足或异常时，可平滑回退到默认规则策略 |

### 1.3 符号约定

| 符号 | 含义 |
|------|------|
| $P_t$ | 时刻 $t$ 的瞬时功率（kW） |
| $\bar{P}_h$ | 第 $h$ 小时的历史平均功率 |
| $H$ | 健康度评分（0–100） |
| $R$ | 故障风险概率（0.0–1.0） |
| $S$ | 站点评分（0.0–1.0） |
| $w_i$ | 第 $i$ 个特征的权重 |
| $\mathcal{N}(\mu, \sigma^2)$ | 均值为 $\mu$、方差为 $\sigma^2$ 的正态分布 |
| $\text{clip}(x, a, b)$ | 将 $x$ 截断到 $[a, b]$ 区间 |

---

## 二、充电负荷智能预测算法

### 2.1 问题定义

**输入**：充电站 $s$ 的历史负荷时序数据 $\{P_{t_1}, P_{t_2}, ..., P_{t_N}\}$，外部维度（天气、节假日）  
**输出**：未来 $T$ 小时（$T \in \{1, 6, 24\}$）的负荷预测序列 $\{\hat{P}_{t_{N+1}}, ..., \hat{P}_{t_{N+T}}\}$，含置信区间

### 2.2 核心算法：周期性移动平均外推（PME）

#### 2.2.1 算法思想

电动汽车充电负荷呈现强烈的**日周期性**（早晚双峰）与**周周期性**（工作日 vs 周末差异）。算法核心是利用历史同期数据匹配当前模式，再通过移动平均平滑噪声，最后外推未来值。

#### 2.2.2 数学模型

**步骤 1：历史同期聚合**

对于预测目标站 $s$，读取最近 $D$ 天（建议 $D=7$）的历史负荷数据，按"星期几 × 小时"分桶聚合：

```math
\bar{P}_{d,h}^{(s)} = \frac{1}{|\mathcal{O}_{d,h}|} \sum_{t \in \mathcal{O}_{d,h}} P_t^{(s)}
```

其中 $d \in \{0,1,...,6\}$（0=周一），$h \in \{0,1,...,23\}$，$\mathcal{O}_{d,h}$ 为属于星期 $d$ 第 $h$ 小时的所有采样时刻集合。

**步骤 2：移动平均平滑**

对 24 小时序列做中心移动平均，消除随机噪声：

```math
\tilde{P}_{d,h}^{(s)} = \frac{1}{2k+1} \sum_{i=-k}^{k} \bar{P}_{d,(h+i) \mod 24}^{(s)}
```

建议窗口 $k=1$（即 3 小时平滑）。

**步骤 3：趋势外推**

假设负荷存在线性增长趋势，计算最近 $D$ 天的日总量增长率：

```math
g = \frac{\sum_{h=0}^{23} \bar{P}_{\text{today},h} - \sum_{h=0}^{23} \bar{P}_{\text{today}-D,h}}{D \cdot \sum_{h=0}^{23} \bar{P}_{\text{today}-D,h}}
```

未来第 $i$ 小时（$i = 1, ..., T$）的预测值为：

```math
\hat{P}_{t_{N+i}} = \tilde{P}_{d^*, (h_0 + i) \mod 24} \cdot (1 + g)^{\lceil i/24 \rceil} \cdot \epsilon_i
```

其中：
- $d^*$ 为预测起始日的星期索引
- $h_0$ 为预测起始小时
- $\epsilon_i \sim \mathcal{N}(1, \sigma_\epsilon^2)$ 为乘性噪声（建议 $\sigma_\epsilon = 0.05$）

**步骤 4：置信区间**

基于历史预测残差的标准差 $\sigma_r$ 计算 90% 置信区间：

```math
\hat{P}_{t_{N+i}}^{\text{upper}} = \hat{P}_{t_{N+i}} \cdot (1 + 1.645 \cdot \sigma_r)
```
```math
\hat{P}_{t_{N+i}}^{\text{lower}} = \hat{P}_{t_{N+i}} \cdot (1 - 1.645 \cdot \sigma_r)
```

### 2.3 C++ 实现框架

```cpp
#include <QVector>
#include <QDateTime>
#include <cmath>
#include <random>

struct LoadForecast {
    int stationId;
    int horizon;               // 预测步长（小时）
    QVector<double> predicted; // 预测值
    QVector<double> upper;     // 置信上界
    QVector<double> lower;     // 置信下界
    QDateTime generatedAt;
};

class LoadForecastingEngine {
public:
    LoadForecast predict(int stationId, int horizonHours, 
                         const QDateTime& startTime) {
        // 1. 读取最近 7 天历史数据
        auto history = fetchHistory(stationId, 7);
        
        // 2. 构建 7×24 历史矩阵
        QVector<QVector<double>> histMatrix(7, QVector<double>(24, 0.0));
        QVector<QVector<int>> countMatrix(7, QVector<int>(24, 0));
        
        for (const auto& rec : history) {
            int dow = rec.timestamp.date().dayOfWeek() - 1; // 0-6
            int hour = rec.timestamp.time().hour();
            histMatrix[dow][hour] += rec.power;
            countMatrix[dow][hour]++;
        }
        
        // 3. 计算平均值
        for (int d = 0; d < 7; ++d)
            for (int h = 0; h < 24; ++h)
                if (countMatrix[d][h] > 0)
                    histMatrix[d][h] /= countMatrix[d][h];
        
        // 4. 3 小时移动平均平滑
        auto smoothed = movingAverageSmooth(histMatrix, 1);
        
        // 5. 计算日增长率
        double growthRate = computeGrowthRate(histMatrix);
        
        // 6. 外推预测
        int startDow = startTime.date().dayOfWeek() - 1;
        int startHour = startTime.time().hour();
        
        LoadForecast result;
        result.stationId = stationId;
        result.horizon = horizonHours;
        result.generatedAt = QDateTime::currentDateTime();
        
        std::mt19937 rng(QDateTime::currentDateTime().toSecsSinceEpoch());
        std::normal_distribution<double> noise(1.0, 0.05);
        
        for (int i = 0; i < horizonHours; ++i) {
            int targetDow = (startDow + (startHour + i) / 24) % 7;
            int targetHour = (startHour + i) % 24;
            double base = smoothed[targetDow][targetHour];
            int dayOffset = (startHour + i) / 24;
            double trend = std::pow(1.0 + growthRate, dayOffset);
            double pred = base * trend * noise(rng);
            
            result.predicted.append(std::max(0.0, pred));
            result.upper.append(std::max(0.0, pred * 1.08));
            result.lower.append(std::max(0.0, pred * 0.92));
        }
        
        return result;
    }
    
private:
    QVector<QVector<double>> movingAverageSmooth(
        const QVector<QVector<double>>& input, int k) {
        auto output = input;
        for (int d = 0; d < 7; ++d) {
            for (int h = 0; h < 24; ++h) {
                double sum = 0.0;
                int cnt = 0;
                for (int i = -k; i <= k; ++i) {
                    int hh = (h + i + 24) % 24;
                    sum += input[d][hh];
                    cnt++;
                }
                output[d][h] = sum / cnt;
            }
        }
        return output;
    }
    
    double computeGrowthRate(const QVector<QVector<double>>& matrix) {
        double todaySum = 0, weekAgoSum = 0;
        for (int h = 0; h < 24; ++h) {
            todaySum += matrix[6][h];      // 最近一天
            weekAgoSum += matrix[0][h];    // 7 天前
        }
        if (weekAgoSum < 0.001) return 0.0;
        return (todaySum - weekAgoSum) / (7.0 * weekAgoSum);
    }
    
    // ... fetchHistory 实现
};
```

### 2.4 输入输出规格

**输入**：
| 字段 | 类型 | 说明 |
|------|------|------|
| `station_id` | INTEGER | 目标充电站 ID |
| `horizon` | INTEGER | 预测时长：1 / 6 / 24（小时） |
| `start_time` | TEXT | 预测起始时间，格式 `YYYY-MM-DD HH:00:00` |

**输出**：
| 字段 | 类型 | 说明 |
|------|------|------|
| `predicted[]` | REAL[] | 预测负荷序列，单位 kW |
| `upper[]` | REAL[] | 90% 置信区间上界 |
| `lower[]` | REAL[] | 90% 置信区间下界 |
| `generated_at` | TEXT | 生成时间戳 |

### 2.5 验收标准

- 大屏展示：实线（历史实际值）+ 虚线（预测值）+ 阴影区（置信区间）
- 小时级平均绝对百分比误差（MAPE）：$\text{MAPE} = \frac{1}{T}\sum_{i=1}^{T} \frac{|\hat{P}_i - P_i|}{P_i} \leq 15\%$
- 预测曲线呈现明显的早晚双峰特征

---

## 三、设备健康度与故障预测算法

### 3.1 问题定义

**输入**：单台充电桩的实时状态与历史运行数据  
**输出**：健康度评分 $H \in [0, 100]$，24 小时故障风险概率 $R \in [0.0, 1.0]$

### 3.2 健康度评分模型

健康度为**扣分制**，满分 100，根据多维异常指标逐项扣减：

```math
H = \text{clip}\left(100 - \sum_{j=1}^{4} \text{penalty}_j,\ 0,\ 100\right)
```

#### 3.2.1 各惩罚项定义

**（1）温度惩罚**

```math
\text{penalty}_{\text{temp}} = \begin{cases}
0 & T \leq 50^{\circ}\text{C} \\
2 \cdot (T - 50) & 50 < T \leq 80 \\
60 + 5 \cdot (T - 80) & T > 80
\end{cases}
```

**（2）通信惩罚**

```math
\text{penalty}_{\text{comm}} = \begin{cases}
0 & \text{通信正常} \\
15 & \text{通信异常}
\end{cases}
```

**（3）功率波动惩罚**

计算最近 24 小时内功率序列 $\{P_1, P_2, ..., P_N\}$ 的变异系数（CV）：

```math
\mu_P = \frac{1}{N}\sum_{i=1}^{N} P_i, \quad \sigma_P = \sqrt{\frac{1}{N}\sum_{i=1}^{N}(P_i - \mu_P)^2}
```

```math
\text{CV} = \frac{\sigma_P}{\mu_P}, \quad \text{penalty}_{\text{power}} = 20 \cdot \min(\text{CV}, 2.0)
```

**（4）历史异常惩罚**

```math
\text{penalty}_{\text{fault}} = 5 \cdot N_{\text{fault}}
```

其中 $N_{\text{fault}}$ 为最近 30 天内该桩的故障/告警次数。

### 3.3 故障风险预测模型

基于健康度推导故障风险概率：

```math
R = \text{clip}\left(\frac{100 - H}{100} \cdot 0.8 + \mathcal{N}(0, 0.02^2),\ 0.0,\ 1.0\right)
```

风险等级划分：
| 风险概率 $R$ | 等级 | 颜色 |
|-------------|------|------|
| $0.0 \leq R < 0.3$ | low（低风险） | 绿色 |
| $0.3 \leq R < 0.6$ | medium（中风险） | 黄色 |
| $0.6 \leq R \leq 1.0$ | high（高风险） | 红色 |

### 3.4 C++ 实现框架

```cpp
struct ChargerHealth {
    int chargerId;
    int healthScore;        // 0-100
    double faultRisk;       // 0.0-1.0
    QString riskLevel;      // "low" / "medium" / "high"
    QMap<QString, double> penaltyBreakdown; // 各惩罚项明细
};

class HealthAssessmentEngine {
public:
    ChargerHealth assess(int chargerId) {
        auto charger = db->getCharger(chargerId);
        auto measures = db->getRecentMeasures(chargerId, 24); // 最近 24h
        
        int score = 100;
        QMap<QString, double> penalties;
        
        // 温度惩罚
        double tempPenalty = 0;
        if (charger.temperature > 50.0) {
            if (charger.temperature <= 80.0)
                tempPenalty = 2.0 * (charger.temperature - 50.0);
            else
                tempPenalty = 60.0 + 5.0 * (charger.temperature - 80.0);
        }
        penalties["temperature"] = tempPenalty;
        score -= static_cast<int>(tempPenalty);
        
        // 通信惩罚
        double commPenalty = (charger.commStatus == "abnormal") ? 15.0 : 0.0;
        penalties["communication"] = commPenalty;
        score -= static_cast<int>(commPenalty);
        
        // 功率波动惩罚
        double cv = computeCV(measures);
        double powerPenalty = 20.0 * std::min(cv, 2.0);
        penalties["power_variation"] = powerPenalty;
        score -= static_cast<int>(powerPenalty);
        
        // 历史异常惩罚
        int faultCount = db->countFaults(chargerId, 30); // 30 天内
        double faultPenalty = 5.0 * faultCount;
        penalties["historical_faults"] = faultPenalty;
        score -= static_cast<int>(faultPenalty);
        
        score = std::clamp(score, 0, 100);
        
        // 故障风险
        double risk = (100.0 - score) / 100.0 * 0.8;
        risk += randomGaussian(0.0, 0.02);
        risk = std::clamp(risk, 0.0, 1.0);
        
        QString level;
        if (risk < 0.3) level = "low";
        else if (risk < 0.6) level = "medium";
        else level = "high";
        
        return {chargerId, score, risk, level, penalties};
    }
    
private:
    double computeCV(const QVector<ChargingMeasure>& measures) {
        if (measures.isEmpty()) return 0.0;
        double sum = 0.0, sumSq = 0.0;
        for (const auto& m : measures) {
            sum += m.power;
            sumSq += m.power * m.power;
        }
        double n = measures.size();
        double mean = sum / n;
        double variance = sumSq / n - mean * mean;
        double stddev = std::sqrt(std::max(0.0, variance));
        return (mean > 0.001) ? stddev / mean : 0.0;
    }
};
```

### 3.5 验收标准

- 健康度评分范围 $[0, 100]$，计算结果可复算
- 健康度 $< 60$ 的桩在管理端标红并触发告警
- 高风险桩（$R \geq 0.6$）排序位于列表顶部

---

## 四、智能推荐引擎算法

### 4.1 问题定义

**输入**：用户当前位置、候选充电站集合、负荷预测结果、天气条件  
**输出**：按评分降序排列的站点评分列表，支持三类标签（最快/最省/综合最优）

### 4.2 评分模型

#### 4.2.1 基础评分公式

对于候选站 $s$ 和用户 $u$：

```math
S_{\text{balanced}}(s, u) = w_d \cdot f_d(s, u) + w_p \cdot f_p(s) + w_w \cdot f_w(s) + w_h \cdot f_h(s)
```

其中特征函数定义如下：

**距离特征** $f_d$（指数衰减，越近越好）：

```math
f_d(s, u) = \exp\left(-\frac{d(s, u)}{d_0}\right)
```

其中 $d(s, u)$ 为用户到站的直线距离（km），$d_0 = 5$ km 为衰减常数。

**价格特征** $f_p$（价格越低越好）：

```math
f_p(s) = \frac{1}{1 + \alpha \cdot p_s}
```

其中 $p_s$ 为站 $s$ 的服务费（元/kWh），$\alpha = 2.0$ 为敏感度参数。

**等待特征** $f_w$（空闲桩比例越高越好）：

```math
f_w(s) = \frac{N_{\text{idle}}(s)}{N_{\text{total}}(s)}
```

**健康度特征** $f_h$（设备平均健康度越高越好）：

```math
f_h(s) = \frac{1}{N_{\text{total}}(s)} \sum_{c \in s} \frac{H_c}{100}
```

#### 4.2.2 权重配置（默认 / 三类标签）

| 标签 | $w_d$（距离） | $w_p$（价格） | $w_w$（等待） | $w_h$（健康） |
|------|-------------|-------------|-------------|-------------|
| balanced（综合最优） | 0.30 | 0.30 | 0.20 | 0.20 |
| fastest（最快充电） | 0.20 | 0.10 | 0.50 | 0.20 |
| cheapest（最省钱） | 0.15 | 0.55 | 0.15 | 0.15 |

#### 4.2.3 天气修正

雨天时，对室内站/有雨棚站施加加分：

```math
S_{\text{rain}}(s) = S(s) \cdot \left(1 + 0.15 \cdot \mathbb{1}_{[s\text{ 有雨棚或地下停车}]}\right)
```

极端天气时，所有室外站评分乘以 0.5。

### 4.3 C++ 实现框架

```cpp
struct StationScore {
    int stationId;
    double score;
    QString category;
    QMap<QString, double> featureBreakdown;
};

class RecommendationEngine {
public:
    QList<StationScore> rankStations(
        const User& user, 
        const QList<Station>& candidates,
        const QString& category,  // "balanced" / "fastest" / "cheapest"
        const QString& weather) {
        
        // 获取权重
        auto weights = getWeights(category);
        QList<StationScore> results;
        
        for (const auto& station : candidates) {
            double fd = exp(-user.distanceTo(station) / 5.0);
            double fp = 1.0 / (1.0 + 2.0 * station.serviceFee);
            double fw = station.idleRate();
            double fh = station.avgHealthScore() / 100.0;
            
            double score = weights.d * fd + weights.p * fp 
                         + weights.w * fw + weights.h * fh;
            
            // 天气修正
            if (weather == "rain" && station.hasRainShelter())
                score *= 1.15;
            else if (weather == "extreme" && !station.hasRainShelter())
                score *= 0.50;
            
            QMap<QString, double> breakdown;
            breakdown["distance"] = fd;
            breakdown["price"] = fp;
            breakdown["wait"] = fw;
            breakdown["health"] = fh;
            
            results.append({station.id, score, category, breakdown});
        }
        
        // 按评分降序排列
        std::sort(results.begin(), results.end(),
            [](const StationScore& a, const StationScore& b) {
                return a.score > b.score;
            });
        
        return results;
    }
    
private:
    struct Weights { double d, p, w, h; };
    
    Weights getWeights(const QString& category) {
        if (category == "fastest") return {0.20, 0.10, 0.50, 0.20};
        if (category == "cheapest") return {0.15, 0.55, 0.15, 0.15};
        return {0.30, 0.30, 0.20, 0.20}; // balanced
    }
};
```

### 4.4 验收标准

- 切换"最快/最省/综合"标签时，排序Top3发生变化
- 距离用户 500m 内空闲率 > 80% 的站，在"最快"模式下排名进入前 5
- 评分计算过程可追溯（日志输出各特征值与权重乘积）

---

## 五、等待时间预估算法

### 5.1 问题定义

**输入**：目标站当前排队队列长度、历史充电时长数据、负荷预测结果  
**输出**：当前预计等待时间（分钟），以及 10 分钟/20 分钟后等待时间预测与建议

### 5.2 核心模型

#### 5.2.1 当前等待时间

```math
W_{\text{now}} = Q \cdot \bar{t}_{\text{remain}}
```

其中：
- $Q$：当前排队人数（`reservation` 表中 `status = waiting` 的数量）
- $\bar{t}_{\text{remain}}$：历史同期该站平均剩余充电时长（分钟）

#### 5.2.2 历史同期平均剩余时长计算

```math
\bar{t}_{\text{remain}} = \frac{1}{|\mathcal{C}|} \sum_{c \in \mathcal{C}} \frac{t_{\text{total}}^{(c)} - t_{\text{elapsed}}^{(c)}}{2}
```

其中 $\mathcal{C}$ 为最近 7 天同小时时段内该站所有充电中的订单集合。除以 2 是因为平均而言当前正在充的订单还剩一半时间。

#### 5.2.3 未来等待时间预测

利用负荷预测推断未来队列变化：

```math
W_{t+\Delta} = W_{\text{now}} \cdot \frac{L_{\text{forecast}}(t+\Delta)}{L_{\text{current}}} \cdot \eta(\Delta)
```

其中：
- $L_{\text{forecast}}(t+\Delta)$：未来 $\Delta$ 时刻的预测负荷
- $L_{\text{current}}$：当前负荷
- $\eta(\Delta)$：衰减因子，$\eta(10\text{min}) = 0.9$，$\eta(20\text{min}) = 0.8$

#### 5.2.4 建议规则

```math
\text{suggestion} = \begin{cases}
\text{"建议现在前往"} & W_{\text{now}} \leq 5 \\
\text{"建议 10 分钟后前往"} & 5 < W_{\text{now}} \leq 15 \\
\text{"建议 20 分钟后前往"} & 15 < W_{\text{now}} \leq 30 \\
\text{"建议更换充电站"} & W_{\text{now}} > 30
\end{cases}
```

### 5.3 C++ 实现框架

```cpp
struct WaitEstimate {
    int currentMinutes;
    int after10Minutes;
    int after20Minutes;
    QString suggestion;
    int queueLength;
    double avgRemainingMinutes;
};

class WaitTimeEngine {
public:
    WaitEstimate estimate(int stationId, const QDateTime& now) {
        int queueLen = db->countWaitingReservations(stationId);
        double avgRemain = computeAvgRemainingTime(stationId, now);
        
        int current = static_cast<int>(queueLen * avgRemain);
        
        // 获取当前负荷与预测负荷
        double currentLoad = db->getCurrentLoad(stationId);
        auto forecast = loadEngine->predict(stationId, 1, now);
        double forecastLoad1h = forecast.predicted.isEmpty() 
            ? currentLoad : forecast.predicted.first();
        
        double loadRatio = (currentLoad > 0.001) 
            ? forecastLoad1h / currentLoad : 1.0;
        
        int after10 = static_cast<int>(current * loadRatio * 0.9);
        int after20 = static_cast<int>(current * loadRatio * 0.8);
        
        QString sug;
        if (current <= 5) sug = "建议现在前往";
        else if (current <= 15) sug = "建议 10 分钟后前往";
        else if (current <= 30) sug = "建议 20 分钟后前往";
        else sug = "建议更换充电站";
        
        return {current, after10, after20, sug, queueLen, avgRemain};
    }
    
private:
    double computeAvgRemainingTime(int stationId, const QDateTime& now) {
        // 取最近 7 天同小时的充电中订单
        auto activeOrders = db->getActiveOrdersAtHour(stationId, 
            now.time().hour(), 7);
        if (activeOrders.isEmpty()) return 20.0; // 默认 20 分钟
        
        double totalRemain = 0.0;
        for (const auto& order : activeOrders) {
            int elapsed = order.startTime.secsTo(now) / 60;
            int typicalTotal = db->getTypicalChargeDuration(order.chargerId);
            totalRemain += std::max(0, typicalTotal - elapsed) / 2.0;
        }
        return totalRemain / activeOrders.size();
    }
};
```

### 5.4 验收标准

- 队列人数变化时，等待时间实时更新
- 当前空闲桩 > 0 时，$W_{\text{now}} = 0$
- 建议结论与等待时间阈值匹配

---

## 六、天气维度修正模型

### 6.1 修正系数表

| 天气状况 | 室外站系数 | 室内/有雨棚站系数 | 对充电时长的影响 |
|----------|-----------|------------------|----------------|
| sunny（晴） | 1.00 | 1.00 | 无 |
| cloudy（多云） | 0.95 | 0.98 | 无 |
| rain（雨） | 0.89 | 1.18 | 无 |
| hot（高温 > 35℃） | 0.95 | 0.95 | +10% |
| extreme（极端天气） | 0.60 | 0.75 | +20% |

### 6.2 负荷预测中的天气修正

在负荷预测步骤 3 中加入天气乘数：

```math
\hat{P}_{t_{N+i}}^{(\text{weather})} = \hat{P}_{t_{N+i}} \cdot \gamma_{\text{weather}} \cdot \gamma_{\text{type}}
```

其中 $\gamma_{\text{weather}}$ 为天气系数，$\gamma_{\text{type}}$ 为站类型系数（室内/室外）。

### 6.3 验收标准

- 切换模拟天气后，大屏预测曲线在 30 秒内刷新并呈现差异
- 雨天用户端推荐列表中，室内站/有雨棚站排名平均上升 >= 3 位

---

## 七、用户需求预测算法

### 7.1 问题定义

**输入**：全平台历史订单数据  
**输出**：24×7 时段-区域需求热力图，以及需求激增预警

### 7.2 核心算法

#### 7.2.1 时段-需求聚合

按"星期几 $d$ × 小时 $h$ × 区域 $a$"三维聚合：

```math
D_{d,h,a} = \sum_{o \in \mathcal{O}_{d,h,a}} \mathbb{1}_{[o\text{ 为有效订单}]}
```

#### 7.2.2 需求激增检测

对每 $(d, h, a)$ 组合，计算相对基准的增幅：

```math
\Delta_{d,h,a} = \frac{D_{d,h,a} - \bar{D}_{h,a}}{\bar{D}_{h,a}}
```

其中 $\bar{D}_{h,a}$ 为该区域该小时的历史平均值（不区分星期）。

若 $\Delta_{d,h,a} > 0.5$（即需求比均值高 50% 以上），触发"需求激增"预警。

### 7.3 输出示例

```json
{
  "heatmap": [
    {"dow": 4, "hour": 18, "area": "商业区", "demand": 45, "surge": true, "delta": 0.62}
  ],
  "top_surge_windows": [
    {"window": "周五 18:00-20:00 商业区", "demand": 45, "recommendation": "建议在商业区增配快充桩 12 台"}
  ]
}
```

---

## 八、智能调度算法

### 8.1 调度触发条件

当某站 $s$ 满足以下任一条件时，触发调度：

1. 空闲率 $< 20\%$：$\frac{N_{\text{idle}}}{N_{\text{total}}} < 0.2$
2. 排队人数 $> 5$
3. 预测未来 1 小时负荷超过额定容量的 90%

### 8.2 调度动作集合

| 动作编号 | 动作 | 目标 | 生效范围 |
|----------|------|------|----------|
| A1 | 提升替代站推荐权重 | 附近 3km 内空闲率 > 50% 的站 | 用户端推荐列表 |
| A2 | 发放定向优惠券 | 向排队用户推送"前往 XX 站立减 ¥3" | 用户端消息中心 |
| A3 | 提高积分倍率 | 拥堵站充电积分 ×2 | 结算时生效 |
| A4 | 触发运营告警 | 管理端告警中心弹窗 | 管理端 |

### 8.3 动作选择策略

优先级顺序：A4（必触发）→ A1（必触发）→ A3（可选）→ A2（当 A1 效果不足时）

---

## 九、智能风控算法

### 9.1 风险用户识别规则

**规则 1：高频预约取消**

```math
\text{Risk}_1 = \mathbb{1}_{[N_{\text{reserve}}^{24h} \geq 10 \ \land \ r_{\text{cancel}} \geq 0.6]}
```

其中 $N_{\text{reserve}}^{24h}$ 为最近 24 小时预约次数，$r_{\text{cancel}}$ 为取消率。

**规则 2：异常充电行为**

```math
\text{Risk}_2 = \mathbb{1}_{[\text{单次充电时长} < 5\text{min} \ \land \ \text{次数} \geq 3 \text{/天}]}
```

（疑似刷单：频繁插拔枪但不充电）

### 9.2 风险评分

```math
\text{RiskScore} = \text{clip}\left(50 \cdot \text{Risk}_1 + 30 \cdot \text{Risk}_2 + 20 \cdot \text{Risk}_3,\ 0,\ 100\right)
```

| 评分 | 等级 | 处置建议 |
|------|------|----------|
| 0–40 | 低风险 | 无需处置 |
| 41–70 | 中风险 | 限制预约次数（每日上限 5 次） |
| 71–100 | 高风险 | 冻结预约功能 24 小时，同步管理端 |

---

## 十、what-if 决策仿真算法

### 10.1 输入参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `add_chargers` | INTEGER | 增配桩数量（可为负） |
| `price_delta` | REAL | 电价调整幅度（-0.5 ~ +0.5 元/kWh） |
| `failure_scale` | REAL | 故障桩比例（0.0 ~ 0.5） |
| `traffic_delta` | REAL | 客流变化比例（-0.3 ~ +0.3） |

### 10.2 推演模型

基于当前需求模型 $D_{d,h,a}$，调整参数后重新计算四项指标：

**平均等待时间**：

```math
\hat{W} = \frac{Q \cdot \bar{t}_{\text{remain}}}{N_{\text{total}} + \text{add\_chargers}} \cdot (1 + \text{failure\_scale}) \cdot (1 - 0.3 \cdot \text{price\_delta})
```

**峰值利用率**：

```math
\hat{U}_{\text{peak}} = \frac{D_{\text{peak}} \cdot (1 + \text{traffic\_delta})}{N_{\text{total}} + \text{add\_chargers}} \cdot \frac{1}{1 - \text{failure\_scale}}
```

**预计日订单量**：

```math
\hat{N}_{\text{order}} = \bar{N}_{\text{order}} \cdot (1 + \text{traffic\_delta}) \cdot (1 - 0.2 \cdot \text{price\_delta})
```

**预计日营收**：

```math
\hat{R} = \hat{N}_{\text{order}} \cdot (\bar{p} + \text{price\_delta}) \cdot \bar{e}
```

其中 $\bar{e}$ 为平均单次充电量（kWh）。

---

## 十一、评价标签分析算法

### 11.1 标签频次统计

对某站 $s$ 的评价标签集合 $\mathcal{T}_s$：

```math
\text{Freq}(t, s) = \sum_{r \in \mathcal{R}_s} \mathbb{1}_{[t \in r.\text{tags}]}, \quad \forall t \in \mathcal{T}
```

### 11.2 环比计算

对比本期（最近 30 天）与上期（再前 30 天）：

```math
\text{MoM}(t, s) = \frac{\text{Freq}_{\text{current}}(t, s) - \text{Freq}_{\text{previous}}(t, s)}{\text{Freq}_{\text{previous}}(t, s) + \epsilon}
```

### 11.3 TOP5 问题提取

按频次降序排列，取 Top 5：

```math
\text{TOP5}(s) = \underset{t \in \mathcal{T}}{\text{argtop5}}\ \text{Freq}(t, s)
```

### 11.4 分维度评分均值

对五个维度分别计算均值：

```math
\bar{S}_{\text{dim}} = \frac{1}{|\mathcal{R}_s|} \sum_{r \in \mathcal{R}_s} r.\text{dim\_score}, \quad \text{dim} \in \{\text{speed}, \text{device}, \text{parking}, \text{hygiene}, \text{service}\}
```

---

## 十二、AI 运营助手算法

### 12.1 设计思路

由于项目约束（无大模型 API），AI 运营助手采用**模板匹配 + 数据查询**的轻量级方案。

### 12.2 问答模板库

| 用户问题模式 | 数据查询 | 回答模板 |
|-------------|----------|----------|
| "今日营收多少" | `SELECT SUM(pay_amount) FROM charging_order WHERE date(create_time) = today` | "今日营收 ¥{amount}，较昨日 {trend}%" |
| "哪个站最忙" | `SELECT station_id, COUNT(*) FROM charging_order GROUP BY station_id ORDER BY COUNT DESC LIMIT 1` | "最繁忙的站点是 {station_name}，今日已处理 {count} 笔订单" |
| "有多少设备故障" | `SELECT COUNT(*) FROM charger WHERE status = 'fault'` | "当前共有 {count} 台设备处于故障状态，其中高风险 {high_risk} 台" |
| "用户增长情况" | `SELECT COUNT(*) FROM user WHERE date(register_time) = today` | "今日新增用户 {count} 人，累计注册用户 {total} 人" |
| "预测今晚负荷" | 调用负荷预测引擎 | "预测今晚 18:00-22:00 高峰负荷 {peak_load} kW，建议提前调配电力" |

### 12.3 匹配逻辑

采用关键词匹配 + 意图分类：

```cpp
QString answerQuery(const QString& question) {
    QString q = question.toLower();
    if (q.contains("营收") || q.contains("收入"))
        return queryRevenue();
    if (q.contains("最忙") || q.contains("繁忙"))
        return queryBusiestStation();
    if (q.contains("故障") || q.contains("坏"))
        return queryFaultCount();
    // ...
    return "抱歉，暂不支持该问题，请尝试询问：今日营收、设备故障、用户增长等";
}
```

---

## 十三、模型评估与兜底策略

### 13.1 评估指标

#### 13.1.1 负荷预测准确度

```math
\text{MAPE} = \frac{100\%}{T} \sum_{i=1}^{T} \left|\frac{\hat{P}_i - P_i}{P_i}\right|
```

**达标阈值**：MAPE ≤ 15%

#### 13.1.2 故障预测命中率

```math
\text{HitRate} = \frac{|\{\text{预测高风险且 24h 内实际故障}\}|}{|\{\text{预测高风险}\}|}
```

**达标阈值**：HitRate ≥ 30%（基于规则模型的合理预期）

#### 13.1.3 推荐排序相关性

采用 Kendall's Tau 系数衡量推荐排序与用户选择的匹配度：

```math
\tau = \frac{\text{一致对数} - \text{不一致对数}}{\binom{n}{2}}
```

### 13.2 兜底机制

当算法因数据不足、计算异常或质量不达标时，自动回退到默认规则：

| 算法 | 正常策略 | 兜底策略 |
|------|----------|----------|
| 负荷预测 | PME 周期性外推 | 固定日周期模板（早 8 晚 18 双峰） |
| 智能推荐 | 多特征加权排序 | 距离最近 + 空闲最多 |
| 避峰推荐 | 基于预测的低峰推荐 | 固定分时电价提示（峰时 10:00-15:00, 18:00-22:00） |
| 故障预测 | 健康度规则评分 | 仅按温度 > 70℃ 标红 |
| 等待时间 | 排队 × 平均时长 | 固定按每队 15 分钟估算 |

### 13.3 数据质量检查

每次算法运行前检查数据可用性：

```cpp
bool validateData(int stationId) {
    int historyDays = db->countHistoryDays(stationId);
    if (historyDays < 3) {
        log.warning("历史数据不足 %d 天，启用兜底策略", historyDays);
        return false;
    }
    int missingRate = db->calculateMissingRate(stationId, 7);
    if (missingRate > 0.3) {
        log.warning("缺失率 %.1f%%，启用兜底策略", missingRate * 100);
        return false;
    }
    return true;
}
```

---

## 附录 A：参数速查表

| 参数名 | 建议初值 | 可调范围 | 影响 |
|--------|----------|----------|------|
| 负荷预测历史天数 $D$ | 7 | 3–14 | 周期性匹配准确度 |
| 移动平均窗口 $k$ | 1 | 0–3 | 平滑程度 |
| 增长系数 $g$ | 0.02 | 0–0.1 | 预测趋势斜率 |
| 乘性噪声 $\sigma_\epsilon$ | 0.05 | 0.01–0.1 | 预测曲线波动 |
| 温度惩罚阈值 | 50℃ | 40–60℃ | 健康度敏感度 |
| 健康度告警阈值 | 60 | 50–70 | 故障预警触发 |
| 推荐权重 $w_d/w_p/w_w/w_h$ | 0.3/0.3/0.2/0.2 | 总和=1 | 排序偏好 |
| 风控预约次数阈值 | 10 | 5–20 | 风控敏感度 |
| 风控取消率阈值 | 0.6 | 0.5–0.8 | 风控严格度 |
| 调度空闲率阈值 | 0.2 | 0.15–0.3 | 调度触发 |

## 附录 B：数据库索引建议

为确保 ML 查询性能，建议对以下字段建立索引：

```sql
-- 负荷预测查询加速
CREATE INDEX idx_measure_station_time 
ON charging_measure(station_id, measure_time);

-- 健康度评估查询加速
CREATE INDEX idx_measure_charger_time 
ON charging_measure(charger_id, measure_time);

-- 需求预测查询加速
CREATE INDEX idx_order_station_start 
ON charging_order(station_id, start_time);

-- 风控查询加速
CREATE INDEX idx_reservation_user_time 
ON reservation(user_id, reserve_time);

-- 评价分析查询加速
CREATE INDEX idx_review_station_time 
ON review(station_id, create_time);
```
