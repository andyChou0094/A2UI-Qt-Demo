#include "HostShell.h"
#include "SurfaceRenderer.h"
#include "Theme.h"
#include "WidgetRegistry.h"

#include <QCoreApplication>
#include <QHostAddress>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>
#include <QVariant>
#include <QtTest>

namespace {

QByteArray hostHttpResponse(int status, const QByteArray &body)
{
    return QByteArrayLiteral("HTTP/1.1 ") + QByteArray::number(status)
        + (status == 200 ? QByteArrayLiteral(" OK\r\n")
                         : QByteArrayLiteral(" Unprocessable Entity\r\n"))
        + QByteArrayLiteral("Content-Type: application/json\r\nConnection: close\r\nContent-Length: ")
        + QByteArray::number(body.size()) + QByteArrayLiteral("\r\n\r\n") + body;
}

void hostReplyOnce(QTcpServer *server, const QByteArray &body, int status)
{
    QObject::connect(server, &QTcpServer::newConnection, server, [server, body, status]() {
        QTcpSocket *socket = server->nextPendingConnection();
        QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket, body, status]() {
            const QByteArray request = socket->readAll();
            if (!request.contains("\r\n\r\n")) {
                return;
            }
            socket->write(hostHttpResponse(status, body));
            socket->disconnectFromHost();
        });
    });
}

} // namespace

class HostShellTest : public QObject
{
    Q_OBJECT

private slots:
    void exposesFixedControlsAndScrollableMainSurface()
    {
        a2ui::WidgetRegistry registry;
        QVERIFY(registry.registerFactory(QStringLiteral("Clock"), [](QWidget *parent) {
            QWidget *widget = new QWidget(parent);
            widget->setMinimumSize(1000, 800);
            return widget;
        }));

        a2ui::HostShell shell(registry);
        QVERIFY(shell.promptInput());
        QVERIFY(shell.composeButton());
        QVERIFY(shell.progressLabel());
        QVERIFY(shell.statusPanel());
        QVERIFY(shell.statusPanel()->isReadOnly());
        QVERIFY(shell.statusSummary());
        QVERIFY(shell.diagnosticsToggle());
        QVERIFY(shell.importButton());
        QVERIFY(shell.exportButton());
        QVERIFY(shell.restoreDefaultButton());
        QVERIFY(shell.workspaceSplitter());
        QCOMPARE(shell.workspaceSplitter()->count(), 2);
        QVERIFY(shell.statusPanel()->isHidden());
        QVERIFY(shell.findChild<QWidget *>(QStringLiteral("workspaceControlPane")));
        QVERIFY(shell.findChild<QWidget *>(QStringLiteral("surfaceStage")));
        QVERIFY(shell.surfaceScrollArea()->widgetResizable());
        QCOMPARE(shell.surfaceScrollArea()->widget(), shell.surfaceHost());

        const QByteArray surface = QByteArrayLiteral(
            "{\"version\":\"0.1\",\"surfaceId\":\"main\",\"root\":\"root\",\"nodes\":["
            "{\"id\":\"root\",\"type\":\"Column\",\"children\":[\"clock-main\"]},"
            "{\"id\":\"clock-main\",\"type\":\"Clock\"}]}");
        QVERIFY(shell.applySurface(surface).applied);

        shell.resize(360, 280);
        shell.show();
        QCoreApplication::processEvents();
        QVERIFY(shell.surfaceScrollArea()->horizontalScrollBar()->maximum() > 0);
        QVERIFY(shell.surfaceScrollArea()->verticalScrollBar()->maximum() > 0);
        QCOMPARE(shell.progressLabel()->property("statusKind").toString(),
                 QStringLiteral("success"));
        QVERIFY(shell.exportButton()->isEnabled());
        QVERIFY(shell.surfaceMetadata()->text().contains(QStringLiteral("main")));
        QVERIFY(shell.surfaceMetadata()->text().contains(QStringLiteral("2 nodes")));
    }

