#include "Theme.h"

#include <QApplication>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QStringList>

static void initializeThemeResource()
{
    Q_INIT_RESOURCE(theme);
}

namespace a2ui {

ThemeResult applyHostTheme(QApplication *application)
{
    ThemeResult result;
    result.loaded = false;
    result.chineseFontAvailable = false;
    if (!application) {
        result.diagnostic = QStringLiteral("QApplication 不可用，已保留系统样式。");
        return result;
    }
    initializeThemeResource();

    const QStringList preferred = QStringList()
        << QStringLiteral("Noto Sans CJK SC")
        << QStringLiteral("Noto Sans SC")
        << QStringLiteral("Source Han Sans SC")
        << QStringLiteral("WenQuanYi Micro Hei")
        << QStringLiteral("Microsoft YaHei");
    const QStringList installed = QFontDatabase().families();
    for (QStringList::const_iterator it = preferred.constBegin(); it != preferred.constEnd(); ++it) {
        if (installed.contains(*it, Qt::CaseInsensitive)) {
            QFont font = application->font();
            font.setFamily(*it);
            application->setFont(font);
            result.fontFamily = *it;
            result.chineseFontAvailable = true;
            break;
        }
    }
    if (!result.chineseFontAvailable) {
        const QFontDatabase database;
        for (QStringList::const_iterator it = installed.constBegin(); it != installed.constEnd(); ++it) {
            if (database.writingSystems(*it).contains(QFontDatabase::SimplifiedChinese)) {
                QFont font = application->font();
                font.setFamily(*it);
                application->setFont(font);
                result.fontFamily = *it;
                result.chineseFontAvailable = true;
                break;
            }
        }
    }
    if (result.fontFamily.isEmpty()) {
        result.fontFamily = application->font().family();
    }

    QFile stylesheet(QStringLiteral(":/a2ui/theme.qss"));
    if (!stylesheet.open(QIODevice::ReadOnly)) {
        result.diagnostic = QStringLiteral("静态主题资源加载失败，已回退系统样式。");
        return result;
    }
    application->setStyleSheet(QString::fromUtf8(stylesheet.readAll()));
    application->setProperty("a2ui.hostTheme", QStringLiteral("controlled-workbench-v2"));
    application->setProperty("a2ui.chineseFontAvailable", result.chineseFontAvailable);
    result.loaded = true;
    result.diagnostic = result.chineseFontAvailable
        ? QStringLiteral("静态主题与中文字体已就绪。")
        : QStringLiteral("静态主题已就绪；未确认中文字体，使用系统回退。");
    return result;
}

} // namespace a2ui
