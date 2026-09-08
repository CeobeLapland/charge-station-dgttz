#pragma once
#include <QWidget>
#include <QVector>

class QTableWidget;
class QLabel;
class QPushButton;
class ApiClient;

struct AlarmRow {
    QString level;      // 高 / 中
    QString type;       // 设备故障 / 设备离线 / 站在线率低
    QString object;     // 站名 / 桩编号
    QString detail;
    int stationId = 0;
    int chargerId = 0;
};

// 运营告警中心（推导式）：从现有接口计算站/桩异常，处理走真实接口。
// 待 server 提供 admin.alarm_list 后可替换为读真 alarm 表。
class AlarmCenterPage : public QWidget {
    Q_OBJECT
public:
    explicit AlarmCenterPage(ApiClient* api, QWidget* parent = nullptr);

private slots:
    void refresh();

private:
    void renderAlarms();
    void onHandle();

    ApiClient* m_api = nullptr;
    QTableWidget* m_table = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QVector<AlarmRow> m_alarms;
};