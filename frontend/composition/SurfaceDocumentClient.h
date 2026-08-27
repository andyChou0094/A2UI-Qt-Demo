#ifndef A2UI_SURFACE_DOCUMENT_CLIENT_H
#define A2UI_SURFACE_DOCUMENT_CLIENT_H

#include "CompositionClient.h"

#include <QByteArray>
#include <QObject>

class QNetworkAccessManager;
class QNetworkReply;

namespace a2ui {

class SurfaceDocumentClient : public QObject
{
    Q_OBJECT
public:
    explicit SurfaceDocumentClient(const CompositionClientConfig &config,
                                   QObject *parent = 0);

    bool isConfigured() const;
    QString configurationError() const;
    bool importDocument(const QByteArray &document);
    bool exportDocument(const QByteArray &document);
    bool requestDefault();

signals:
    void importValidated(const QByteArray &surfaceJson);
    void exportReady(const QByteArray &canonicalJson, const QString &suggestedFileName);
    void defaultReady(const QByteArray &surfaceJson);
    void documentFailed(const QString &operation, const QString &code,
                        const QString &message);

private:
    bool send(const QString &operation, const QString &path,
              const QByteArray &body, bool useGet);
    void handleFinished(QNetworkReply *reply);
    void fail(const QString &operation, const QString &code,
              const QString &message);

    CompositionClientConfig config_;
    QNetworkAccessManager *network_;
    QString configurationError_;
};

} // namespace a2ui

#endif
