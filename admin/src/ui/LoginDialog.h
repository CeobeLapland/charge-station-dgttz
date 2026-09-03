#pragma once
#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;
class ApiClient;

class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(ApiClient* api, QWidget* parent = nullptr);
    QString account() const { return m_account; }

private slots:
    void onLoginClicked();

private:
    ApiClient* m_api = nullptr;
    QString m_account;
    QLineEdit* m_accountEdit = nullptr;
    QLineEdit* m_passwordEdit = nullptr;
    QLabel* m_errorLabel = nullptr;
    QPushButton* m_loginButton = nullptr;
};
