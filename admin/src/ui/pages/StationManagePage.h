#pragma once
#include <QWidget>

class QTableWidget;
class ApiClient;

class StationManagePage : public QWidget {
    Q_OBJECT
public:
    explicit StationManagePage(ApiClient* api, QWidget* parent = nullptr);

private slots:
    void refresh();
    void onAddStation();
    void onShowDetail();

private:
    ApiClient* m_api = nullptr;
    QTableWidget* m_table = nullptr;
};
