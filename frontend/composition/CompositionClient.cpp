#include "CompositionClient.h"

#include "SurfaceRenderer.h"

#include <QCoreApplication>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QVariant>

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

namespace a2ui {

CompositionClientConfig::CompositionClientConfig()
    : baseUrl(QStringLiteral("http://127.0.0.1:8000")),
      timeoutMilliseconds(120000)
{
}

CompositionClientConfig::CompositionClientConfig(const QUrl &url, int timeout)
    : baseUrl(url), timeoutMilliseconds(timeout)
{
}

CompositionClientConfig CompositionClientConfig::fromApplication()
{
    CompositionClientConfig config;
    QCoreApplication *application = QCoreApplication::instance();
    if (!application) {
        return config;
    }
    const QVariant url = application->property("a2ui.compositionApiBaseUrl");
    const QVariant timeout = application->property("a2ui.compositionApiTimeoutMs");
    if (url.isValid()) {
        config.baseUrl = QUrl(url.toString());
    }
    if (timeout.isValid()) {
        config.timeoutMilliseconds = timeout.toInt();
    }
    return config;
}

bool CompositionClientConfig::validate(QString *errorMessage) const
{
    if (!baseUrl.isValid() || baseUrl.scheme() != QStringLiteral("http")
            || !isLoopbackHost(baseUrl.host())) {
        setError(errorMessage,
                 QStringLiteral("Composition API must use HTTP on a local loopback host"));
        return false;
    }
    if (!baseUrl.userInfo().isEmpty() || !baseUrl.query().isEmpty()
            || !baseUrl.fragment().isEmpty()) {
        setError(errorMessage,
                 QStringLiteral("Composition API base URL must not contain credentials, query, or fragment"));
        return false;
    }
    if (timeoutMilliseconds <= 0) {
        setError(errorMessage, QStringLiteral("Composition API timeout must be positive"));
        return false;
    }
    return true;
}

CompositionClient::CompositionClient(const CompositionClientConfig &config,
                                     SurfaceRenderer *renderer,
                                     QObject *parent)
    : QObject(parent),
      config_(config),
      renderer_(renderer),
      network_(new QNetworkAccessManager(this))
{
    config_.validate(&configurationError_);
    if (!renderer_ && configurationError_.isEmpty()) {
        configurationError_ = QStringLiteral("Composition renderer is unavailable");
    }
}

bool CompositionClient::isConfigured() const
{
    return configurationError_.isEmpty();
}

QString CompositionClient::configurationError() const
{
    return configurationError_;
}

bool CompositionClient::compose(const QString &prompt, const QByteArray &currentSurfaceJson)
{
    if (!isConfigured()) {
        fail(QStringLiteral("configuration_error"), configurationError_);
        return false;
    }
    if (prompt.trimmed().isEmpty()) {
        fail(QStringLiteral("invalid_request"), QStringLiteral("Prompt must not be empty"));
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument current = QJsonDocument::fromJson(currentSurfaceJson, &parseError);
    if (parseError.error != QJsonParseError::NoError || !current.isObject()) {
        fail(QStringLiteral("invalid_current_surface"),
             QStringLiteral("Current Surface must be a JSON object"));
        return false;
    }

    QJsonObject body;
    body.insert(QStringLiteral("prompt"), prompt);
    body.insert(QStringLiteral("currentSurface"), current.object());
    QNetworkRequest request(endpoint());
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QNetworkReply *reply = network_->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
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

QUrl CompositionClient::endpoint() const
{
    QUrl url(config_.baseUrl);
    url.setPath(QStringLiteral("/compose"));
    url.setQuery(QString());
    url.setFragment(QString());
    return url;
}

void CompositionClient::handleFinished(QNetworkReply *reply)
{
    if (reply->property("a2uiTimedOut").toBool()) {
        fail(QStringLiteral("timeout"), QStringLiteral("Composition request timed out"));
        return;
    }
    const QByteArray responseBytes = reply->readAll();
    QJsonParseError parseError;
    const QJsonDocument response = QJsonDocument::fromJson(responseBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !response.isObject()) {
        fail(QStringLiteral("invalid_response"),
             QStringLiteral("Composition API returned invalid JSON"));
        return;
    }
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QJsonObject object = response.object();
    if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
        const QJsonObject error = object.value(QStringLiteral("error")).toObject();
        const QString code = error.value(QStringLiteral("code")).toString(
            QStringLiteral("composition_failed"));
        const QString message = error.value(QStringLiteral("message")).toString(
            reply->errorString());
        fail(code, message);
        return;
    }
    if (object.size() != 1 || !object.value(QStringLiteral("surface")).isObject()) {
        fail(QStringLiteral("invalid_response"),
             QStringLiteral("Composition response must contain only a complete surface"));
        return;
    }
    const QByteArray surface = QJsonDocument(
        object.value(QStringLiteral("surface")).toObject()).toJson(QJsonDocument::Compact);
    const SurfaceApplyResult result = renderer_->apply(surface);
    if (!result.applied) {
        fail(QStringLiteral("invalid_surface"), result.diagnostics.join(QStringLiteral("; ")));
        return;
    }
    emit compositionApplied(surface);
}

void CompositionClient::fail(const QString &code, const QString &message)
{
    emit compositionFailed(code, message);
}

} // namespace a2ui
