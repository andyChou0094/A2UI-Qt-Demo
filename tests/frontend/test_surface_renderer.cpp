#include "SurfaceRenderer.h"
#include "TestSourceRoot.h"
#include "WidgetRegistry.h"

#include <QBoxLayout>
#include <QDir>
#include <QFile>
#include <QFileInfoList>
#include <QPointer>
#include <QSizePolicy>
#include <QtTest>

namespace {

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    return file.readAll();
}

QString fixturePath(const QString &classification, const QString &name)
{
    return a2ui_test::path(QStringLiteral("shared/fixtures/surface-spec/")
        + classification + QStringLiteral("/") + name);
}

a2ui::WidgetRegistry createRegistry()
{
    a2ui::WidgetRegistry registry;
    const QStringList types = QStringList()
        << QStringLiteral("Calculator") << QStringLiteral("CalculationHistory")
        << QStringLiteral("CalculationStats") << QStringLiteral("Clock")
        << QStringLiteral("NotePad");
    for (QStringList::const_iterator it = types.constBegin(); it != types.constEnd(); ++it) {
        const QString type = *it;
        registry.registerFactory(type, [type](QWidget *parent) {
            QWidget *widget = new QWidget(parent);
            widget->setObjectName(type);
            if (type == QStringLiteral("Calculator")) {
                widget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            }
            return widget;
        });
    }
    return registry;
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

} // namespace

class SurfaceRendererTest : public QObject
{
    Q_OBJECT

private slots:
    void rendersEveryValidFixture()
    {
        const QString path = a2ui_test::path(
            QStringLiteral("shared/fixtures/surface-spec/valid"));
        const QFileInfoList files = QDir(path).entryInfoList(
            QStringList() << QStringLiteral("*.json"), QDir::Files, QDir::Name);
        QCOMPARE(files.size(), 7);
        for (QFileInfoList::const_iterator it = files.constBegin(); it != files.constEnd(); ++it) {
            QWidget host;
            a2ui::WidgetRegistry registry = createRegistry();
            a2ui::SurfaceRenderer renderer(&host, &registry);
            const a2ui::SurfaceApplyResult result = renderer.apply(readFile(it->absoluteFilePath()));
            QVERIFY2(result.applied, qPrintable(it->fileName() + QStringLiteral(": ")
                                               + result.diagnostics.join(QStringLiteral("; "))));
            QVERIFY(renderer.activeSurface());
        }
    }

    void mapsMarginsGapWeightsAndPreservesLeafSizePolicy()
    {
        QWidget host;
        a2ui::WidgetRegistry registry = createRegistry();
        a2ui::SurfaceRenderer renderer(&host, &registry);
        QVERIFY(renderer.apply(readFile(fixturePath(QStringLiteral("valid"),
                                                    QStringLiteral("weighted-highlight.json")))).applied);
        QWidget *root = renderer.activeSurface();
        QBoxLayout *layout = qobject_cast<QBoxLayout *>(root->layout());
        QVERIFY(layout);
        int left = -1, top = -1, right = -1, bottom = -1;
        layout->getContentsMargins(&left, &top, &right, &bottom);
        QCOMPARE(left, 0);
        QCOMPARE(top, 0);
        QCOMPARE(right, 0);
        QCOMPARE(bottom, 0);
        QCOMPARE(layout->spacing(), 8);
        QCOMPARE(layout->count(), 2);
        QCOMPARE(layout->stretch(0), 2);
        QCOMPARE(layout->stretch(1), 1);
        QWidget *calculator = node(root, QStringLiteral("calculator-main"));
        QVERIFY(calculator);
        QCOMPARE(calculator->sizePolicy().horizontalPolicy(), QSizePolicy::Fixed);
        QCOMPARE(calculator->sizePolicy().verticalPolicy(), QSizePolicy::Fixed);
    }

    void invalidFixtureDoesNotChangeTheActiveSurface()
    {
        QWidget host;
        a2ui::WidgetRegistry registry = createRegistry();
        a2ui::SurfaceRenderer renderer(&host, &registry);
        QVERIFY(renderer.apply(readFile(fixturePath(QStringLiteral("valid"),
                                                    QStringLiteral("top-bottom.json")))).applied);
        QPointer<QWidget> original(renderer.activeSurface());
        const a2ui::SurfaceApplyResult result = renderer.apply(
            readFile(fixturePath(QStringLiteral("invalid"), QStringLiteral("cycle.json"))));
        QVERIFY(!result.applied);
        QVERIFY(!original.isNull());
        QCOMPARE(renderer.activeSurface(), original.data());
    }

    void mapsEmptyAndSingleItemJustifyWithoutUnexpectedSpacers()
    {
        QWidget host;
        a2ui::WidgetRegistry registry = createRegistry();
        a2ui::SurfaceRenderer renderer(&host, &registry);
        QVERIFY(renderer.apply(readFile(fixturePath(QStringLiteral("valid"),
                                                    QStringLiteral("empty-surface.json")))).applied);
        QCOMPARE(renderer.activeSurface()->layout()->count(), 0);

        const QByteArray centered = QByteArrayLiteral(
            "{\"version\":\"0.1\",\"surfaceId\":\"main\",\"root\":\"root\",\"nodes\":["
            "{\"id\":\"root\",\"type\":\"Row\",\"children\":[\"clock-main\"],\"justify\":\"spaceAround\"},"
            "{\"id\":\"clock-main\",\"type\":\"Clock\"}]}");
        QVERIFY(renderer.apply(centered).applied);
        QCOMPARE(renderer.activeSurface()->layout()->count(), 3);
    }

