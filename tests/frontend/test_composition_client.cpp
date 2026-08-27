#include "CompositionClient.h"
#include "SurfaceRenderer.h"
#include "WidgetRegistry.h"

#include <QPointer>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

namespace {

const QByteArray kCurrentSurface = QByteArrayLiteral(
    "{\"version\":\"0.1\",\"surfaceId\":\"main\",\"root\":\"clock-main\",\"nodes\":["
    "{\"id\":\"clock-main\",\"type\":\"Clock\"}]}");

QByteArray httpResponse(int status, const QByteArray &body)
{
    const QByteArray reason = status == 200 ? QByteArrayLiteral("OK")
                                            : QByteArrayLiteral("Unprocessable Entity");
    return QByteArrayLiteral("HTTP/1.1 ") + QByteArray::number(status) + QByteArrayLiteral(" ")
        + reason + QByteArrayLiteral("\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: ")
        + QByteArray::number(body.size()) + QByteArrayLiteral("\r\n\r\n") + body;
}

void configureServer(QTcpServer *server, const QByteArray &body, int status,
                     QByteArray *capturedRequest)
{
    QObject::connect(server, &QTcpServer::newConnection, server,
                     [server, body, status, capturedRequest]() {
        QTcpSocket *socket = server->nextPendingConnection();
        QObject::connect(socket, &QTcpSocket::readyRead, socket,
                         [socket, body, status, capturedRequest]() {
            *capturedRequest += socket->readAll();
            if (!capturedRequest->contains("\r\n\r\n")) {
                return;
            }
            socket->write(httpResponse(status, body));
            socket->disconnectFromHost();
        });
    });
}

a2ui::WidgetRegistry createRegistry()
{
    a2ui::WidgetRegistry registry;
    registry.registerFactory(QStringLiteral("Clock"), [](QWidget *parent) {
        return new QWidget(parent);
    });
    registry.registerFactory(QStringLiteral("Calculator"), [](QWidget *parent) {
        return new QWidget(parent);
    });
    return registry;
}

} // namespace

class CompositionClientTest : public QObject
{
    Q_OBJECT

private slots:
    void usesProviderAwareDefaultAndApplicationTimeout()
    {
        a2ui::CompositionClientConfig defaults;
        QCOMPARE(defaults.timeoutMilliseconds, 120000);

        QCoreApplication *application = QCoreApplication::instance();
        QVERIFY(application != 0);
        const QVariant previous = application->property("a2ui.compositionApiTimeoutMs");
        application->setProperty("a2ui.compositionApiTimeoutMs", 125000);
        const a2ui::CompositionClientConfig configured =
            a2ui::CompositionClientConfig::fromApplication();
        QCOMPARE(configured.timeoutMilliseconds, 125000);
        application->setProperty("a2ui.compositionApiTimeoutMs", previous);
    }

    void appliesValidatedCompleteSurfaceFromFixedComposeEndpoint()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QByteArray captured;
        const QByteArray body = QByteArrayLiteral(
            "{\"surface\":{\"version\":\"0.1\",\"surfaceId\":\"main\",\"root\":\"root\",\"nodes\":["
            "{\"id\":\"root\",\"type\":\"Row\",\"children\":[\"clock-main\",\"calculator-new\"]},"
            "{\"id\":\"clock-main\",\"type\":\"Clock\"},"
            "{\"id\":\"calculator-new\",\"type\":\"Calculator\"}]}}");
        configureServer(&server, body, 200, &captured);

