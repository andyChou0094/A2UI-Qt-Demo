#include "WidgetRegistry.h"

#include <QtTest>
#include <QLabel>
#include <QWidget>

class WidgetRegistryTest : public QObject
{
    Q_OBJECT

private slots:
    void createsOpaqueEmbeddedWidget()
    {
        a2ui::WidgetRegistry registry;
        QString error;
        QVERIFY2(registry.registerFactory(
                     QStringLiteral("NotePad"),
                     [](QWidget *parent) { return new QWidget(parent); },
                     &error),
                 qPrintable(error));

        QWidget parent;
        QWidget *widget = registry.create(QStringLiteral("NotePad"), &parent, &error);
        QVERIFY2(widget, qPrintable(error));
        QCOMPARE(widget->parentWidget(), &parent);
        QVERIFY(!widget->isWindow());
    }

    void rejectsDuplicateAndUnknownTypes()
    {
        a2ui::WidgetRegistry registry;
        const a2ui::WidgetRegistry::Factory factory =
            [](QWidget *parent) { return new QWidget(parent); };
        QVERIFY(registry.registerFactory(QStringLiteral("Clock"), factory));
        QVERIFY(!registry.registerFactory(QStringLiteral("Clock"), factory));

        QWidget parent;
        QVERIFY(!registry.create(QStringLiteral("Missing"), &parent));
    }

    void rejectsTopLevelAndWrongParentFactories()
    {
        a2ui::WidgetRegistry registry;
        QVERIFY(registry.registerFactory(
            QStringLiteral("TopLevel"),
            [](QWidget *) { return new QWidget(0, Qt::Window); }));
        QVERIFY(registry.registerFactory(
            QStringLiteral("WrongParent"),
            [](QWidget *) { return new QWidget; }));

        QWidget parent;
        QVERIFY(!registry.create(QStringLiteral("TopLevel"), &parent));
        QVERIFY(!registry.create(QStringLiteral("WrongParent"), &parent));
    }

    void recordsProjectMaintainedAdapterWithoutInspectingChildren()
    {
        a2ui::WidgetRegistry registry;
        QVERIFY(registry.registerAdapterFactory(
            QStringLiteral("LegacyReport"),
            [](QWidget *parent) {
                QWidget *adapter = new QWidget(parent);
                new QLabel(QStringLiteral("legacy internals"), adapter);
                return adapter;
            }));

        bool found = false;
        QCOMPARE(registry.registrationKind(QStringLiteral("LegacyReport"), &found),
                 a2ui::WidgetRegistry::ProjectMaintainedAdapter);
        QVERIFY(found);

        QWidget firstParent;
        QWidget secondParent;
        QWidget *adapter = registry.create(QStringLiteral("LegacyReport"), &firstParent);
        QVERIFY(adapter);
        QCOMPARE(adapter->parentWidget(), &firstParent);

        adapter->resize(320, 180);
        QCOMPARE(adapter->size(), QSize(320, 180));
        adapter->setParent(&secondParent);
        QCOMPARE(adapter->parentWidget(), &secondParent);
    }
};

QTEST_MAIN(WidgetRegistryTest)

#include "test_widget_registry.moc"
