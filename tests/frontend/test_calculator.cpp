#include "DemoWidgets.h"

#include <QtTest>

#include <QSignalSpy>

class CalculatorTest : public QObject
{
    Q_OBJECT

private slots:
    void predefinedButtonsCalculateLocally_data();
    void predefinedButtonsCalculateLocally();
    void invalidInputsDoNotCreateRecords_data();
    void invalidInputsDoNotCreateRecords();
};

void CalculatorTest::predefinedButtonsCalculateLocally_data()
{
    QTest::addColumn<QStringList>("buttons");
    QTest::addColumn<QString>("expression");
    QTest::addColumn<double>("result");

    QTest::newRow("addition") << (QStringList() << "7" << "+" << "5" << "=") << QString("7+5") << 12.0;
    QTest::newRow("subtraction") << (QStringList() << "7" << "-" << "9" << "=") << QString("7-9") << -2.0;
    QTest::newRow("multiplication") << (QStringList() << "6" << "×" << "7" << "=") << QString("6×7") << 42.0;
    QTest::newRow("division") << (QStringList() << "8" << "÷" << "2" << "=") << QString("8÷2") << 4.0;
}

void CalculatorTest::predefinedButtonsCalculateLocally()
{
    QFETCH(QStringList, buttons);
    QFETCH(QString, expression);
    QFETCH(double, result);

    a2ui::Calculator calculator;
    QSignalSpy successfulCalculations(&calculator, SIGNAL(calculationSucceeded(QString,double)));
    for (int index = 0; index < buttons.size(); ++index) {
        calculator.pressButton(buttons.at(index));
    }

    QCOMPARE(successfulCalculations.count(), 1);
    QCOMPARE(successfulCalculations.at(0).at(0).toString(), expression);
    QCOMPARE(successfulCalculations.at(0).at(1).toDouble(), result);
    QCOMPARE(calculator.inputText(), QString::number(result, 'g', 15));
    QCOMPARE(calculator.statusText(), QStringLiteral("计算成功"));
}

void CalculatorTest::invalidInputsDoNotCreateRecords_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("button");
    QTest::addColumn<QString>("status");

    QTest::newRow("division-by-zero") << QString("8÷0") << QString("=") << QStringLiteral("除数不能为零");
    QTest::newRow("malformed") << QString("1++2") << QString("=") << QStringLiteral("非法输入");
    QTest::newRow("script") << QString("system('calc')") << QString("=") << QStringLiteral("非法输入");
    QTest::newRow("unknown-token") << QString("1") << QString(";") << QStringLiteral("不支持的按钮");
}

void CalculatorTest::invalidInputsDoNotCreateRecords()
{
    QFETCH(QString, input);
    QFETCH(QString, button);
    QFETCH(QString, status);

    a2ui::Calculator calculator;
    QSignalSpy successfulCalculations(&calculator, SIGNAL(calculationSucceeded(QString,double)));
    calculator.setInputText(input);
    calculator.pressButton(button);

    QCOMPARE(successfulCalculations.count(), 0);
    QCOMPARE(calculator.inputText(), input);
    QCOMPARE(calculator.statusText(), status);
}

QTEST_MAIN(CalculatorTest)

#include "test_calculator.moc"
