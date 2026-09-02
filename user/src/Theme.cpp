#include "Theme.h"

namespace {

// 风格注册表：techBlue(现代科技蓝，默认) / minimalDark(深色极简) / springGreen(清新绿)
const QVariantMap &kStyles() {
    static const QVariantMap s = {
        { QStringLiteral("techBlue"), QVariantMap {
            { QStringLiteral("primary"), QColor("#0e7dff") },
            { QStringLiteral("primaryLight"), QColor("#3d9bff") },
            { QStringLiteral("primaryDark"), QColor("#0a5bbf") },
            { QStringLiteral("accent"), QColor("#12c8ff") },
            { QStringLiteral("background"), QColor("#f5f7fb") },
            { QStringLiteral("card"), QColor("#ffffff") },
            { QStringLiteral("border"), QColor("#e6eaf2") },
            { QStringLiteral("textPrimary"), QColor("#101828") },
            { QStringLiteral("textSecondary"), QColor("#667085") },
            { QStringLiteral("success"), QColor("#12b76a") },
            { QStringLiteral("warn"), QColor("#f79009") },
            { QStringLiteral("danger"), QColor("#f04438") },
            { QStringLiteral("radius"), 12 },
            { QStringLiteral("radiusSmall"), 8 }
        }},
        { QStringLiteral("minimalDark"), QVariantMap {
            { QStringLiteral("primary"), QColor("#38bdf8") },
            { QStringLiteral("primaryLight"), QColor("#7dd3fc") },
            { QStringLiteral("primaryDark"), QColor("#0284c7") },
            { QStringLiteral("accent"), QColor("#22d3ee") },
            { QStringLiteral("background"), QColor("#0b1220") },
            { QStringLiteral("card"), QColor("#17233a") },
            { QStringLiteral("border"), QColor("#24334d") },
            { QStringLiteral("textPrimary"), QColor("#e6efff") },
            { QStringLiteral("textSecondary"), QColor("#8fa3c4") },
            { QStringLiteral("success"), QColor("#34d399") },
            { QStringLiteral("warn"), QColor("#fbbf24") },
            { QStringLiteral("danger"), QColor("#f87171") },
            { QStringLiteral("radius"), 14 },
            { QStringLiteral("radiusSmall"), 8 }
        }},
        { QStringLiteral("springGreen"), QVariantMap {
            { QStringLiteral("primary"), QColor("#16a34a") },
            { QStringLiteral("primaryLight"), QColor("#4ade80") },
            { QStringLiteral("primaryDark"), QColor("#15803d") },
            { QStringLiteral("accent"), QColor("#a3e635") },
            { QStringLiteral("background"), QColor("#f4fbf5") },
            { QStringLiteral("card"), QColor("#ffffff") },
            { QStringLiteral("border"), QColor("#dfeee3") },
            { QStringLiteral("textPrimary"), QColor("#16231a") },
            { QStringLiteral("textSecondary"), QColor("#5f7c68") },
            { QStringLiteral("success"), QColor("#16a34a") },
            { QStringLiteral("warn"), QColor("#f59e0b") },
            { QStringLiteral("danger"), QColor("#ef4444") },
            { QStringLiteral("radius"), 12 },
            { QStringLiteral("radiusSmall"), 8 }
        }}
    };
    return s;
}

} // namespace

Theme::Theme(QObject *parent)
    : QObject(parent), m_styleName(QStringLiteral("techBlue")) {
    m_style = styleFor(m_styleName);
}

QString Theme::styleName() const { return m_styleName; }

void Theme::setStyleName(const QString &name) {
    const QVariantMap s = styleFor(name);
    if (s.isEmpty() || name == m_styleName)
        return;
    m_styleName = name;
    m_style = s;
    emit styleNameChanged();
}

QVariantMap Theme::styleFor(const QString &name) const {
    const QVariantMap &all = kStyles();
    return all.value(name).toMap();
}

#define THEME_COLOR_GETTER(prop) \
    QColor Theme::prop() const { return m_style.value(QStringLiteral(#prop)).value<QColor>(); }

THEME_COLOR_GETTER(primary)
THEME_COLOR_GETTER(primaryLight)
THEME_COLOR_GETTER(primaryDark)
THEME_COLOR_GETTER(accent)
THEME_COLOR_GETTER(background)
THEME_COLOR_GETTER(card)
THEME_COLOR_GETTER(border)
THEME_COLOR_GETTER(textPrimary)
THEME_COLOR_GETTER(textSecondary)
THEME_COLOR_GETTER(success)
THEME_COLOR_GETTER(warn)
THEME_COLOR_GETTER(danger)

#define THEME_INT_GETTER(prop) \
    int Theme::prop() const { return m_style.value(QStringLiteral(#prop)).toInt(); }

THEME_INT_GETTER(radius)
THEME_INT_GETTER(radiusSmall)

QString Theme::fontFamily() const { return QStringLiteral(""); }

// 字号参考（与风格无关）
int Theme::fontSizeTiny() const { return 11; }
int Theme::fontSizeSmall() const { return 13; }
int Theme::fontSizeBase() const { return 15; }
int Theme::fontSizeTitle() const { return 18; }
int Theme::fontSizeLarge() const { return 24; }

// 常用栅格间距（与风格无关）
int Theme::spacingXs() const { return 4; }
int Theme::spacingS() const { return 8; }
int Theme::spacingM() const { return 12; }
int Theme::spacingL() const { return 16; }
int Theme::spacingXl() const { return 24; }