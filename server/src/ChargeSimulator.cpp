#include "ChargeSimulator.h"

#include <QDebug>
#include <QTimer>
#include <QtGlobal>
#include <cmath>

ChargeSimulator::ChargeSimulator(QObject *parent) : QObject(parent) {}

void ChargeSimulator::startTicking()
{
    // 注意: QTimer 在这里 new, 这个函数是通过队列连接在工作线程里执行的,
    // 所以定时器天然属于工作线程, 不会占用主线程。
    m_timer = new QTimer(this);
    m_timer->setInterval(kTickMs);
    connect(m_timer, &QTimer::timeout, this, &ChargeSimulator::onTick);
    m_timer->start();
    qInfo().noquote() << QStringLiteral("[仿真] 充电仿真线程已启动 (%1 秒 = %2 分钟)")
                             .arg(kTickMs / 1000.0).arg(kSimMinutesPerTick);
}

void ChargeSimulator::addOrder(int orderId, int userId, int chargerId, int stationId,
                               double startSoc, double targetSoc,
                               double powerKw, double batteryKwh, double unitPrice)
{
    Sim s;
    s.orderId    = orderId;
    s.userId     = userId;
    s.chargerId  = chargerId;
    s.stationId  = stationId;
    s.soc        = qBound(0.0, startSoc, 100.0);
    s.targetSoc  = (targetSoc > s.soc && targetSoc <= 100) ? targetSoc : 100.0;
    s.powerKw    = powerKw > 0 ? powerKw : 60.0;
    s.batteryKwh = batteryKwh > 0 ? batteryKwh : 60.0;
    s.unitPrice  = unitPrice;
    m_sims.insert(orderId, s);
    qInfo().noquote() << QStringLiteral("[仿真] 订单 #%1 开始 (SOC %2%% → %3%%, %4kW)")
                             .arg(orderId).arg(s.soc, 0, 'f', 1)
                             .arg(s.targetSoc, 0, 'f', 0).arg(s.powerKw, 0, 'f', 0);
}

void ChargeSimulator::removeOrder(int orderId)
{
    if (m_sims.remove(orderId) > 0)
        qInfo().noquote() << QStringLiteral("[仿真] 订单 #%1 停止").arg(orderId);
}

void ChargeSimulator::onTick()
{
    if (m_sims.isEmpty()) return;

    QList<int> done;
    for (auto it = m_sims.begin(); it != m_sims.end(); ++it) {
        Sim &s = it.value();
        ++s.ticks;

        // 这一 tick 充进去多少电: 功率 × 时间 × 效率
        double kwh = s.powerKw * (kSimMinutesPerTick / 60.0) * 0.92;
        // 涓流充电: 电量越高越慢, 80% 以上功率减半, 95% 以上再减半
        if (s.soc >= 95)      kwh *= 0.25;
        else if (s.soc >= 80) kwh *= 0.50;

        const double deltaSoc = kwh / s.batteryKwh * 100.0;
        s.soc       = qMin(s.targetSoc, s.soc + deltaSoc);
        s.energyKwh += kwh;

        const double cost = std::round(s.energyKwh * s.unitPrice * 100) / 100.0;

        // 剩余时间估算(按当前档位的充电速度)
        const double remainKwh = qMax(0.0, (s.targetSoc - s.soc) / 100.0 * s.batteryKwh);
        const double kwhPerMin = qMax(0.001, kwh / kSimMinutesPerTick);
        const int    etaMin    = static_cast<int>(std::ceil(remainKwh / kwhPerMin));

        emit progress(s.orderId, s.userId, s.chargerId, s.stationId,
                      std::round(s.soc * 10) / 10.0,
                      s.soc >= 95 ? s.powerKw * 0.25 : (s.soc >= 80 ? s.powerKw * 0.5 : s.powerKw),
                      std::round(s.energyKwh * 100) / 100.0, cost, etaMin);

        // 每 kMeasureEveryTicks 个 tick 落一条时序点, 喂大屏的负荷曲线
        if (s.ticks % kMeasureEveryTicks == 0)
            emit measureTick(s.chargerId, s.stationId, s.powerKw,
                             std::round(s.soc * 10) / 10.0, std::round(kwh * 1000) / 1000.0);

        if (s.soc >= s.targetSoc - 0.01)
            done.append(s.orderId);
    }

    // 充满的订单: 通知主线程自动结束(主线程去写库)
    for (int oid : done) {
        const Sim s = m_sims.value(oid);
        m_sims.remove(oid);
        qInfo().noquote() << QStringLiteral("[仿真] 订单 #%1 已充至目标 %2%%")
                                 .arg(oid).arg(s.soc, 0, 'f', 1);
        emit reachedTarget(oid, s.userId, std::round(s.soc * 10) / 10.0);
    }
}
