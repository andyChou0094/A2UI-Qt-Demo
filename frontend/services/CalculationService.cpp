#include "CalculationService.h"

#include <QCoreApplication>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrlQuery>

namespace a2ui {
namespace {

void setError(QString *errorMessage, const QString &message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
}

bool isLoopbackHost(const QString &host)
{
    if (host.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) == 0) {
        return true;
    }
    const QHostAddress address(host);
    return !address.isNull() && address.isLoopback();
}

} // namespace

CalculationServiceConfig::CalculationServiceConfig()
    : baseUrl(QStringLiteral("http://127.0.0.1:8000")),
      timeoutMilliseconds(3000)
{
}

CalculationServiceConfig::CalculationServiceConfig(const QUrl &url,
                                                   int timeout)
    : baseUrl(url), timeoutMilliseconds(timeout)
{
}

CalculationServiceConfig CalculationServiceConfig::fromApplication()
{
    CalculationServiceConfig config;
    QCoreApplication *application = QCoreApplication::instance();
    if (!application) {
        return config;
    }
    const QVariant url = application->property("a2ui.calculationApiBaseUrl");
    const QVariant timeout = application->property("a2ui.calculationApiTimeoutMs");
    if (url.isValid()) {
        config.baseUrl = QUrl(url.toString());
    }
    if (timeout.isValid()) {
        config.timeoutMilliseconds = timeout.toInt();
    }
    return config;
}

bool CalculationServiceConfig::validate(QString *errorMessage) const
{
    if (!baseUrl.isValid() || baseUrl.scheme() != QStringLiteral("http")
        || !isLoopbackHost(baseUrl.host())) {
        setError(errorMessage,
                 QStringLiteral("Calculation API must use HTTP on a local loopback host"));
        return false;
    }
    if (!baseUrl.userInfo().isEmpty() || !baseUrl.query().isEmpty()
        || !baseUrl.fragment().isEmpty()) {
        setError(errorMessage,
                 QStringLiteral("Calculation API base URL must not contain credentials, query, or fragment"));
        return false;
    }
    if (timeoutMilliseconds <= 0) {
        setError(errorMessage, QStringLiteral("Calculation API timeout must be positive"));
        return false;
    }
    return true;
}

CalculationService::CalculationService(const CalculationServiceConfig &config,
                                       QObject *parent)
    : QObject(parent), config_(config), network_(new QNetworkAccessManager(this))
{
    config_.validate(&configurationError_);
    qRegisterMetaType<a2ui::CalculationRecord>();
    qRegisterMetaType<QList<a2ui::CalculationRecord> >();
    qRegisterMetaType<a2ui::CalculationSummary>();
    qRegisterMetaType<a2ui::CalculationService::Operation>();
}

bool CalculationService::isConfigured() const { return configurationError_.isEmpty(); }
QString CalculationService::configurationError() const { return configurationError_; }

bool CalculationService::createRecord(const QString &expression, double result)
{
    QJsonObject object;
    object.insert(QStringLiteral("expression"), expression);
    object.insert(QStringLiteral("result"), result);
    return send(CreateRecord,
                QByteArrayLiteral("POST"),
                endpoint(QStringLiteral("/api/calculations")),
                QJsonDocument(object));
}

