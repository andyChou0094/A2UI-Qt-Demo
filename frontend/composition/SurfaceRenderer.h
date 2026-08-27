#ifndef A2UI_SURFACE_RENDERER_H
#define A2UI_SURFACE_RENDERER_H

#include "SurfaceValidator.h"

#include <QByteArray>
#include <QMap>
#include <QSet>
#include <QStringList>

class QJsonObject;
class QWidget;

namespace a2ui {

class WidgetRegistry;

struct ReconciliationPlan
{
    QStringList orderedLeafIds;
    QStringList reusedIds;
    QStringList createdIds;
    QStringList replacedIds;
    QStringList removedIds;
};

struct SurfaceApplyResult
{
    SurfaceApplyResult();

    bool applied;
    QStringList diagnostics;
    ReconciliationPlan plan;
};

class SurfaceRenderer
{
public:
    SurfaceRenderer(QWidget *host, const WidgetRegistry *registry,
                    const SurfaceValidator &validator = SurfaceValidator());
    ~SurfaceRenderer();

    SurfaceApplyResult apply(const QByteArray &surfaceJson);
    QWidget *activeSurface() const;

private:
    QWidget *buildNode(const QString &nodeId,
                       const QMap<QString, QJsonObject> &nodes,
                       QWidget *parent,
                       QStringList *diagnostics);
    QWidget *buildLayoutNode(const QJsonObject &node,
                             const QMap<QString, QJsonObject> &nodes,
                             QWidget *parent,
                             QStringList *diagnostics);

    QWidget *host_;
    const WidgetRegistry *registry_;
    SurfaceValidator validator_;
    QWidget *activeSurface_;
    QMap<QString, QWidget *> activeLeaves_;
    QMap<QString, QString> activeTypes_;
    QMap<QString, QWidget *> buildLeaves_;
    QMap<QString, QWidget *> buildPlaceholders_;
    QSet<QString> buildReuseIds_;
};

} // namespace a2ui

#endif
