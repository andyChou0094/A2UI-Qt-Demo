#include "SurfaceDocumentClient.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

namespace {

const QByteArray kSurface = QByteArrayLiteral(
    "{\"version\":\"0.1\",\"surfaceId\":\"main\",\"root\":\"clock-main\",\"nodes\":["
    "{\"id\":\"clock-main\",\"type\":\"Clock\"}]}");

QByteArray httpResponse(int status, const QByteArray &body,
                        const QByteArray &extraHeaders = QByteArray())
{
    const QByteArray reason = status == 200 ? QByteArrayLiteral("OK")
                                            : QByteArrayLiteral("Unprocessable Entity");
    return QByteArrayLiteral("HTTP/1.1 ") + QByteArray::number(status)
        + QByteArrayLiteral(" ") + reason
        + QByteArrayLiteral("\r\nContent-Type: application/json\r\n")
        + extraHeaders + QByteArrayLiteral("Connection: close\r\nContent-Length: ")
        + QByteArray::number(body.size()) + QByteArrayLiteral("\r\n\r\n") + body;
}

void replyOnce(QTcpServer *server, const QByteArray &body, int status,
               const QByteArray &extraHeaders, QByteArray *captured)
{
    QObject::connect(server, &QTcpServer::newConnection, server,
                     [server, body, status, extraHeaders, captured]() {
        QTcpSocket *socket = server->nextPendingConnection();
        QObject::connect(socket, &QTcpSocket::readyRead, socket,
                         [socket, body, status, extraHeaders, captured]() {
            *captured += socket->readAll();
            if (!captured->contains("\r\n\r\n")) {
                return;
            }
            socket->write(httpResponse(status, body, extraHeaders));
            socket->disconnectFromHost();
        });
    });
}

a2ui::CompositionClientConfig configFor(const QTcpServer &server)
{
    return a2ui::CompositionClientConfig(
        QUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort())), 1000);
}

} // namespace

class SurfaceDocumentClientTest : public QObject
{
    Q_OBJECT

private slots:
    void importsOnlyCompleteValidatedResponse()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QByteArray captured;
        replyOnce(&server, QByteArrayLiteral("{\"surface\":") + kSurface + QByteArrayLiteral("}"),
                  200, QByteArray(), &captured);
        a2ui::SurfaceDocumentClient client(configFor(server));
        QSignalSpy validated(&client, SIGNAL(importValidated(QByteArray)));
        QSignalSpy failed(&client, SIGNAL(documentFailed(QString,QString,QString)));
        QVERIFY(client.importDocument(kSurface));
        QVERIFY(validated.wait(1000));
        QCOMPARE(failed.count(), 0);
        QVERIFY(captured.startsWith("POST /surface/import HTTP/1.1"));
        QCOMPARE(QJsonDocument::fromJson(validated.at(0).at(0).toByteArray()),
                 QJsonDocument::fromJson(kSurface));
    }

    void structuredValidationFailureDoesNotEmitImport()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QByteArray captured;
        replyOnce(&server,
                  QByteArrayLiteral("{\"error\":{\"code\":\"invalid_surface_document\",\"message\":\"bad graph\",\"diagnostics\":[\"bad graph\"]}}"),
                  422, QByteArray(), &captured);
        a2ui::SurfaceDocumentClient client(configFor(server));
        QSignalSpy validated(&client, SIGNAL(importValidated(QByteArray)));
        QSignalSpy failed(&client, SIGNAL(documentFailed(QString,QString,QString)));
        QVERIFY(client.importDocument(QByteArrayLiteral("{")));
        QVERIFY(failed.wait(1000));
        QCOMPARE(validated.count(), 0);
        QCOMPARE(failed.at(0).at(0).toString(), QStringLiteral("import"));
        QCOMPARE(failed.at(0).at(1).toString(), QStringLiteral("invalid_surface_document"));
    }

    void exportsCanonicalBytesAndSuggestedFileName()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QByteArray captured;
        const QByteArray canonical = QByteArrayLiteral("{\n  \"surfaceId\": \"main\"\n}\n");
        replyOnce(&server, canonical, 200,
                  QByteArrayLiteral("Content-Disposition: attachment; filename=\"surface-main.json\"\r\n"),
                  &captured);
        a2ui::SurfaceDocumentClient client(configFor(server));
        QSignalSpy ready(&client, SIGNAL(exportReady(QByteArray,QString)));
        QVERIFY(client.exportDocument(kSurface));
        QVERIFY(ready.wait(1000));
        QCOMPARE(ready.at(0).at(0).toByteArray(), canonical);
        QCOMPARE(ready.at(0).at(1).toString(), QStringLiteral("surface-main.json"));
        QVERIFY(captured.startsWith("POST /surface/export HTTP/1.1"));
    }

    void networkFailureIsReportedWithoutAFalseSuccess()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        const quint16 closedPort = server.serverPort();
        server.close();
        a2ui::SurfaceDocumentClient client(a2ui::CompositionClientConfig(
            QUrl(QStringLiteral("http://127.0.0.1:%1").arg(closedPort)), 1000));
        QSignalSpy ready(&client, SIGNAL(defaultReady(QByteArray)));
        QSignalSpy failed(&client, SIGNAL(documentFailed(QString,QString,QString)));
        QVERIFY(client.requestDefault());
        QVERIFY(failed.wait(1000));
        QCOMPARE(ready.count(), 0);
        QCOMPARE(failed.at(0).at(0).toString(), QStringLiteral("default"));
    }
};

QTEST_MAIN(SurfaceDocumentClientTest)
#include "test_surface_document_client.moc"
