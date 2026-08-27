#include "DemoWidgets.h"
#include "SurfaceRenderer.h"
#include "TestSourceRoot.h"
#include "WidgetRegistry.h"

#include <QApplication>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QPointer>
#include <QSet>
#include <QtTest>

namespace {

void registerWidgets(a2ui::WidgetRegistry *registry)
{
    QVERIFY(registry->registerFactory(QStringLiteral("Calculator"), [](QWidget *parent) {
        return new a2ui::Calculator(parent);
    }));
    QVERIFY(registry->registerFactory(QStringLiteral("CalculationHistory"), [](QWidget *parent) {
        return new a2ui::CalculationHistory(parent);
    }));
    QVERIFY(registry->registerFactory(QStringLiteral("CalculationStats"), [](QWidget *parent) {
        return new a2ui::CalculationStats(parent);
    }));
    QVERIFY(registry->registerFactory(QStringLiteral("Clock"), [](QWidget *parent) {
        return new a2ui::Clock(parent);
    }));
    QVERIFY(registry->registerFactory(QStringLiteral("NotePad"), [](QWidget *parent) {
        return new a2ui::NotePad(parent);
    }));
}

QWidget *node(QWidget *root, const QString &id)
{
    if (root && root->property("a2uiNodeId").toString() == id) {
        return root;
    }
    const QList<QWidget *> widgets = root ? root->findChildren<QWidget *>() : QList<QWidget *>();
    for (QList<QWidget *>::const_iterator it = widgets.constBegin(); it != widgets.constEnd(); ++it) {
        if ((*it)->property("a2uiNodeId").toString() == id) {
            return *it;
        }
    }
    return 0;
}

const QByteArray initialSurface = QByteArrayLiteral(
    "{\"version\":\"0.1\",\"surfaceId\":\"main\",\"root\":\"root\",\"nodes\":["
    "{\"id\":\"root\",\"type\":\"Column\",\"children\":[\"calculator-main\",\"history-main\",\"clock-main\",\"notes-main\"]},"
    "{\"id\":\"calculator-main\",\"type\":\"Calculator\"},"
    "{\"id\":\"history-main\",\"type\":\"CalculationHistory\"},"
    "{\"id\":\"clock-main\",\"type\":\"Clock\"},"
    "{\"id\":\"notes-main\",\"type\":\"NotePad\"}]}");

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    return file.readAll();
}

} // namespace

