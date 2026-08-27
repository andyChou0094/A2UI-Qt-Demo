#include "SurfaceDocumentClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QVariant>

namespace a2ui {

SurfaceDocumentClient::SurfaceDocumentClient(const CompositionClientConfig &config,
                                             QObject *parent)
    : QObject(parent), config_(config), network_(new QNetworkAccessManager(this))
{
    config_.validate(&configurationError_);
}

bool SurfaceDocumentClient::isConfigured() const { return configurationError_.isEmpty(); }
QString SurfaceDocumentClient::configurationError() const { return configurationError_; }

bool SurfaceDocumentClient::importDocument(const QByteArray &document)
{
    return send(QStringLiteral("import"), QStringLiteral("/surface/import"), document, false);
}

bool SurfaceDocumentClient::exportDocument(const QByteArray &document)
{
    return send(QStringLiteral("export"), QStringLiteral("/surface/export"), document, false);
}

bool SurfaceDocumentClient::requestDefault()
{
    return send(QStringLiteral("default"), QStringLiteral("/surface/default"), QByteArray(), true);
}

bool SurfaceDocumentClient::send(const QString &operation, const QString &path,
                                 const QByteArray &body, bool useGet)
{
    if (!isConfigured()) {
        fail(operation, QStringLiteral("configuration_error"), configurationError_);
        return false;
    }
    QUrl url(config_.baseUrl);
    url.setPath(path);
    url.setQuery(QString());
    url.setFragment(QString());
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QNetworkReply *reply = useGet ? network_->get(request) : network_->post(request, body);
    reply->setProperty("a2uiOperation", operation);
    reply->setProperty("a2uiTimedOut", false);
    QTimer *timer = new QTimer(reply);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, reply, [reply]() {
        reply->setProperty("a2uiTimedOut", true);
        reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, timer]() {
        timer->stop();
        handleFinished(reply);
        reply->deleteLater();
    });
    timer->start(config_.timeoutMilliseconds);
    return true;
}

void SurfaceDocumentClient::handleFinished(QNetworkReply *reply)
{
    const QString operation = reply->property("a2uiOperation").toString();
    if (reply->property("a2uiTimedOut").toBool()) {
        fail(operation, QStringLiteral("timeout"), QStringLiteral("Surface document request timed out"));
        return;
    }
    const QByteArray bytes = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
        QJsonParseError parseError;
        const QJsonObject response = QJsonDocument::fromJson(bytes, &parseError).object();
        const QJsonObject error = response.value(QStringLiteral("error")).toObject();
        fail(operation,
             error.value(QStringLiteral("code")).toString(QStringLiteral("surface_document_failed")),
             error.value(QStringLiteral("message")).toString(reply->errorString()));
        return;
    }
    if (operation == QStringLiteral("export")) {
        QString fileName = QStringLiteral("surface-main.json");
        const QString disposition = QString::fromLatin1(reply->rawHeader("Content-Disposition"));
        const QRegularExpression expression(QStringLiteral("filename=\\\"?([^\\\";]+)"));
        const QRegularExpressionMatch match = expression.match(disposition);
        if (match.hasMatch()) {
            fileName = match.captured(1);
        }
        emit exportReady(bytes, fileName);
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument response = QJsonDocument::fromJson(bytes, &parseError);
    const QJsonObject object = response.object();
    if (parseError.error != QJsonParseError::NoError || !response.isObject()
            || object.size() != 1 || !object.value(QStringLiteral("surface")).isObject()) {
        fail(operation, QStringLiteral("invalid_response"),
             QStringLiteral("Surface document API returned an invalid response"));
        return;
    }
    const QByteArray surface = QJsonDocument(
        object.value(QStringLiteral("surface")).toObject()).toJson(QJsonDocument::Compact);
    if (operation == QStringLiteral("import")) {
        emit importValidated(surface);
    } else {
        emit defaultReady(surface);
    }
}

void SurfaceDocumentClient::fail(const QString &operation, const QString &code,
                                 const QString &message)
{
    emit documentFailed(operation, code, message);
}

} // namespace a2ui
