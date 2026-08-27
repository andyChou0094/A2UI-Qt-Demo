#include "DemoWidgets.h"
#include "CalculationService.h"

#include <QtTest>

#include <QElapsedTimer>
#include <QLabel>
#include <QTcpServer>
#include <QTcpSocket>
#include <QWidget>

namespace {

class MockCalculationHttpServer
{
public:
    MockCalculationHttpServer()
        : hasRecord_(false)
    {
        QObject::connect(&server_, &QTcpServer::newConnection, &server_, [this]() {
            QTcpSocket *socket = server_.nextPendingConnection();
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [this, socket]() {
                QByteArray request = socket->property("requestBytes").toByteArray();
                request += socket->readAll();
                socket->setProperty("requestBytes", request);
                if (!request.contains("\r\n\r\n")
                    || socket->property("responded").toBool()) {
                    return;
                }
                if (request.startsWith("POST ")
                    && !request.contains("\"expression\":\"1+2\"")) {
                    return;
                }
                socket->setProperty("responded", true);

                QByteArray body;
                QByteArray status = QByteArrayLiteral("200 OK");
                if (request.startsWith("POST /api/calculations ")) {
                    hasRecord_ = true;
                    status = QByteArrayLiteral("201 Created");
                    body = recordJson();
                } else if (request.startsWith("GET /api/calculations/summary ")) {
                    body = hasRecord_
                        ? QByteArrayLiteral("{\"count\":1,\"latest\":") + recordJson()
                            + QByteArrayLiteral("}")
                        : QByteArrayLiteral("{\"count\":0,\"latest\":null}");
                } else if (request.startsWith("GET /api/calculations?limit=50 ")) {
                    body = hasRecord_
                        ? QByteArrayLiteral("[") + recordJson() + QByteArrayLiteral("]")
                        : QByteArrayLiteral("[]");
                } else {
                    status = QByteArrayLiteral("404 Not Found");
                    body = QByteArrayLiteral("{}");
                }
                socket->write(QByteArrayLiteral("HTTP/1.1 ") + status
                    + QByteArrayLiteral("\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: ")
                    + QByteArray::number(body.size()) + QByteArrayLiteral("\r\n\r\n") + body);
                socket->disconnectFromHost();
            });
        });
    }

    bool listen() { return server_.listen(QHostAddress::LocalHost); }
    QUrl baseUrl() const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1").arg(server_.serverPort()));
    }

private:
    static QByteArray recordJson()
    {
        return QByteArrayLiteral(
            "{\"createdAt\":\"2026-08-18T00:00:00Z\",\"expression\":\"1+2\","
            "\"id\":\"record-1\",\"note\":\"\",\"result\":3,"
            "\"updatedAt\":\"2026-08-18T00:00:00Z\"}");
    }

    QTcpServer server_;
    bool hasRecord_;
};

} // namespace

class DemoWidgetsTest : public QObject
{
    Q_OBJECT

private slots:
    void widgetsAreEmbeddableAndStateful();
    void instancesKeepIndependentState();
    void serviceFailuresPreserveLastValidWidgetState();
    void backendPollingMakesCalculationVisibleWithinFourSeconds();
};

void DemoWidgetsTest::widgetsAreEmbeddableAndStateful()
{
    QWidget parent;
    a2ui::Calculator calculator(&parent);
    a2ui::CalculationHistory history(&parent);
    a2ui::CalculationStats stats(&parent);
    a2ui::Clock clock(&parent);
    a2ui::NotePad notePad(&parent);

    const QList<QWidget *> widgets = QList<QWidget *>()
        << &calculator << &history << &stats << &clock << &notePad;
    for (int index = 0; index < widgets.size(); ++index) {
        QCOMPARE(widgets.at(index)->parentWidget(), &parent);
        QVERIFY(!widgets.at(index)->isWindow());
        QCOMPARE(widgets.at(index)->objectName(), QStringLiteral("businessCard"));
        QVERIFY(!widgets.at(index)->property("componentType").toString().isEmpty());
        QLabel *title = widgets.at(index)->findChild<QLabel *>(
            QStringLiteral("businessCardTitle"));
        QVERIFY(title);
        QVERIFY(!title->text().isEmpty());
    }

    calculator.setInputText(QStringLiteral("12+3"));
    QCOMPARE(calculator.inputText(), QStringLiteral("12+3"));

    history.setRecords(QStringList() << QStringLiteral("1+1 = 2")
                                     << QStringLiteral("2+2 = 4"));
    QCOMPARE(history.recordCount(), 2);

    stats.setSummary(2, QStringLiteral("2+2 = 4"));
    QCOMPARE(stats.count(), 2);
    QCOMPARE(stats.latest(), QStringLiteral("2+2 = 4"));

    QVERIFY(clock.isRunning());

    notePad.setText(QStringLiteral("草稿"));
    QCOMPARE(notePad.text(), QStringLiteral("草稿"));
}