class ReconciliationStateTest : public QObject
{
    Q_OBJECT

private slots:
    void recordedLlmSurfacesPreserveStableQObjectIdentity()
    {
        const QString sourceDir = a2ui_test::sourceRoot();
        QVERIFY2(!sourceDir.isEmpty(), "未能在运行时解析测试源码根");
        const QJsonDocument reportDocument = QJsonDocument::fromJson(
            readFile(sourceDir + QStringLiteral("/docs/llm-acceptance-results.json")));
        QVERIFY(reportDocument.isObject());
        const QJsonObject report = reportDocument.object();
        QVERIFY(report.value(QStringLiteral("passed")).toBool());

        const QJsonArray scenarios = report.value(QStringLiteral("scenarios")).toArray();
        QCOMPARE(scenarios.size(), 7);
        for (QJsonArray::const_iterator it = scenarios.constBegin(); it != scenarios.constEnd(); ++it) {
            const QJsonObject scenario = it->toObject();
            const QString label = scenario.value(QStringLiteral("name")).toString();
            QVERIFY2(scenario.value(QStringLiteral("passed")).toBool(), qPrintable(label));

            const QString fixture = scenario.value(QStringLiteral("currentFixture")).toString();
            const QByteArray current = readFile(
                sourceDir + QStringLiteral("/shared/fixtures/surface-spec/valid/") + fixture);
            QVERIFY2(!current.isEmpty(), qPrintable(label));

            a2ui::WidgetRegistry registry;
            registerWidgets(&registry);
            QWidget host;
            a2ui::SurfaceRenderer renderer(&host, &registry);
            QVERIFY2(renderer.apply(current).applied, qPrintable(label));

            const QJsonObject before = scenario.value(QStringLiteral("stableBusinessIdsBefore")).toObject();
            QHash<QString, QWidget *> pointers;
            for (QJsonObject::const_iterator id = before.constBegin(); id != before.constEnd(); ++id) {
                QWidget *widget = node(renderer.activeSurface(), id.key());
                QVERIFY2(widget, qPrintable(label + QStringLiteral(": ") + id.key()));
                pointers.insert(id.key(), widget);
            }

            const QJsonValue compiledValue = scenario.value(QStringLiteral("compiledSurface"));
            if (compiledValue.isObject()) {
                const QByteArray compiled = QJsonDocument(compiledValue.toObject()).toJson(QJsonDocument::Compact);
                QVERIFY2(renderer.apply(compiled).applied, qPrintable(label));
                const QJsonObject after = scenario.value(QStringLiteral("stableBusinessIdsAfter")).toObject();
                for (QHash<QString, QWidget *>::const_iterator id = pointers.constBegin(); id != pointers.constEnd(); ++id) {
                    QVERIFY(after.contains(id.key()));
                    QCOMPARE(node(renderer.activeSurface(), id.key()), id.value());
                }
                const QJsonObject newIds = scenario.value(QStringLiteral("newBusinessIds")).toObject();
                QSet<QWidget *> existingPointers;
                for (QHash<QString, QWidget *>::const_iterator id = pointers.constBegin(); id != pointers.constEnd(); ++id) {
                    existingPointers.insert(id.value());
                }
                for (QJsonObject::const_iterator id = newIds.constBegin(); id != newIds.constEnd(); ++id) {
                    QWidget *created = node(renderer.activeSurface(), id.key());
                    QVERIFY(created);
                    QVERIFY(!existingPointers.contains(created));
                }
            } else {
                QCOMPARE(scenario.value(QStringLiteral("errorCode")).toString(), QStringLiteral("unsupported_layout"));
                for (QHash<QString, QWidget *>::const_iterator id = pointers.constBegin(); id != pointers.constEnd(); ++id) {
                    QCOMPARE(node(renderer.activeSurface(), id.key()), id.value());
                }
            }
        }
    }

    void movePreservesOpaqueBusinessStateIdentityAndFocus()
    {
        a2ui::WidgetRegistry registry;
        registerWidgets(&registry);
        QWidget host;
        host.resize(900, 700);
        a2ui::SurfaceRenderer renderer(&host, &registry);
        QVERIFY(renderer.apply(initialSurface).applied);
        host.show();
        QCoreApplication::processEvents();

        a2ui::Calculator *calculator = qobject_cast<a2ui::Calculator *>(
            node(renderer.activeSurface(), QStringLiteral("calculator-main")));
        a2ui::CalculationHistory *history = qobject_cast<a2ui::CalculationHistory *>(
            node(renderer.activeSurface(), QStringLiteral("history-main")));
        a2ui::Clock *clock = qobject_cast<a2ui::Clock *>(
            node(renderer.activeSurface(), QStringLiteral("clock-main")));
        a2ui::NotePad *notes = qobject_cast<a2ui::NotePad *>(
            node(renderer.activeSurface(), QStringLiteral("notes-main")));
        QVERIFY(calculator);
        QVERIFY(history);
        QVERIFY(clock);
        QVERIFY(notes);
        calculator->setInputText(QStringLiteral("40+2"));
        history->setRecords(QStringList() << QStringLiteral("1+1 = 2") << QStringLiteral("2+2 = 4"));
        history->selectRow(1);
        notes->setText(QStringLiteral("未提交草稿"));
        QLineEdit *input = calculator->findChild<QLineEdit *>();
        QVERIFY(input);
        input->setFocus();
        QCoreApplication::processEvents();
        QCOMPARE(QApplication::focusWidget(), input);

        const QByteArray moved = QByteArrayLiteral(
            "{\"version\":\"0.1\",\"surfaceId\":\"main\",\"root\":\"root\",\"nodes\":["
            "{\"id\":\"root\",\"type\":\"Row\",\"children\":[\"left\",\"right\"]},"
            "{\"id\":\"left\",\"type\":\"Column\",\"children\":[\"notes-main\",\"calculator-main\"]},"
            "{\"id\":\"right\",\"type\":\"Column\",\"children\":[\"clock-main\",\"history-main\"]},"
            "{\"id\":\"notes-main\",\"type\":\"NotePad\"},"
            "{\"id\":\"calculator-main\",\"type\":\"Calculator\"},"
            "{\"id\":\"clock-main\",\"type\":\"Clock\"},"
            "{\"id\":\"history-main\",\"type\":\"CalculationHistory\"}]}");
        QVERIFY(renderer.apply(moved).applied);
        QCoreApplication::processEvents();
        QCOMPARE(node(renderer.activeSurface(), QStringLiteral("calculator-main")), calculator);
        QCOMPARE(node(renderer.activeSurface(), QStringLiteral("history-main")), history);
        QCOMPARE(node(renderer.activeSurface(), QStringLiteral("clock-main")), clock);
        QCOMPARE(node(renderer.activeSurface(), QStringLiteral("notes-main")), notes);
        QCOMPARE(calculator->inputText(), QStringLiteral("40+2"));
        QCOMPARE(history->selectedRow(), 1);
        QCOMPARE(notes->text(), QStringLiteral("未提交草稿"));
        QVERIFY(clock->isRunning());
        QCOMPARE(QApplication::focusWidget(), input);
    }

