#ifndef THEME_H
#define THEME_H

#include <QObject>
#include <QColor>
#include <QVariantMap>

// 全局主题：统一色调 / 字体 / 圆角 / 间距。
// 作为纯 C++ 对象注册为 UserClient 模块的 singleton "Theme"，页面 import UserClient 后直接访问 Theme.xxx。
// 采用 C++ 对象而非 context property / QML 单例：Qt 6.2 的 qmlcache（AOT）在属性绑定里
// 读运行时注入的 context property 或 QML 组件实例可能会得到 null/undefined，导致白屏；
// C++ 单例走编译期类型路径，AOT 安全、无 QQmlContext 冲突。
class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString styleName READ styleName WRITE setStyleName NOTIFY styleNameChanged)

    // —— 颜色 ——
    Q_PROPERTY(QColor primary READ primary NOTIFY styleNameChanged)
    Q_PROPERTY(QColor primaryLight READ primaryLight NOTIFY styleNameChanged)
    Q_PROPERTY(QColor primaryDark READ primaryDark NOTIFY styleNameChanged)
    Q_PROPERTY(QColor accent READ accent NOTIFY styleNameChanged)
    Q_PROPERTY(QColor background READ background NOTIFY styleNameChanged)
    Q_PROPERTY(QColor card READ card NOTIFY styleNameChanged)
    Q_PROPERTY(QColor border READ border NOTIFY styleNameChanged)
    Q_PROPERTY(QColor textPrimary READ textPrimary NOTIFY styleNameChanged)
    Q_PROPERTY(QColor textSecondary READ textSecondary NOTIFY styleNameChanged)
    Q_PROPERTY(QColor success READ success NOTIFY styleNameChanged)
    Q_PROPERTY(QColor warn READ warn NOTIFY styleNameChanged)
    Q_PROPERTY(QColor danger READ danger NOTIFY styleNameChanged)

    // —— 圆角 / 字体 ——
    Q_PROPERTY(int radius READ radius NOTIFY styleNameChanged)
    Q_PROPERTY(int radiusSmall READ radiusSmall NOTIFY styleNameChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily CONSTANT)

    // —— 字号 ——
    Q_PROPERTY(int fontSizeTiny READ fontSizeTiny CONSTANT)
    Q_PROPERTY(int fontSizeSmall READ fontSizeSmall CONSTANT)
    Q_PROPERTY(int fontSizeBase READ fontSizeBase CONSTANT)
    Q_PROPERTY(int fontSizeTitle READ fontSizeTitle CONSTANT)
    Q_PROPERTY(int fontSizeLarge READ fontSizeLarge CONSTANT)

    // —— 间距 ——
    Q_PROPERTY(int spacingXs READ spacingXs CONSTANT)
    Q_PROPERTY(int spacingS READ spacingS CONSTANT)
    Q_PROPERTY(int spacingM READ spacingM CONSTANT)
    Q_PROPERTY(int spacingL READ spacingL CONSTANT)
    Q_PROPERTY(int spacingXl READ spacingXl CONSTANT)

public:
    explicit Theme(QObject *parent = nullptr);

    QString styleName() const;
    void setStyleName(const QString &name);

    QColor primary() const;
    QColor primaryLight() const;
    QColor primaryDark() const;
    QColor accent() const;
    QColor background() const;
    QColor card() const;
    QColor border() const;
    QColor textPrimary() const;
    QColor textSecondary() const;
    QColor success() const;
    QColor warn() const;
    QColor danger() const;

    int radius() const;
    int radiusSmall() const;
    QString fontFamily() const;

    int fontSizeTiny() const;
    int fontSizeSmall() const;
    int fontSizeBase() const;
    int fontSizeTitle() const;
    int fontSizeLarge() const;

    int spacingXs() const;
    int spacingS() const;
    int spacingM() const;
    int spacingL() const;
    int spacingXl() const;

signals:
    void styleNameChanged();

private:
    QVariantMap styleFor(const QString &name) const;
    QVariantMap m_style;   // 当前生效的风格
    QString m_styleName;
};

#endif // THEME_H