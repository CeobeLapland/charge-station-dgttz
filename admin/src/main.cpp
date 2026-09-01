#include <QApplication>
#include <QFile>
#include <QIODevice>

#include "services/ApiClient.h"
#include "ui/LoginDialog.h"
#include "ui/MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // 深色主题
    QFile qssFile(QStringLiteral(":/theme.qss"));
    if (qssFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(QString::fromUtf8(qssFile.readAll()));
    }

    ApiClient api;
    api.start();  // 尝试连接服务端进程；未连接时自动进入 Mock 模式

    LoginDialog login(&api);
    if (login.exec() != QDialog::Accepted) {
        return 0;
    }

    MainWindow window(&api, login.account());
    QObject::connect(&window, &MainWindow::logoutRequested, &app, &QApplication::quit);
    window.show();
    return app.exec();
}