    void reportsInvalidSurfaceWithoutReplacingTheValidContent()
    {
        a2ui::WidgetRegistry registry;
        QVERIFY(registry.registerFactory(QStringLiteral("Clock"), [](QWidget *parent) {
            return new QWidget(parent);
        }));
        a2ui::HostShell shell(registry);
        const QByteArray valid = QByteArrayLiteral(
            "{\"version\":\"0.1\",\"surfaceId\":\"main\",\"root\":\"clock-main\",\"nodes\":["
            "{\"id\":\"clock-main\",\"type\":\"Clock\"}]}");
        QVERIFY(shell.applySurface(valid).applied);
        QWidget *active = shell.surfaceHost()->findChild<QWidget *>(QString(), Qt::FindDirectChildrenOnly);
        QVERIFY(active);
        QVERIFY(!shell.applySurface(QByteArrayLiteral("{" )).applied);
        QCOMPARE(shell.surfaceHost()->findChild<QWidget *>(QString(), Qt::FindDirectChildrenOnly), active);
        QCOMPARE(shell.progressLabel()->text(), QStringLiteral("界面无效"));
        QVERIFY(shell.statusSummary()->text().contains(QStringLiteral("保留最后有效")));
        QVERIFY(!shell.statusPanel()->toPlainText().isEmpty());
        QVERIFY(shell.statusPanel()->toPlainText().contains(QStringLiteral("invalid_surface")));
        QVERIFY(shell.statusPanel()->toPlainText().contains(QStringLiteral("原始诊断")));
        shell.diagnosticsToggle()->click();
        QVERIFY(!shell.statusPanel()->isHidden());
        shell.diagnosticsToggle()->click();
        QVERIFY(shell.statusPanel()->isHidden());
    }

    void staticThemeAndChineseFontFallbackAreHostControlled()
    {
        const a2ui::ThemeResult result = a2ui::applyHostTheme(qApp);
        QVERIFY(result.loaded);
        QVERIFY(!qApp->styleSheet().isEmpty());
        QCOMPARE(qApp->property("a2ui.hostTheme").toString(),
                 QStringLiteral("controlled-workbench-v2"));
        QVERIFY(!result.fontFamily.isEmpty());
    }

    void restoresVersionedDefaultResourceTransactionally()
    {
        const a2ui::ThemeResult theme = a2ui::applyHostTheme(qApp);
        QVERIFY(theme.loaded);
        a2ui::WidgetRegistry registry;
        const QStringList types = QStringList()
            << QStringLiteral("Calculator") << QStringLiteral("CalculationHistory")
            << QStringLiteral("CalculationStats") << QStringLiteral("Clock")
            << QStringLiteral("NotePad");
        for (QStringList::const_iterator it = types.constBegin(); it != types.constEnd(); ++it) {
            QVERIFY(registry.registerFactory(*it, [](QWidget *parent) {
                return new QWidget(parent);
            }));
        }
        a2ui::HostShell shell(registry);
        QVERIFY(shell.restoreDefaultSurface());
        QVERIFY(!shell.currentSurfaceJson().isEmpty());
        QVERIFY(shell.surfaceMetadata()->text().contains(QStringLiteral("7 nodes")));
        QVERIFY(shell.surfaceMetadata()->text().contains(QStringLiteral("来源：默认")));
    }