    void multipleInstancesStayIndependentAndRemovalCreatesFreshState()
    {
        a2ui::WidgetRegistry registry;
        registerWidgets(&registry);
        QWidget host;
        a2ui::SurfaceRenderer renderer(&host, &registry);
        const QByteArray multiple = QByteArrayLiteral(
            "{\"version\":\"0.1\",\"surfaceId\":\"main\",\"root\":\"root\",\"nodes\":["
            "{\"id\":\"root\",\"type\":\"Row\",\"children\":[\"calculator-a\",\"calculator-b\",\"notes-old\"]},"
            "{\"id\":\"calculator-a\",\"type\":\"Calculator\"},"
            "{\"id\":\"calculator-b\",\"type\":\"Calculator\"},"
            "{\"id\":\"notes-old\",\"type\":\"NotePad\"}]}");
        QVERIFY(renderer.apply(multiple).applied);
        a2ui::Calculator *first = qobject_cast<a2ui::Calculator *>(node(renderer.activeSurface(), QStringLiteral("calculator-a")));
        a2ui::Calculator *second = qobject_cast<a2ui::Calculator *>(node(renderer.activeSurface(), QStringLiteral("calculator-b")));
        a2ui::NotePad *oldNotes = qobject_cast<a2ui::NotePad *>(node(renderer.activeSurface(), QStringLiteral("notes-old")));
        QVERIFY(first && second && oldNotes);
        first->setInputText(QStringLiteral("1"));
        second->setInputText(QStringLiteral("2"));
        oldNotes->setText(QStringLiteral("old"));
        QPointer<a2ui::NotePad> destroyed(oldNotes);

        const QByteArray fresh = QByteArrayLiteral(
            "{\"version\":\"0.1\",\"surfaceId\":\"main\",\"root\":\"root\",\"nodes\":["
            "{\"id\":\"root\",\"type\":\"Row\",\"children\":[\"calculator-a\",\"calculator-b\",\"notes-new\"]},"
            "{\"id\":\"calculator-a\",\"type\":\"Calculator\"},"
            "{\"id\":\"calculator-b\",\"type\":\"Calculator\"},"
            "{\"id\":\"notes-new\",\"type\":\"NotePad\"}]}");
        QVERIFY(renderer.apply(fresh).applied);
        QVERIFY(destroyed.isNull());
        QCOMPARE(first->inputText(), QStringLiteral("1"));
        QCOMPARE(second->inputText(), QStringLiteral("2"));
        a2ui::NotePad *newNotes = qobject_cast<a2ui::NotePad *>(node(renderer.activeSurface(), QStringLiteral("notes-new")));
        QVERIFY(newNotes);
        QCOMPARE(newNotes->text(), QString());
    }
};

QTEST_MAIN(ReconciliationStateTest)
#include "test_reconciliation_state.moc"