void DemoWidgetsTest::instancesKeepIndependentState()
{
    QWidget parent;
    a2ui::Calculator firstCalculator(&parent);
    a2ui::Calculator secondCalculator(&parent);
    a2ui::NotePad firstNotePad(&parent);
    a2ui::NotePad secondNotePad(&parent);

    firstCalculator.setInputText(QStringLiteral("7"));
    secondCalculator.setInputText(QStringLiteral("9"));
    firstNotePad.setText(QStringLiteral("甲"));
    secondNotePad.setText(QStringLiteral("乙"));

    QCOMPARE(firstCalculator.inputText(), QStringLiteral("7"));
    QCOMPARE(secondCalculator.inputText(), QStringLiteral("9"));
    QCOMPARE(firstNotePad.text(), QStringLiteral("甲"));
    QCOMPARE(secondNotePad.text(), QStringLiteral("乙"));
}

void DemoWidgetsTest::serviceFailuresPreserveLastValidWidgetState()
{
    a2ui::CalculationService service(a2ui::CalculationServiceConfig(
        QUrl(QStringLiteral("https://example.com")), 50));
    QWidget parent;
    a2ui::Calculator calculator(&service, &parent);
    a2ui::CalculationHistory history(&service, &parent);
    a2ui::CalculationStats stats(&service, &parent);

    calculator.setInputText(QStringLiteral("1+2"));
    calculator.pressButton(QStringLiteral("="));
    QCOMPARE(calculator.inputText(), QStringLiteral("3"));
    QCOMPARE(calculator.statusText(), QStringLiteral("保存失败"));

    history.setRecords(QStringList() << QStringLiteral("last valid"));
    history.refresh();
    QCOMPARE(history.recordCount(), 1);
    QVERIFY(history.statusText().contains(QStringLiteral("保留最后有效")));

    stats.setSummary(7, QStringLiteral("6*7 = 42"));
    stats.refresh();
    QCOMPARE(stats.count(), 7);
    QCOMPARE(stats.latest(), QStringLiteral("6*7 = 42"));
    QCOMPARE(stats.statusText(), QStringLiteral("摘要已过期"));
}

void DemoWidgetsTest::backendPollingMakesCalculationVisibleWithinFourSeconds()
{
    MockCalculationHttpServer server;
    QVERIFY(server.listen());
    a2ui::CalculationService service(
        a2ui::CalculationServiceConfig(server.baseUrl(), 500));
    QWidget parent;
    a2ui::Calculator calculator(&service, &parent);
    a2ui::CalculationHistory history(&service, &parent);
    a2ui::CalculationStats stats(&service, &parent);

    calculator.setInputText(QStringLiteral("1+2"));
    QElapsedTimer elapsed;
    elapsed.start();
    calculator.pressButton(QStringLiteral("="));

    while ((history.recordCount() != 1 || stats.count() != 1)
           && elapsed.elapsed() < 4000) {
        QTest::qWait(50);
    }
    QCOMPARE(history.recordCount(), 1);
    QCOMPARE(stats.count(), 1);
    QVERIFY(elapsed.elapsed() < 4000);
    QCOMPARE(calculator.statusText(), QStringLiteral("已持久化"));
}

QTEST_MAIN(DemoWidgetsTest)

#include "test_demo_widgets.moc"
