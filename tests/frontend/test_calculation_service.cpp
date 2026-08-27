#include "CalculationService.h"

#include <QtTest>

#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>

class CalculationServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void rejectsNonLoopbackConfiguration();
    void createsRecordAsynchronously();
    void abortsRequestAfterTimeout();
    void recoversAfterTimeout();
};

void CalculationServiceTest::rejectsNonLoopbackConfiguration()
{
    a2ui::CalculationService service(
        a2ui::CalculationServiceConfig(QUrl(QStringLiteral("https://example.com")), 100));
    QVERIFY(!service.isConfigured());
    QSignalSpy failures(&service, SIGNAL(requestFailed(a2ui::CalculationService::Operation,QString)));
    QVERIFY(!service.fetchSummary());
    QCOMPARE(failures.count(), 1);
}

void CalculationServiceTest::createsRecordAsynchronously()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    connect(&server, &QTcpServer::newConnection, &server, [&server]() {
        QTcpSocket *socket = server.nextPendingConnection();
        QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket]() {
            const QByteArray request =
                socket->property("requestBytes").toByteArray() + socket->readAll();
            socket->setProperty("requestBytes", request);
            if (!request.contains("POST /api/calculations HTTP/1.1")
                || !request.contains("\"expression\":\"1+2\"")) {
                return;
            }
            const QByteArray body = QByteArrayLiteral(
                "{\"createdAt\":\"2026-08-18T00:00:00Z\","
                "\"expression\":\"1+2\",\"id\":\"record-1\",\"note\":\"\","
                "\"result\":3,\"updatedAt\":\"2026-08-18T00:00:00Z\"}");
            const QByteArray response = QByteArrayLiteral(
                "HTTP/1.1 201 Created\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: ")
                + QByteArray::number(body.size()) + QByteArrayLiteral("\r\n\r\n") + body;
            socket->write(response);
            socket->disconnectFromHost();
        });
    });

    const QUrl baseUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()));
    a2ui::CalculationService service(
        a2ui::CalculationServiceConfig(baseUrl, 1000));
    QSignalSpy created(&service, SIGNAL(recordCreated(a2ui::CalculationRecord)));
    QSignalSpy failures(&service, SIGNAL(requestFailed(a2ui::CalculationService::Operation,QString)));

    QVERIFY(service.createRecord(QStringLiteral("1+2"), 3.0));
    QVERIFY(created.wait(1000));
    QCOMPARE(failures.count(), 0);
    const a2ui::CalculationRecord record =
        qvariant_cast<a2ui::CalculationRecord>(created.at(0).at(0));
    QCOMPARE(record.id, QStringLiteral("record-1"));
    QCOMPARE(record.expression, QStringLiteral("1+2"));
    QCOMPARE(record.result, 3.0);
}

void CalculationServiceTest::abortsRequestAfterTimeout()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    connect(&server, &QTcpServer::newConnection, &server, [&server]() {
        QTcpSocket *socket = server.nextPendingConnection();
        QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket]() {
            socket->readAll();
        });
    });

    const QUrl baseUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()));
    a2ui::CalculationService service(
        a2ui::CalculationServiceConfig(baseUrl, 50));
    QSignalSpy failures(&service, SIGNAL(requestFailed(a2ui::CalculationService::Operation,QString)));

    QVERIFY(service.fetchSummary());
    QVERIFY(failures.wait(500));
    QCOMPARE(failures.at(0).at(0).value<a2ui::CalculationService::Operation>(),
             a2ui::CalculationService::FetchSummary);
    QVERIFY(failures.at(0).at(1).toString().contains(QStringLiteral("timed out")));
}

void CalculationServiceTest::recoversAfterTimeout()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    int connectionCount = 0;
    connect(&server, &QTcpServer::newConnection, &server,
            [&server, &connectionCount]() {
        QTcpSocket *socket = server.nextPendingConnection();
        const int connectionNumber = ++connectionCount;
        QObject::connect(socket, &QTcpSocket::readyRead, socket,
                         [socket, connectionNumber]() {
            socket->readAll();
            if (connectionNumber != 2) {
                return;
            }
            const QByteArray body = QByteArrayLiteral("{\"count\":0,\"latest\":null}");
            socket->write(QByteArrayLiteral(
                "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: ")
                + QByteArray::number(body.size()) + QByteArrayLiteral("\r\n\r\n") + body);
            socket->disconnectFromHost();
        });
    });

    const QUrl baseUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()));
    a2ui::CalculationService service(a2ui::CalculationServiceConfig(baseUrl, 50));
    QSignalSpy failures(&service, SIGNAL(requestFailed(a2ui::CalculationService::Operation,QString)));
    QSignalSpy summaries(&service, SIGNAL(summaryFetched(a2ui::CalculationSummary)));

    QVERIFY(service.fetchSummary());
    QVERIFY(failures.wait(500));
    QVERIFY(service.fetchSummary());
    QVERIFY(summaries.wait(500));
    QCOMPARE(summaries.count(), 1);
    QCOMPARE(qvariant_cast<a2ui::CalculationSummary>(summaries.at(0).at(0)).count, 0);
}

QTEST_MAIN(CalculationServiceTest)

#include "test_calculation_service.moc"
