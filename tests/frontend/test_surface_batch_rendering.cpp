#include "HostShell.h"
#include "WidgetRegistry.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QScrollArea>
#include <QScrollBar>
#include <QStringList>
#include <QtTest>

namespace {

QStringList *gCriticalWarnings = 0;

void captureCriticalWarnings(QtMsgType type, const QMessageLogContext &, const QString &message)
{
    if (gCriticalWarnings
            && (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg)
            && (message.contains(QStringLiteral("QLayout"))
                || message.contains(QStringLiteral("QObject::"))
                || message.contains(QStringLiteral("QWidget::"))
                || message.contains(QStringLiteral("ASSERT"), Qt::CaseInsensitive)
                || message.contains(QStringLiteral("cannot"), Qt::CaseInsensitive))) {
        gCriticalWarnings->append(message);
    }
}

} // namespace

class SurfaceBatchRenderingTest : public QObject
{
    Q_OBJECT

private slots:
    void appliesEveryGeneratedSurfaceInOneHostShell()
    {
        QProcess generator;
        generator.start(QStringLiteral(A2UI_PYTHON_EXECUTABLE), QStringList()
            << QStringLiteral(A2UI_SOURCE_DIR "/scripts/generate_legal_surfaces.py")
            << QStringLiteral("--seed") << QStringLiteral("20260821"));
        QVERIFY(generator.waitForFinished(10000));
        QCOMPARE(generator.exitCode(), 0);
        const QJsonDocument corpus = QJsonDocument::fromJson(generator.readAllStandardOutput());
        QVERIFY(corpus.isObject());
        const QJsonArray samples = corpus.object().value(QStringLiteral("samples")).toArray();
        QVERIFY(samples.size() >= 20);

        a2ui::WidgetRegistry registry;
        const QStringList types = QStringList()
            << QStringLiteral("Calculator") << QStringLiteral("CalculationHistory")
            << QStringLiteral("CalculationStats") << QStringLiteral("Clock")
            << QStringLiteral("NotePad");
        for (QStringList::const_iterator it = types.constBegin(); it != types.constEnd(); ++it) {
            const QString type = *it;
            QVERIFY(registry.registerFactory(type, [type](QWidget *parent) {
                QWidget *widget = new QWidget(parent);
                widget->setProperty("componentType", type);
                widget->setMinimumSize(80, 60);
                return widget;
            }));
        }
        a2ui::HostShell shell(registry);
        shell.resize(900, 700);
        shell.show();
        QStringList criticalWarnings;
        gCriticalWarnings = &criticalWarnings;
        const QtMessageHandler previousHandler = qInstallMessageHandler(captureCriticalWarnings);

        for (int index = 0; index < samples.size(); ++index) {
            criticalWarnings.clear();
            const QJsonObject sample = samples.at(index).toObject();
            const QJsonObject surface = sample.value(QStringLiteral("surface")).toObject();
            const QByteArray bytes = QJsonDocument(surface).toJson(QJsonDocument::Compact);
            QVERIFY2(shell.applySurface(bytes).applied,
                     qPrintable(sample.value(QStringLiteral("id")).toString()));
            QCoreApplication::processEvents();
            int expectedLeaves = 0;
            QList<QWidget *> activeLeaves;
            const QJsonArray nodes = surface.value(QStringLiteral("nodes")).toArray();
            for (int nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
                const QJsonObject node = nodes.at(nodeIndex).toObject();
                const QString type = node.value(QStringLiteral("type")).toString();
                if (type == QStringLiteral("Row") || type == QStringLiteral("Column")) {
                    continue;
                }
                ++expectedLeaves;
                QWidget *leaf = 0;
                const QList<QWidget *> widgets = shell.surfaceHost()->findChildren<QWidget *>();
                for (QList<QWidget *>::const_iterator it = widgets.constBegin(); it != widgets.constEnd(); ++it) {
                    if ((*it)->property("a2uiNodeId").toString()
                            == node.value(QStringLiteral("id")).toString()) {
                        leaf = *it;
                        break;
                    }
                }
                QVERIFY(leaf != 0);
                QVERIFY(leaf->width() > 0);
                QVERIFY(leaf->height() > 0);
                activeLeaves.append(leaf);
            }
            for (int left = 0; left < activeLeaves.size(); ++left) {
                const QRect leftRect(activeLeaves.at(left)->mapTo(shell.surfaceHost(), QPoint(0, 0)),
                                     activeLeaves.at(left)->size());
                for (int right = left + 1; right < activeLeaves.size(); ++right) {
                    const QRect rightRect(activeLeaves.at(right)->mapTo(shell.surfaceHost(), QPoint(0, 0)),
                                          activeLeaves.at(right)->size());
                    const QRect overlap = leftRect.intersected(rightRect);
                    QVERIFY2(overlap.width() <= 1 || overlap.height() <= 1,
                             qPrintable(sample.value(QStringLiteral("id")).toString()));
                }
            }
            int actualLeaves = 0;
            const QList<QWidget *> widgets = shell.surfaceHost()->findChildren<QWidget *>();
            for (QList<QWidget *>::const_iterator it = widgets.constBegin(); it != widgets.constEnd(); ++it) {
                if (!(*it)->property("componentType").toString().isEmpty()) {
                    ++actualLeaves;
                }
            }
            QCOMPARE(actualLeaves, expectedLeaves);
            QVERIFY(shell.surfaceScrollArea()->widgetResizable());
            QVERIFY(shell.surfaceScrollArea()->horizontalScrollBar()->maximum() >= 0);
            QVERIFY(shell.surfaceScrollArea()->verticalScrollBar()->maximum() >= 0);
            QVERIFY2(criticalWarnings.isEmpty(), qPrintable(criticalWarnings.join(QStringLiteral("\n"))));
        }
        qInstallMessageHandler(previousHandler);
        gCriticalWarnings = 0;
    }
};

QTEST_MAIN(SurfaceBatchRenderingTest)
#include "test_surface_batch_rendering.moc"