        QWidget host;
        a2ui::WidgetRegistry registry = createRegistry();
        a2ui::SurfaceRenderer renderer(&host, &registry);
        QVERIFY(renderer.apply(kCurrentSurface).applied);
        QWidget *clock = renderer.activeSurface();
        a2ui::CompositionClient client(
            a2ui::CompositionClientConfig(
                QUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort())), 1000),
            &renderer);
        QSignalSpy applied(&client, SIGNAL(compositionApplied(QByteArray)));
        QSignalSpy failed(&client, SIGNAL(compositionFailed(QString,QString)));
        QVERIFY(client.compose(QStringLiteral("添加计算器"), kCurrentSurface));
        QVERIFY(applied.wait(1000));
        QCOMPARE(failed.count(), 0);
        QVERIFY(captured.startsWith("POST /compose HTTP/1.1"));
        QVERIFY(!captured.contains("/api/calculations"));
        QVERIFY(renderer.activeSurface() != clock);
        QCOMPARE(renderer.activeSurface()->findChildren<QWidget *>().size() >= 2, true);
    }

    void unsupportedLayoutPreservesLastValidSurface()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QByteArray captured;
        configureServer(
            &server,
            QByteArrayLiteral("{\"error\":{\"code\":\"unsupported_layout\",\"message\":\"Grid is unsupported\",\"diagnostics\":[\"Grid is unsupported\"]}}"),
            422, &captured);
        QWidget host;
        a2ui::WidgetRegistry registry = createRegistry();
        a2ui::SurfaceRenderer renderer(&host, &registry);
        QVERIFY(renderer.apply(kCurrentSurface).applied);
        QPointer<QWidget> original(renderer.activeSurface());
        a2ui::CompositionClient client(
            a2ui::CompositionClientConfig(
                QUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort())), 1000),
            &renderer);
        QSignalSpy failed(&client, SIGNAL(compositionFailed(QString,QString)));
        QVERIFY(client.compose(QStringLiteral("使用 Grid"), kCurrentSurface));
        QVERIFY(failed.wait(1000));
        QCOMPARE(failed.at(0).at(0).toString(), QStringLiteral("unsupported_layout"));
        QVERIFY(!original.isNull());
        QCOMPARE(renderer.activeSurface(), original.data());
    }

    void timesOutWithoutChangingSurfaceAndCanRetryLater()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server, &QTcpServer::newConnection, &server, [&server]() {
            QTcpSocket *socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [socket]() { socket->readAll(); });
        });
        QWidget host;
        a2ui::WidgetRegistry registry = createRegistry();
        a2ui::SurfaceRenderer renderer(&host, &registry);
        QVERIFY(renderer.apply(kCurrentSurface).applied);
        QPointer<QWidget> original(renderer.activeSurface());
        a2ui::CompositionClient client(
            a2ui::CompositionClientConfig(
                QUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort())), 50),
            &renderer);
        QSignalSpy failed(&client, SIGNAL(compositionFailed(QString,QString)));
        QVERIFY(client.compose(QStringLiteral("timeout"), kCurrentSurface));
        QVERIFY(failed.wait(500));
        QCOMPARE(failed.at(0).at(0).toString(), QStringLiteral("timeout"));
        QCOMPARE(renderer.activeSurface(), original.data());
    }

    void appliesRealProviderCompositionWhenExplicitlyEnabled()
    {
        const QByteArray baseUrl = qgetenv("A2UI_REAL_COMPOSITION_BASE_URL");
        if (baseUrl.isEmpty()) {
            QSKIP("A2UI_REAL_COMPOSITION_BASE_URL is not configured");
        }
        QWidget host;
        a2ui::WidgetRegistry registry = createRegistry();
        a2ui::SurfaceRenderer renderer(&host, &registry);
        QVERIFY(renderer.apply(kCurrentSurface).applied);
        a2ui::CompositionClient client(
            a2ui::CompositionClientConfig(QUrl(QString::fromLocal8Bit(baseUrl)), 120000),
            &renderer);
        QSignalSpy applied(&client, SIGNAL(compositionApplied(QByteArray)));
        QSignalSpy failed(&client, SIGNAL(compositionFailed(QString,QString)));
        QVERIFY(client.compose(QStringLiteral("把时钟和计算器左右排列"), kCurrentSurface));
        QVERIFY(applied.wait(120000));
        QCOMPARE(failed.count(), 0);
        QVERIFY(renderer.activeSurface() != 0);
        const QByteArray result = applied.at(0).at(0).toByteArray();
        QVERIFY(result.contains("\"type\":\"Row\""));
        QVERIFY(result.contains("\"id\":\"clock-main\""));
    }
};

QTEST_MAIN(CompositionClientTest)
#include "test_composition_client.moc"
