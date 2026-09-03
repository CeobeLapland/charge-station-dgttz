#pragma once
#include <QWidget>

class QLineEdit;
class QTableWidget;
class ApiClient;

class UserManagePage : public QWidget {
    Q_OBJECT
public:
    explicit UserManagePage(ApiClient* api, QWidget* parent = nullptr);

private slots:
    void refresh();
    void onToggleStatus();

private:
    ApiClient* m_api = nullptr;
    QTableWidget* m_table = nullptr;
    QLineEdit* m_searchEdit = nullptr;
};
