#include <QtTest>

class BuildContractTest : public QObject
{
    Q_OBJECT

private slots:
    void usesPinnedQtAndLanguageMode()
    {
        QCOMPARE(QString::fromLatin1(qVersion()), QStringLiteral("5.12.8"));
        QCOMPARE(static_cast<long>(__cplusplus), 201402L);
#ifdef _GLIBCXX_USE_CXX11_ABI
        QCOMPARE(_GLIBCXX_USE_CXX11_ABI, 1);
#else
        QFAIL("_GLIBCXX_USE_CXX11_ABI is not defined");
#endif
    }
};

QTEST_APPLESS_MAIN(BuildContractTest)

#include "test_build_contract.moc"
