#pragma once
#include <QHash>
#include <QObject>

class QTimer;

// ============================================================
// ChargeSimulator — 充电过程仿真器(运行在独立工作线程)
//
// 为什么要它: 演示时不可能真等一小时把车充满。仿真器每 1 秒推进
// kSimMinutesPerTick 分钟的充电过程, 把 SOC / 电量 / 费用算出来,
// 通过信号发给主线程, 由主线程写库并推送给用户端(push.order_progress)。
//
// ★ 线程模型(答辩要点):
//   工作线程  ChargeSimulator: 只做纯计算, 一行 SQL 都不碰
//   主线程    WsServer       : 收到信号后写数据库、发 WebSocket
//
//   为什么这么切? 因为 SQLite 的连接不能跨线程共享 —— QSqlDatabase 的
//   连接绑定在创建它的线程上, 别的线程直接用会出未定义行为。所以数据库
//   访问全部留在主线程串行执行, 工作线程只算数, 两者用 Qt 的跨线程信号槽
//   (队列连接)通信, 数据被自动拷贝, 不需要任何互斥锁。
//
//   这不是偷懒, 是有意的设计: 把"共享可变状态"限制在一个线程里,
//   比到处加锁更简单也更不容易错。
// ============================================================
class ChargeSimulator : public QObject
{
    Q_OBJECT
public:
    // 每个 tick(1 秒真实时间) 推进多少分钟的充电过程 —— 相当于 60 倍速
    static constexpr double kSimMinutesPerTick = 1.0;
    static constexpr int    kTickMs            = 1000;
    // 每多少个 tick 往 charging_measure 落一条时序点(供大屏负荷曲线)
    static constexpr int    kMeasureEveryTicks = 15;

    explicit ChargeSimulator(QObject *parent = nullptr);

public slots:
    // 在工作线程里创建并启动定时器(必须在线程内调用, 不能在主线程 new QTimer)
    void startTicking();

    // 一笔订单开始充电
    void addOrder(int orderId, int userId, int chargerId, int stationId,
                  double startSoc, double targetSoc,
                  double powerKw, double batteryKwh, double unitPrice);

    // 订单结束/取消, 停止仿真
    void removeOrder(int orderId);

signals:
    // 每个 tick 发一次: 当前进度
    void progress(int orderId, int userId, int chargerId, int stationId,
                  double soc, double powerKw, double energyKwh, double cost, int etaMin);

    // 需要往 charging_measure 落一条时序点
    void measureTick(int chargerId, int stationId, double powerKw, double soc, double energyDelta);

    // 充到目标电量, 自动结束充电
    void reachedTarget(int orderId, int userId, double endSoc);

private slots:
    void onTick();

private:
    struct Sim {
        int    orderId = 0, userId = 0, chargerId = 0, stationId = 0;
        double soc = 0, targetSoc = 100;
        double powerKw = 0, batteryKwh = 60, unitPrice = 0;
        double energyKwh = 0;      // 累计充入电量
        int    ticks = 0;
    };

    QTimer            *m_timer = nullptr;
    QHash<int, Sim>    m_sims;     // orderId → 仿真状态
};
