#ifndef A2UI_COMPOSITION_CLIENT_H
#define A2UI_COMPOSITION_CLIENT_H

#include <QByteArray>
#include <QObject>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

namespace a2ui {

class SurfaceRenderer;

class CompositionClientConfig
{
public:
    CompositionClientConfig();
    CompositionClientConfig(const QUrl &baseUrl, int timeoutMilliseconds);

    static CompositionClientConfig fromApplication();
    bool validate(QString *errorMessage = 0) const;

    QUrl baseUrl;
    int timeoutMilliseconds;
};

class CompositionClient : public QObject
{
    Q_OBJECT
public:
    CompositionClient(const CompositionClientConfig &config,
                      SurfaceRenderer *renderer,
                      QObject *parent = 0);

    bool isConfigured() const;
    QString configurationError() const;
    bool compose(const QString &prompt, const QByteArray &currentSurfaceJson);

signals:
    void compositionApplied(const QByteArray &surfaceJson);
    void compositionFailed(const QString &code, const QString &message);

private:
    QUrl endpoint() const;
    void handleFinished(QNetworkReply *reply);
    void fail(const QString &code, const QString &message);

    CompositionClientConfig config_;
    SurfaceRenderer *renderer_;
    QNetworkAccessManager *network_;
    QString configurationError_;
};

} // namespace a2ui

#endif
