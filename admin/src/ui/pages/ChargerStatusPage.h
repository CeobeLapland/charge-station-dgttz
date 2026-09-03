#pragma once
#include <QWidget>

class QTableWidget;
class ApiClient;

class ChargerStatusPage : public QWidget {
    Q_OBJECT
public:
    explicit ChargerStatusPage(ApiClient* api, QWidget* parent = nullptr);

private:
    void refresh();
    ApiClient* m_api = nullptr;
    QTableWidget* m_table = nullptr;
};