    void reusesStableLeavesAcrossOrderedMoves()
    {
        QWidget host;
        a2ui::WidgetRegistry registry = createRegistry();
        a2ui::SurfaceRenderer renderer(&host, &registry);
        QVERIFY(renderer.apply(readFile(fixturePath(QStringLiteral("valid"),
                                                    QStringLiteral("top-bottom.json")))).applied);
        QWidget *calculator = node(renderer.activeSurface(), QStringLiteral("calculator-main"));
        QWidget *history = node(renderer.activeSurface(), QStringLiteral("history-main"));
        QVERIFY(calculator);
        QVERIFY(history);
        calculator->setProperty("localInput", QStringLiteral("12+30"));

        const QByteArray moved = QByteArrayLiteral(
            "{\"version\":\"0.1\",\"surfaceId\":\"main\",\"root\":\"root\",\"nodes\":["
            "{\"id\":\"root\",\"type\":\"Row\",\"children\":[\"history-main\",\"calculator-main\"]},"
            "{\"id\":\"history-main\",\"type\":\"CalculationHistory\"},"
            "{\"id\":\"calculator-main\",\"type\":\"Calculator\"}]}");
        const a2ui::SurfaceApplyResult result = renderer.apply(moved);
        QVERIFY(result.applied);
        QCOMPARE(result.plan.orderedLeafIds,
                 QStringList() << QStringLiteral("history-main") << QStringLiteral("calculator-main"));
        QCOMPARE(result.plan.reusedIds.size(), 2);
        QCOMPARE(node(renderer.activeSurface(), QStringLiteral("calculator-main")), calculator);
        QCOMPARE(node(renderer.activeSurface(), QStringLiteral("history-main")), history);
        QCOMPARE(calculator->property("localInput").toString(), QStringLiteral("12+30"));
    }

    void replacesChangedTypeAndDestroysRemovedLeavesAfterCommit()
    {
        QWidget host;
        a2ui::WidgetRegistry registry = createRegistry();
        a2ui::SurfaceRenderer renderer(&host, &registry);
        QVERIFY(renderer.apply(readFile(fixturePath(QStringLiteral("valid"),
                                                    QStringLiteral("top-bottom.json")))).applied);
        QPointer<QWidget> oldCalculator(node(renderer.activeSurface(), QStringLiteral("calculator-main")));
        QPointer<QWidget> oldHistory(node(renderer.activeSurface(), QStringLiteral("history-main")));

        const QByteArray replacement = QByteArrayLiteral(
            "{\"version\":\"0.1\",\"surfaceId\":\"main\",\"root\":\"calculator-main\",\"nodes\":["
            "{\"id\":\"calculator-main\",\"type\":\"Clock\"}]}");
        const a2ui::SurfaceApplyResult result = renderer.apply(replacement);
        QVERIFY(result.applied);
        QCOMPARE(result.plan.replacedIds, QStringList() << QStringLiteral("calculator-main"));
        QCOMPARE(result.plan.removedIds, QStringList() << QStringLiteral("history-main"));
        QVERIFY(oldCalculator.isNull());
        QVERIFY(oldHistory.isNull());
        QWidget *newWidget = node(renderer.activeSurface(), QStringLiteral("calculator-main"));
        QVERIFY(newWidget);
        QCOMPARE(newWidget->property("a2uiNodeType").toString(), QStringLiteral("Clock"));
    }

    void constructionFailureRollsBackStagedResourcesAndActiveTree()
    {
        bool failCalculator = false;
        a2ui::WidgetRegistry registry;
        QVERIFY(registry.registerFactory(QStringLiteral("Clock"), [](QWidget *parent) {
            return new QWidget(parent);
        }));
        QVERIFY(registry.registerFactory(QStringLiteral("Calculator"), [&failCalculator](QWidget *parent) {
            return failCalculator ? static_cast<QWidget *>(0) : new QWidget(parent);
        }));
        QWidget host;
        a2ui::SurfaceRenderer renderer(&host, &registry);
        const QByteArray initial = QByteArrayLiteral(
            "{\"version\":\"0.1\",\"surfaceId\":\"main\",\"root\":\"clock-main\",\"nodes\":["
            "{\"id\":\"clock-main\",\"type\":\"Clock\"}]}");
        QVERIFY(renderer.apply(initial).applied);
        QPointer<QWidget> originalRoot(renderer.activeSurface());

        failCalculator = true;
        const QByteArray failing = QByteArrayLiteral(
            "{\"version\":\"0.1\",\"surfaceId\":\"main\",\"root\":\"root\",\"nodes\":["
            "{\"id\":\"root\",\"type\":\"Row\",\"children\":[\"clock-main\",\"calculator-new\"]},"
            "{\"id\":\"clock-main\",\"type\":\"Clock\"},"
            "{\"id\":\"calculator-new\",\"type\":\"Calculator\"}]}");
        const a2ui::SurfaceApplyResult result = renderer.apply(failing);
        QVERIFY(!result.applied);
        QVERIFY(!originalRoot.isNull());
        QCOMPARE(renderer.activeSurface(), originalRoot.data());
        QCOMPARE(originalRoot->parentWidget(), &host);
    }
};

QTEST_MAIN(SurfaceRendererTest)
#include "test_surface_renderer.moc"
