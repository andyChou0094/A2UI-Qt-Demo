#include "DemoWidgetRegistration.h"
#include "CalculationService.h"
#include "HostShell.h"
#include "Theme.h"
#include "WidgetRegistry.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QTimer>
#include <QUrl>

namespace {

void applyLoopbackConfiguration(QApplication *application)
{
    const QByteArray calculationUrl = qgetenv("A2UI_CALCULATION_API_BASE_URL");
    const QByteArray compositionUrl = qgetenv("A2UI_COMPOSITION_API_BASE_URL");
    const QByteArray compositionTimeout = qgetenv("A2UI_COMPOSITION_TIMEOUT_MS");
    if (!calculationUrl.isEmpty()) {
        application->setProperty("a2ui.calculationApiBaseUrl",
                                 QUrl(QString::fromLocal8Bit(calculationUrl)));
    }
    if (!compositionUrl.isEmpty()) {
        application->setProperty("a2ui.compositionApiBaseUrl",
                                 QUrl(QString::fromLocal8Bit(compositionUrl)));
    }
    if (!compositionTimeout.isEmpty()) {
        bool valid = false;
        const int timeout = QString::fromLatin1(compositionTimeout).toInt(&valid);
        if (valid && timeout > 0) {
            application->setProperty("a2ui.compositionApiTimeoutMs", timeout);
        } else {
            qWarning("Ignoring invalid A2UI_COMPOSITION_TIMEOUT_MS");
        }
    }
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    applyLoopbackConfiguration(&application);
    const a2ui::ThemeResult theme = a2ui::applyHostTheme(&application);
    if (!theme.loaded) {
        qWarning("%s", qPrintable(theme.diagnostic));
    }

    a2ui::WidgetRegistry registry;
    a2ui::CalculationService calculationService(
        a2ui::CalculationServiceConfig::fromApplication(), &application);
    QString error;
    if (!a2ui::registerDemoWidgetFactories(registry, &calculationService, &error)) {
        QMessageBox::critical(0, QStringLiteral("初始化失败"), error);
        return 1;
    }

    a2ui::HostShell dynamicWindow(registry);
    if (!dynamicWindow.restoreDefaultSurface()) {
        QMessageBox::critical(0, QStringLiteral("初始化失败"),
                              QStringLiteral("初始动态 Surface 无法应用"));
        return 1;
    }
    dynamicWindow.resize(900, 700);
    dynamicWindow.show();

    const QByteArray screenshotDirectory = qgetenv("A2UI_DEMO_SCREENSHOT_DIR");
    if (!screenshotDirectory.isEmpty()) {
        const QByteArray screenshotSurfacePath = qgetenv("A2UI_DEMO_SCREENSHOT_SURFACE_PATH");
        if (!screenshotSurfacePath.isEmpty()) {
            QFile surfaceFile(QString::fromLocal8Bit(screenshotSurfacePath));
            if (!surfaceFile.open(QIODevice::ReadOnly)
                    || !dynamicWindow.applySurface(surfaceFile.readAll()).applied) {
                qWarning("Representative screenshot SurfaceSpec could not be applied");
                return 2;
            }
        }
        QTimer::singleShot(2500, &application, [&application,
                                                &dynamicWindow,
                                                screenshotDirectory]() {
            QDir directory;
            const QString path = QString::fromLocal8Bit(screenshotDirectory);
            const bool directoryReady = directory.mkpath(path);
            QDir output(path);
            const bool dynamicSaved = dynamicWindow.grab().save(
                output.filePath(QStringLiteral("dynamic-host.png")));
            application.exit(directoryReady && dynamicSaved ? 0 : 2);
        });
    }
    return application.exec();
}