    void documentFailuresAndCancellationKeepCurrentSurface()
    {
        const a2ui::ThemeResult theme = a2ui::applyHostTheme(qApp);
        QVERIFY(theme.loaded);
        a2ui::WidgetRegistry registry;
        QVERIFY(registry.registerFactory(QStringLiteral("Clock"), [](QWidget *parent) {
            return new QWidget(parent);
        }));
        const QByteArray valid = QByteArrayLiteral(
            "{\"version\":\"0.1\",\"surfaceId\":\"main\",\"root\":\"clock-main\",\"nodes\":["
            "{\"id\":\"clock-main\",\"type\":\"Clock\"}]}");

        QTcpServer validationServer;
        QVERIFY(validationServer.listen(QHostAddress::LocalHost));
        hostReplyOnce(&validationServer,
            QByteArrayLiteral("{\"error\":{\"code\":\"invalid_surface_document\",\"message\":\"bad graph\",\"diagnostics\":[\"bad graph\"]}}"), 422);
        const QVariant previousUrl = qApp->property("a2ui.compositionApiBaseUrl");
        qApp->setProperty("a2ui.compositionApiBaseUrl",
            QUrl(QStringLiteral("http://127.0.0.1:%1").arg(validationServer.serverPort())));
        a2ui::HostShell shell(registry);
        QVERIFY(shell.applySurface(valid).applied);
        const QByteArray before = shell.currentSurfaceJson();
        QWidget *active = shell.surfaceHost()->findChild<QWidget *>(QString(), Qt::FindDirectChildrenOnly);
        QVERIFY(!shell.exportCurrentSurface(QString()));
        QCOMPARE(shell.currentSurfaceJson(), before);
        QVERIFY(shell.importSurfaceDocument(QByteArrayLiteral("{")));
        QTRY_COMPARE_WITH_TIMEOUT(shell.progressLabel()->text(), QStringLiteral("文档操作失败"), 1000);
        QCOMPARE(shell.currentSurfaceJson(), before);
        QCOMPARE(shell.surfaceHost()->findChild<QWidget *>(QString(), Qt::FindDirectChildrenOnly), active);

        QVERIFY(!shell.restoreDefaultSurface());
        QCOMPARE(shell.currentSurfaceJson(), before);
        QCOMPARE(shell.surfaceHost()->findChild<QWidget *>(QString(), Qt::FindDirectChildrenOnly), active);
        qApp->setProperty("a2ui.compositionApiBaseUrl", previousUrl);
    }

    void exportWriteFailureDoesNotCreateSuccessOrChangePage()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        const QByteArray canonical = QByteArrayLiteral(
            "{\n  \"version\": \"0.1\",\n  \"surfaceId\": \"main\",\n"
            "  \"root\": \"clock-main\",\n  \"nodes\": [{\"id\":\"clock-main\",\"type\":\"Clock\"}]\n}\n");
        hostReplyOnce(&server, canonical, 200);
        const QVariant previousUrl = qApp->property("a2ui.compositionApiBaseUrl");
        qApp->setProperty("a2ui.compositionApiBaseUrl",
            QUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort())));
        a2ui::WidgetRegistry registry;
        QVERIFY(registry.registerFactory(QStringLiteral("Clock"), [](QWidget *parent) {
            return new QWidget(parent);
        }));
        a2ui::HostShell shell(registry);
        const QByteArray valid = QByteArrayLiteral(
            "{\"version\":\"0.1\",\"surfaceId\":\"main\",\"root\":\"clock-main\",\"nodes\":["
            "{\"id\":\"clock-main\",\"type\":\"Clock\"}]}");
        QVERIFY(shell.applySurface(valid).applied);
        const QByteArray before = shell.currentSurfaceJson();
        QVERIFY(shell.exportCurrentSurface(QStringLiteral("/proc/a2ui-no-write/surface.json")));
        QTRY_COMPARE_WITH_TIMEOUT(shell.progressLabel()->text(), QStringLiteral("导出失败"), 1000);
        QCOMPARE(shell.currentSurfaceJson(), before);
        qApp->setProperty("a2ui.compositionApiBaseUrl", previousUrl);
    }

    void controllerFitsNineHundredBySevenHundredAndSurfaceRemainsScrollable()
    {
        a2ui::WidgetRegistry registry;
        QVERIFY(registry.registerFactory(QStringLiteral("Clock"), [](QWidget *parent) {
            QWidget *widget = new QWidget(parent);
            widget->setMinimumSize(1200, 900);
            return widget;
        }));
        a2ui::HostShell shell(registry);
        const QByteArray surface = QByteArrayLiteral(
            "{\"version\":\"0.1\",\"surfaceId\":\"main\",\"root\":\"clock-main\",\"nodes\":["
            "{\"id\":\"clock-main\",\"type\":\"Clock\"}]}");
        QVERIFY(shell.applySurface(surface).applied);
        shell.resize(900, 700);
        shell.show();
        QCoreApplication::processEvents();
        QVERIFY(shell.promptInput()->geometry().isValid());
        QVERIFY(shell.composeButton()->geometry().isValid());
        QVERIFY(!shell.promptInput()->geometry().intersects(shell.composeButton()->geometry()));
        QVERIFY(shell.surfaceScrollArea()->verticalScrollBar()->maximum() > 0);
    }
};

QTEST_MAIN(HostShellTest)
#include "test_host_shell.moc"
