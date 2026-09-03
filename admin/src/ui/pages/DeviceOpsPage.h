#pragma once
#include <QWidget>

class QTableWidget;
class QTextEdit;
class ApiClient;

// 设备运维【基础】：设备实时参数（数字孪生）+ 故障处置（远程重启 / 操作日志）+ 故障风险 TOP。
class DeviceOpsPage : public QWidget {
    Q_OBJECT
public:
    explicit DeviceOpsPage(ApiClient* api, QWidget* parent = nullptr);

private slots:
    void refresh();
    void onRestart();
    void onShowLogs();

private:
    void appendLog(const QString& text);

    ApiClient* m_api = nullptr;
    QTableWidget* m_table = nullptr;      // 实时参数表
    QTableWidget* m_riskTable = nullptr;  // 故障风险排序（健康度最低 TOP5）
    QTextEdit* m_logView = nullptr;
};