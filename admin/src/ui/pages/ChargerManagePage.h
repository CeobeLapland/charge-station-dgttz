#pragma once
#include <QWidget>

class QTableWidget;
class QTextEdit;
class ApiClient;

class ChargerManagePage : public QWidget {
    Q_OBJECT
public:
    explicit ChargerManagePage(ApiClient* api, QWidget* parent = nullptr);

private slots:
    void refresh();
    void onRestart();

private:
    ApiClient* m_api = nullptr;
    QTableWidget* m_table = nullptr;
    QTextEdit* m_logView = nullptr;
};
