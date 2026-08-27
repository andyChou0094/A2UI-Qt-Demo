#ifndef A2UI_CALCULATION_RECORD_H
#define A2UI_CALCULATION_RECORD_H

#include <QJsonObject>
#include <QList>
#include <QMetaType>
#include <QString>

namespace a2ui {

struct CalculationRecord
{
    QString id;
    QString expression;
    double result;
    QString note;
    QString createdAt;
    QString updatedAt;

    CalculationRecord();

    QJsonObject toJson() const;
    static bool fromJson(const QJsonObject &object,
                         CalculationRecord *record,
                         QString *errorMessage = 0);
};

struct CalculationSummary
{
    int count;
    bool hasLatest;
    CalculationRecord latest;

    CalculationSummary();

    static bool fromJson(const QJsonObject &object,
                         CalculationSummary *summary,
                         QString *errorMessage = 0);
};

} // namespace a2ui

Q_DECLARE_METATYPE(a2ui::CalculationRecord)
Q_DECLARE_METATYPE(QList<a2ui::CalculationRecord>)
Q_DECLARE_METATYPE(a2ui::CalculationSummary)

#endif
