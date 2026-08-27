#include "CalculationRecord.h"

#include <QJsonValue>
#include <QStringList>

namespace a2ui {
namespace {

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
}

bool hasExactKeys(const QJsonObject &object,
                  const QStringList &keys,
                  QString *errorMessage)
{
    if (object.keys() != keys) {
        setError(errorMessage, QStringLiteral("Calculation contract fields do not match"));
        return false;
    }
    return true;
}

} // namespace

CalculationRecord::CalculationRecord()
    : result(0.0)
{
}

QJsonObject CalculationRecord::toJson() const
{
    QJsonObject object;
    object.insert(QStringLiteral("createdAt"), createdAt);
    object.insert(QStringLiteral("expression"), expression);
    object.insert(QStringLiteral("id"), id);
    object.insert(QStringLiteral("note"), note);
    object.insert(QStringLiteral("result"), result);
    object.insert(QStringLiteral("updatedAt"), updatedAt);
    return object;
}

bool CalculationRecord::fromJson(const QJsonObject &object,
                                 CalculationRecord *record,
                                 QString *errorMessage)
{
    const QStringList keys = QStringList()
        << QStringLiteral("createdAt") << QStringLiteral("expression")
        << QStringLiteral("id") << QStringLiteral("note")
        << QStringLiteral("result") << QStringLiteral("updatedAt");
    if (!record || !hasExactKeys(object, keys, errorMessage)) {
        return false;
    }
    if (!object.value(QStringLiteral("id")).isString()
        || !object.value(QStringLiteral("expression")).isString()
        || !object.value(QStringLiteral("result")).isDouble()
        || !object.value(QStringLiteral("note")).isString()
        || !object.value(QStringLiteral("createdAt")).isString()
        || !object.value(QStringLiteral("updatedAt")).isString()) {
        setError(errorMessage, QStringLiteral("Calculation contract field type is invalid"));
        return false;
    }

    record->id = object.value(QStringLiteral("id")).toString();
    record->expression = object.value(QStringLiteral("expression")).toString();
    record->result = object.value(QStringLiteral("result")).toDouble();
    record->note = object.value(QStringLiteral("note")).toString();
    record->createdAt = object.value(QStringLiteral("createdAt")).toString();
    record->updatedAt = object.value(QStringLiteral("updatedAt")).toString();
    return true;
}

CalculationSummary::CalculationSummary()
    : count(0), hasLatest(false)
{
}

bool CalculationSummary::fromJson(const QJsonObject &object,
                                  CalculationSummary *summary,
                                  QString *errorMessage)
{
    const QStringList keys = QStringList()
        << QStringLiteral("count") << QStringLiteral("latest");
    if (!summary || !hasExactKeys(object, keys, errorMessage)) {
        return false;
    }
    if (!object.value(QStringLiteral("count")).isDouble()) {
        setError(errorMessage, QStringLiteral("Summary count must be a number"));
        return false;
    }

    const double count = object.value(QStringLiteral("count")).toDouble();
    if (count < 0 || count != static_cast<int>(count)) {
        setError(errorMessage, QStringLiteral("Summary count must be a non-negative integer"));
        return false;
    }
    summary->count = static_cast<int>(count);

    const QJsonValue latestValue = object.value(QStringLiteral("latest"));
    if (latestValue.isNull()) {
        summary->hasLatest = false;
        summary->latest = CalculationRecord();
        return true;
    }
    if (!latestValue.isObject()
        || !CalculationRecord::fromJson(latestValue.toObject(),
                                        &summary->latest,
                                        errorMessage)) {
        if (latestValue.isObject()) {
            return false;
        }
        setError(errorMessage, QStringLiteral("Summary latest must be a record or null"));
        return false;
    }
    summary->hasLatest = true;
    return true;
}

} // namespace a2ui