bool CalculationService::fetchRecords(int limit)
{
    if (limit < 1 || limit > 50) {
        fail(FetchRecords, QStringLiteral("Calculation query limit must be between 1 and 50"));
        return false;
    }
    QUrl url = endpoint(QStringLiteral("/api/calculations"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("limit"), QString::number(limit));
    url.setQuery(query);
    return send(FetchRecords, QByteArrayLiteral("GET"), url);
}

bool CalculationService::updateNote(const QString &id, const QString &note)
{
    QJsonObject object;
    object.insert(QStringLiteral("note"), note);
    return send(UpdateNote,
                QByteArrayLiteral("PATCH"),
                endpoint(QStringLiteral("/api/calculations/%1").arg(id)),
                QJsonDocument(object));
}

bool CalculationService::deleteRecord(const QString &id)
{
    return send(DeleteRecord,
                QByteArrayLiteral("DELETE"),
                endpoint(QStringLiteral("/api/calculations/%1").arg(id)));
}

bool CalculationService::fetchSummary()
{
    return send(FetchSummary,
                QByteArrayLiteral("GET"),
                endpoint(QStringLiteral("/api/calculations/summary")));
}

QUrl CalculationService::endpoint(const QString &path) const
{
    QUrl url(config_.baseUrl);
    url.setPath(path);
    url.setQuery(QString());
    url.setFragment(QString());
    return url;
}

bool CalculationService::send(Operation operation,
                              const QByteArray &method,
                              const QUrl &url,
                              const QJsonDocument &payload)
{
    if (!isConfigured()) {
        fail(operation, configurationError_);
        return false;
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    QNetworkReply *reply = network_->sendCustomRequest(
        request, method, payload.isNull() ? QByteArray() : payload.toJson(QJsonDocument::Compact));
    reply->setProperty("a2uiTimedOut", false);
    const QString resourceId = url.path().section(QLatin1Char('/'), -1);

    QTimer *timer = new QTimer(reply);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, reply, [reply]() {
        reply->setProperty("a2uiTimedOut", true);
        reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this,
            [this, operation, resourceId, reply, timer]() {
        timer->stop();
        handleFinished(operation, resourceId, reply);
        reply->deleteLater();
    });
    timer->start(config_.timeoutMilliseconds);
    return true;
}

void CalculationService::handleFinished(Operation operation,
                                        const QString &resourceId,
                                        QNetworkReply *reply)
{
    if (reply->property("a2uiTimedOut").toBool()) {
        fail(operation, QStringLiteral("Calculation API request timed out"));
        return;
    }
    if (reply->error() != QNetworkReply::NoError) {
        fail(operation, reply->errorString());
        return;
    }
    const int status = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status < 200 || status >= 300) {
        fail(operation, QStringLiteral("Calculation API returned HTTP %1").arg(status));
        return;
    }

    if (operation == DeleteRecord) {
        emit recordDeleted(resourceId);
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        fail(operation, QStringLiteral("Calculation API returned invalid JSON"));
        return;
    }

    QString contractError;
    if (operation == FetchRecords) {
        if (!document.isArray()) {
            fail(operation, QStringLiteral("Calculation list response must be an array"));
            return;
        }
        QList<CalculationRecord> records;
        const QJsonArray values = document.array();
        for (int index = 0; index < values.size(); ++index) {
            CalculationRecord record;
            if (!values.at(index).isObject()
                || !CalculationRecord::fromJson(values.at(index).toObject(),
                                                &record,
                                                &contractError)) {
                fail(operation, contractError.isEmpty()
                    ? QStringLiteral("Calculation list item must be an object")
                    : contractError);
                return;
            }
            records.append(record);
        }
        emit recordsFetched(records);
        return;
    }

    if (!document.isObject()) {
        fail(operation, QStringLiteral("Calculation response must be an object"));
        return;
    }
    if (operation == FetchSummary) {
        CalculationSummary summary;
        if (!CalculationSummary::fromJson(document.object(), &summary, &contractError)) {
            fail(operation, contractError);
            return;
        }
        emit summaryFetched(summary);
        return;
    }

    CalculationRecord record;
    if (!CalculationRecord::fromJson(document.object(), &record, &contractError)) {
        fail(operation, contractError);
        return;
    }
    if (operation == CreateRecord) {
        emit recordCreated(record);
    } else {
        emit recordUpdated(record);
    }
}

void CalculationService::fail(Operation operation, const QString &message)
{
    emit requestFailed(operation, message);
}

} // namespace a2ui
