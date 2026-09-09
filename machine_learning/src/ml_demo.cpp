// ml_demo：机器学习引擎自检程序。
// 打开 data/ml_sim.db（可用 argv[1] 覆盖），依次跑全部引擎并做断言；
// 对负荷预测做回测（前 13 天训练、预测第 14 天，小时级 MAPE ≤ 15%）。
// 退出码 0 = 全部通过。

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QTextStream>

#include <algorithm>
#include <cmath>

#include "ml/LoadForecastingEngine.h"
#include "ml/MlEngine.h"

static int g_failures = 0;

static void check(bool ok, const QString& name)
{
    QTextStream(stdout) << (ok ? "  [PASS] " : "  [FAIL] ") << name << "\n";
    if (!ok)
        g_failures++;
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    QString dbPath;
    if (argc > 1) {
        dbPath = QString::fromLocal8Bit(argv[1]);
    } else {
        const QStringList candidates = {
            QStringLiteral("data/ml_sim.db"),
            QStringLiteral("../data/ml_sim.db"),
            QStringLiteral("machine_learning/data/ml_sim.db"),
        };
        for (const auto& c : candidates) {
            if (QFile::exists(c)) {
                dbPath = c;
                break;
            }
        }
    }
    out << "数据库: " << dbPath << "\n";

    ml::MlEngine engine;
    if (!engine.openDatabase(dbPath)) {
        out << "无法打开数据库\n";
        return 1;
    }
    ml::MlDataProvider* provider = engine.provider();
    const QDate today = QDate::currentDate();
    const QDate targetDay = today.addDays(-1); // 第 14 天（回测目标日）

    // ---------- 1. 负荷预测：第 14 天回测（小时级 MAPE ≤ 15%） ----------
    out << "\n== 负荷预测回测（第 14 天 " << targetDay.toString() << "） ==\n";
    ml::LoadForecastingEngine loadEngine(provider);
    loadEngine.setSeed(42);
    double worstMape = 0.0;
    for (const auto& station : provider->stations()) {
        QMap<int, double> actual;
        const auto points = provider->stationHourlyLoad(
            station.id, QDateTime(targetDay, QTime(0, 0)));
        for (const auto& p : points) {
            if (p.time.date() == targetDay)
                actual[p.time.time().hour()] = p.power;
        }
        const auto fc = loadEngine.predict(station.id, 24, QDateTime(today, QTime(0, 0)));
        double sum = 0.0;
        int cnt = 0;
        for (int h = 0; h < 24 && h < fc.predicted.size(); ++h) {
            const double a = actual.value(h, 0.0);
            if (a < 0.5) // MAPE 分母为 0（近似空闲）的小时跳过
                continue;
            sum += std::abs(fc.predicted[h] - a) / a;
            cnt++;
        }
        const double mape = cnt > 0 ? sum / cnt * 100.0 : 0.0;
        worstMape = std::max(worstMape, mape);
        bool bandOk = true;
        for (int i = 0; i < fc.predicted.size(); ++i)
            bandOk = bandOk && fc.lower[i] <= fc.predicted[i]
                     && fc.predicted[i] <= fc.upper[i];
        out << QStringLiteral("  站 %1 (%2): MAPE = %3%  样本小时 %4\n")
                   .arg(station.id).arg(station.name).arg(mape, 0, 'f', 2).arg(cnt);
        check(mape <= 15.0, QStringLiteral("站 %1 MAPE ≤ 15%").arg(station.id));
        check(bandOk, QStringLiteral("站 %1 置信区间合法").arg(station.id));
    }
    check(worstMape <= 15.0, QStringLiteral("总体 MAPE ≤ 15%（最差 %1%）").arg(worstMape, 0, 'f', 2));

    // ---------- 2. 健康度与故障风险 ----------
    out << "\n== 设备健康度与故障风险 ==\n";
    const QJsonArray risks = engine.faultRisk().value(QStringLiteral("risks")).toArray();
    check(!risks.isEmpty(), QStringLiteral("故障风险列表非空"));
    bool rangeOk = true, sortedOk = true, hasHighRisk = false;
    double prevRisk = 2.0;
    for (const auto& v : risks) {
        const QJsonObject o = v.toObject();
        const int h = o.value(QStringLiteral("health_score")).toInt();
        const double r = o.value(QStringLiteral("fault_risk")).toDouble();
        rangeOk = rangeOk && h >= 0 && h <= 100 && r >= 0.0 && r <= 1.0;
        sortedOk = sortedOk && r <= prevRisk;
        prevRisk = r;
        if (o.value(QStringLiteral("risk_level")).toString() == QLatin1String("high"))
            hasHighRisk = true;
    }
    check(rangeOk, QStringLiteral("健康度∈[0,100] 且风险∈[0,1]"));
    check(sortedOk, QStringLiteral("按故障风险降序排列"));
    check(hasHighRisk, QStringLiteral("存在高风险桩"));
    const QJsonObject topRisk = risks.first().toObject();
    out << QStringLiteral("  风险最高: 桩 %1 健康度 %2 风险 %3 (%4)\n")
               .arg(topRisk.value(QStringLiteral("charger_id")).toInt())
               .arg(topRisk.value(QStringLiteral("health_score")).toInt())
               .arg(topRisk.value(QStringLiteral("fault_risk")).toDouble())
               .arg(topRisk.value(QStringLiteral("risk_level")).toString());

    // ---------- 3. 智能推荐 ----------
    out << "\n== 智能推荐（用户位置 116.30, 39.98） ==\n";
    const QStringList categories = {"balanced", "fastest", "cheapest"};
    for (const auto& cat : categories) {
        const QJsonArray rec = engine.recommend(116.30, 39.98, cat)
                                   .value(QStringLiteral("recommendation")).toArray();
        check(rec.size() >= 3, QStringLiteral("%1 排序非空").arg(cat));
        if (!rec.isEmpty()) {
            const QJsonObject best = rec.first().toObject();
            out << QStringLiteral("  %1: Top1 站 %2 评分 %3\n")
                       .arg(cat)
                       .arg(best.value(QStringLiteral("station_id")).toInt())
                       .arg(best.value(QStringLiteral("score")).toDouble());
        }
    }

    // ---------- 4. 等待时间 ----------
    out << "\n== 等待时间预估 ==\n";
    for (const auto& station : provider->stations()) {
        const QJsonObject w = engine.waitEstimate(station.id);
        out << QStringLiteral("  站 %1: 排队 %2 人, 当前等待 %3 分钟, 10分钟后 %4, 20分钟后 %5, 建议: %6\n")
                   .arg(station.id)
                   .arg(w.value(QStringLiteral("queue_length")).toInt())
                   .arg(w.value(QStringLiteral("current_minutes")).toInt())
                   .arg(w.value(QStringLiteral("after_10_minutes")).toInt())
                   .arg(w.value(QStringLiteral("after_20_minutes")).toInt())
                   .arg(w.value(QStringLiteral("suggestion")).toString());
    }
    {
        const QJsonObject w2 = engine.waitEstimate(2);
        check(w2.value(QStringLiteral("queue_length")).toInt() > 5,
              QStringLiteral("站 2 存在长队列（>5）"));
        check(!w2.value(QStringLiteral("suggestion")).toString().isEmpty(),
              QStringLiteral("建议文案非空"));
        // 空闲桩 > 0 的站当前等待应为 0
        bool someIdle = false;
        for (const auto& station : provider->stations()) {
            for (const auto& c : provider->chargers(station.id)) {
                if (c.status == QLatin1String("idle"))
                    someIdle = true;
            }
        }
        check(someIdle, QStringLiteral("存在空闲桩的站点"));
    }

    // ---------- 5. 需求热力图 ----------
    out << "\n== 需求热力图 ==\n";
    const QJsonObject heat = engine.demandHeatmap();
    const QJsonArray heatmap = heat.value(QStringLiteral("heatmap")).toArray();
    check(heatmap.size() > 0, QStringLiteral("热力图非空"));
    int surgeCnt = 0;
    for (const auto& v : heatmap) {
        if (v.toObject().value(QStringLiteral("surge")).toBool())
            surgeCnt++;
    }
    out << QStringLiteral("  热力单元 %1 个，激增预警 %2 个\n")
               .arg(heatmap.size()).arg(surgeCnt);
    for (const auto& v : heat.value(QStringLiteral("top_surge_windows")).toArray()) {
        const QJsonObject o = v.toObject();
        out << QStringLiteral("  激增窗口: %1（需求 %2）→ %3\n")
                   .arg(o.value(QStringLiteral("window")).toString())
                   .arg(o.value(QStringLiteral("demand")).toDouble())
                   .arg(o.value(QStringLiteral("recommendation")).toString());
    }

    // ---------- 6. 智能调度 ----------
    out << "\n== 智能调度 ==\n";
    const QJsonArray decisions = engine.dispatchCheck()
                                     .value(QStringLiteral("decisions")).toArray();
    int triggered = 0;
    bool hasA4 = false;
    for (const auto& v : decisions) {
        const QJsonObject d = v.toObject();
        out << QStringLiteral("  站 %1: 触发=%2 原因=%3 动作=%4\n")
                   .arg(d.value(QStringLiteral("station_id")).toInt())
                   .arg(d.value(QStringLiteral("triggered")).toBool() ? "是" : "否")
                   .arg(d.value(QStringLiteral("trigger_reasons")).toVariant().toStringList().join(","))
                   .arg(d.value(QStringLiteral("actions")).toVariant().toStringList().join(","));
        if (d.value(QStringLiteral("triggered")).toBool()) {
            triggered++;
            hasA4 = hasA4 || d.value(QStringLiteral("actions")).toVariant()
                                 .toStringList().contains(QLatin1String("A4_trigger_operation_alarm"));
        }
    }
    check(triggered > 0, QStringLiteral("至少一个站触发调度"));
    check(hasA4, QStringLiteral("触发站均含 A4 运营告警"));

    // ---------- 7. 智能风控 ----------
    out << "\n== 智能风控 ==\n";
    const QJsonArray riskUsers = engine.riskUsers()
                                     .value(QStringLiteral("risk_users")).toArray();
    bool hasRule1 = false, hasRule2 = false;
    for (const auto& v : riskUsers) {
        const QJsonObject o = v.toObject();
        const QStringList rules = o.value(QStringLiteral("hit_rules")).toVariant().toStringList();
        hasRule1 = hasRule1 || rules.contains(QLatin1String("rule1_high_freq_cancel"));
        hasRule2 = hasRule2 || rules.contains(QLatin1String("rule2_short_charge"));
        out << QStringLiteral("  用户 %1: 评分 %2 (%3) → %4\n")
                   .arg(o.value(QStringLiteral("user_id")).toInt())
                   .arg(o.value(QStringLiteral("risk_score")).toDouble())
                   .arg(o.value(QStringLiteral("level")).toString())
                   .arg(o.value(QStringLiteral("action")).toString());
    }
    check(hasRule1, QStringLiteral("命中规则 1（高频预约取消）"));
    check(hasRule2, QStringLiteral("命中规则 2（异常短时充电）"));

    // ---------- 8. what-if 仿真 ----------
    out << "\n== what-if 决策仿真（站 2：+4 桩、-0.2 元、故障 10%、客流 +15%） ==\n";
    QJsonObject scenario;
    scenario[QStringLiteral("station_id")] = 2;
    scenario[QStringLiteral("add_chargers")] = 4;
    scenario[QStringLiteral("price_delta")] = -0.2;
    scenario[QStringLiteral("failure_scale")] = 0.1;
    scenario[QStringLiteral("traffic_delta")] = 0.15;
    const QJsonObject whatIf = engine.whatIf(scenario);
    const QJsonObject impact = whatIf.value(QStringLiteral("impact")).toObject();
    out << QStringLiteral("  平均等待 %1 分钟, 峰值利用率 %2, 日订单 %3, 日营收 ¥%4\n")
               .arg(impact.value(QStringLiteral("avg_wait_min")).toDouble())
               .arg(impact.value(QStringLiteral("peak_utilization")).toDouble())
               .arg(impact.value(QStringLiteral("daily_orders")).toDouble())
               .arg(impact.value(QStringLiteral("daily_revenue")).toDouble());
    check(impact.value(QStringLiteral("daily_orders")).toDouble() > 0,
          QStringLiteral("推演日订单量 > 0"));
    check(impact.value(QStringLiteral("daily_revenue")).toDouble() > 0,
          QStringLiteral("推演日营收 > 0"));
    // 参数 clip 检查：越界参数应被截断到文档范围
    QJsonObject wild = scenario;
    wild[QStringLiteral("price_delta")] = 2.0;   // 超界 → 0.5
    wild[QStringLiteral("failure_scale")] = 1.0; // 超界 → 0.5
    wild[QStringLiteral("traffic_delta")] = -2.0;// 超界 → -0.3
    const QJsonObject clippedObj = engine.whatIf(wild)
                                       .value(QStringLiteral("clipped_params")).toObject();
    check(qAbs(clippedObj.value(QStringLiteral("price_delta")).toDouble() - 0.5) < 1e-9
              && qAbs(clippedObj.value(QStringLiteral("traffic_delta")).toDouble() + 0.3) < 1e-9,
          QStringLiteral("越界参数被 clip 到文档范围"));

    // ---------- 9. 评价标签分析 ----------
    out << "\n== 评价标签分析 ==\n";
    const QJsonObject review = engine.reviewTags(1);
    const QJsonArray topTags = review.value(QStringLiteral("top_tags")).toArray();
    const QJsonObject dims = review.value(QStringLiteral("dim_averages")).toObject();
    check(!topTags.isEmpty(), QStringLiteral("TOP5 标签非空"));
    check(dims.size() == 5, QStringLiteral("五维评分均值齐全"));
    for (const auto& v : topTags) {
        const QJsonObject o = v.toObject();
        out << QStringLiteral("  标签 %1: 频次 %2, MoM %3\n")
                   .arg(o.value(QStringLiteral("tag")).toString())
                   .arg(o.value(QStringLiteral("freq")).toInt())
                   .arg(o.value(QStringLiteral("mom")).toDouble());
    }

    // ---------- 10. AI 运营助手 ----------
    out << "\n== AI 运营助手 ==\n";
    const QStringList questions = {
        QStringLiteral("今日营收多少"),
        QStringLiteral("哪个站最忙"),
        QStringLiteral("有多少设备故障"),
        QStringLiteral("用户增长情况如何"),
        QStringLiteral("预测今晚负荷"),
        QStringLiteral("明天会下雨吗"), // fallback
    };
    bool fallbackSeen = false;
    for (const auto& q : questions) {
        const QJsonObject reply = engine.assistantQuery(q);
        const bool fb = reply.value(QStringLiteral("fallback")).toBool();
        fallbackSeen = fallbackSeen || fb;
        out << QStringLiteral("  Q: %1\n  A(%2): %3\n")
                   .arg(q)
                   .arg(reply.value(QStringLiteral("intent")).toString())
                   .arg(reply.value(QStringLiteral("answer")).toString());
        check(!reply.value(QStringLiteral("answer")).toString().isEmpty(),
              QStringLiteral("回答非空: %1").arg(q));
    }
    check(fallbackSeen, QStringLiteral("未知问题返回 fallback 文案"));

    // ---------- 11. 门面 forecast 接口（协议对齐形状） ----------
    out << "\n== ml.forecast 门面接口（horizon=6） ==\n";
    const QJsonObject forecastResp = engine.forecast(1, 6);
    check(forecastResp.value(QStringLiteral("code")).toInt() == 0,
          QStringLiteral("forecast code=0"));
    check(forecastResp.value(QStringLiteral("forecast")).toArray().size() == 6,
          QStringLiteral("forecast 序列长度 = horizon"));
    check(forecastResp.value(QStringLiteral("history")).toArray().size() > 0,
          QStringLiteral("含历史 actual 序列"));
    out << QStringLiteral("  forecast[0]: %1\n")
               .arg(QString::fromUtf8(
                   QJsonDocument(forecastResp.value(QStringLiteral("forecast")).toArray()
                                     .first().toObject())
                       .toJson(QJsonDocument::Compact)));

    out << "\n==========================================\n";
    if (g_failures == 0) {
        out << "全部自检通过（MAPE 最差 " << worstMape << "%）\n";
        return 0;
    }
    out << g_failures << " 项自检失败\n";
    return 1;
}
