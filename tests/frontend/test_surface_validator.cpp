#include "SurfaceValidator.h"
#include "TestSourceRoot.h"

#include <QDir>
#include <QFile>
#include <QFileInfoList>
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

QFileInfoList fixtures(const QString &classification)
{
    const QString path = a2ui_test::path(
        QStringLiteral("shared/fixtures/surface-spec/") + classification);
    return QDir(path).entryInfoList(QStringList() << QStringLiteral("*.json"),
                                    QDir::Files, QDir::Name);
}

} // namespace

class SurfaceValidatorTest : public QObject
{
    Q_OBJECT

private slots:
    void resolvesSourceRootWithoutWorkspaceMapping()
    {
        const QString root = a2ui_test::sourceRoot();
        QVERIFY2(!root.isEmpty(), "未能在运行时解析测试源码根");
        QVERIFY(a2ui_test::isSourceRoot(root));
        QVERIFY(QFileInfo(a2ui_test::path(
            QStringLiteral("shared/schema/surface-spec-v0.schema.json"))).isFile());
    }

    void acceptsEverySharedValidFixture()
    {
        const QFileInfoList files = fixtures(QStringLiteral("valid"));
        QCOMPARE(files.size(), 7);
        const a2ui::SurfaceValidator validator;
        for (QFileInfoList::const_iterator it = files.constBegin(); it != files.constEnd(); ++it) {
            const a2ui::SurfaceValidationResult result = validator.validate(readFile(it->absoluteFilePath()));
            QVERIFY2(result.isValid, qPrintable(it->fileName() + QStringLiteral(": ")
                                                + result.diagnostics.join(QStringLiteral("; "))));
        }
    }

    void rejectsEverySharedInvalidFixture()
    {
        const QFileInfoList files = fixtures(QStringLiteral("invalid"));
        QVERIFY(files.size() >= 11);
        const a2ui::SurfaceValidator validator;
        for (QFileInfoList::const_iterator it = files.constBegin(); it != files.constEnd(); ++it) {
            const a2ui::SurfaceValidationResult result = validator.validate(readFile(it->absoluteFilePath()));
            QVERIFY2(!result.isValid, qPrintable(it->fileName()));
            QVERIFY2(!result.diagnostics.isEmpty(), qPrintable(it->fileName()));
        }
    }

    void enforcesConfiguredMultiplicity()
    {
        QMap<QString, bool> catalog = a2ui::SurfaceValidator::demoCatalog();
        catalog[QStringLiteral("Calculator")] = false;
        const a2ui::SurfaceValidator validator(catalog);
        const QString path = a2ui_test::path(
            QStringLiteral("shared/fixtures/surface-spec/valid/duplicate-calculators.json"));
        const a2ui::SurfaceValidationResult result = validator.validate(readFile(path));
        QVERIFY(!result.isValid);
        QVERIFY(result.diagnostics.join(QStringLiteral(" ")).contains(QStringLiteral("multiple")));
    }
};

QTEST_MAIN(SurfaceValidatorTest)
#include "test_surface_validator.moc"
