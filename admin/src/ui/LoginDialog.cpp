#include "ui/LoginDialog.h"

#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "network/Protocol.h"
#include "services/ApiClient.h"

LoginDialog::LoginDialog(ApiClient* api, QWidget* parent)
    : QDialog(parent), m_api(api) {
    setWindowTitle(QStringLiteral("服务器管理端 · 登录"));
    setFixedWidth(360);

    auto* title = new QLabel(QStringLiteral("电动汽车充电桩应用管理平台"));
    title->setObjectName(QStringLiteral("pageTitle"));
    title->setAlignment(Qt::AlignCenter);

    m_accountEdit = new QLineEdit;
    m_accountEdit->setPlaceholderText(QStringLiteral("账号（默认 admin）"));
    m_accountEdit->setText(QStringLiteral("admin"));

    m_passwordEdit = new QLineEdit;
    m_passwordEdit->setPlaceholderText(QStringLiteral("密码（默认 123456）"));
    m_passwordEdit->setText(QStringLiteral("123456"));
    m_passwordEdit->setEchoMode(QLineEdit::Password);

    m_errorLabel = new QLabel;
    m_errorLabel->setStyleSheet(QStringLiteral("color: #ff7b72;"));
    m_errorLabel->setWordWrap(true);
    m_errorLabel->hide();

    m_loginButton = new QPushButton(QStringLiteral("登 录"));
    m_loginButton->setObjectName(QStringLiteral("primaryButton"));
    m_loginButton->setDefault(true);

    connect(m_loginButton, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &LoginDialog::onLoginClicked);

    auto* form = new QVBoxLayout(this);
    form->addWidget(title);
    form->addSpacing(16);
    form->addWidget(new QLabel(QStringLiteral("账号")));
    form->addWidget(m_accountEdit);
    form->addWidget(new QLabel(QStringLiteral("密码")));
    form->addWidget(m_passwordEdit);
    form->addWidget(m_errorLabel);
    form->addSpacing(8);
    form->addWidget(m_loginButton);
}

void LoginDialog::onLoginClicked() {
    const QString account = m_accountEdit->text().trimmed();
    const QString password = m_passwordEdit->text();
    if (account.isEmpty() || password.isEmpty()) {
        m_errorLabel->setText(QStringLiteral("请输入账号和密码"));
        m_errorLabel->show();
        return;
    }
    m_loginButton->setEnabled(false);
    m_errorLabel->hide();

    m_api->login(account, password,
                 [this, account](int code, const QString& message, const QJsonObject&) {
        m_loginButton->setEnabled(true);
        if (code == proto::code::Ok) {
            m_account = account;
            accept();
        } else {
            m_errorLabel->setText(message.isEmpty()
                                      ? QStringLiteral("登录失败，请检查账号密码")
                                      : QStringLiteral("登录失败：%1").arg(message));
            m_errorLabel->show();
        }
    });
}
