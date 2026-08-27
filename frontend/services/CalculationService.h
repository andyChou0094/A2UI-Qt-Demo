#ifndef A2UI_CALCULATION_SERVICE_H
#define A2UI_CALCULATION_SERVICE_H

#include "CalculationRecord.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QObject>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

namespace a2ui {

class CalculationServiceConfig
{
public:
    CalculationServiceConfig();
    CalculationServiceConfig(const QUrl &baseUrl, int timeoutMilliseconds);

    static CalculationServiceConfig fromApplication();
    bool validate(QString *errorMessage = 0) const;

    QUrl baseUrl;
    int timeoutMilliseconds;
};

class CalculationService : public QObject
{
    Q_OBJECT
public:
    enum Operation {
        CreateRecord,
        FetchRecords,
        UpdateNote,
        DeleteRecord,
        FetchSummary
    };
    Q_ENUM(Operation)

    explicit CalculationService(const CalculationServiceConfig &config,
                                QObject *parent = 0);

    bool isConfigured() const;
    QString configurationError() const;

    bool createRecord(const QString &expression, double result);
    bool fetchRecords(int limit = 50);
    bool updateNote(const QString &id, const QString &note);
    bool deleteRecord(const QString &id);
    bool fetchSummary();

signals:
    void recordCreated(const a2ui::CalculationRecord &record);
    void recordsFetched(const QList<a2ui::CalculationRecord> &records);
    void recordUpdated(const a2ui::CalculationRecord &record);
    void recordDeleted(const QString &id);
    void summaryFetched(const a2ui::CalculationSummary &summary);
    void requestFailed(a2ui::CalculationService::Operation operation,
                       const QString &message);

private:
    QUrl endpoint(const QString &path) const;
    bool send(Operation operation,
              const QByteArray &method,
              const QUrl &url,
              const QJsonDocument &payload = QJsonDocument());
    void handleFinished(Operation operation,
                        const QString &resourceId,
                        QNetworkReply *reply);
    void fail(Operation operation, const QString &message);

    CalculationServiceConfig config_;
    QNetworkAccessManager *network_;
    QString configurationError_;
};

} // namespace a2ui

Q_DECLARE_METATYPE(a2ui::CalculationService::Operation)

#endif
