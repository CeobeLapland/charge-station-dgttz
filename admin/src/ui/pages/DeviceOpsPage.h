#pragma once
#include <QWidget>

class QTableWidget;
class ApiClient;

class DeviceOpsPage : public QWidget {
    Q_OBJECT
public:
    explicit DeviceOpsPage(ApiClient* api, QWidget* parent = nullptr);

private:
    void refresh();
    ApiClient* m_api = nullptr;
    QTableWidget* m_table = nullptr;
};
