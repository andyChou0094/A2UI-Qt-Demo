#include "SurfaceRenderer.h"

#include "WidgetRegistry.h"

#include <QApplication>
#include <QBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLayout>
#include <QPointer>
#include <QVariant>
#include <QWidget>

namespace {

int gapPixels(const QString &gap)
{
    if (gap == QStringLiteral("none")) {
        return 0;
    }
    if (gap == QStringLiteral("small")) {
        return 4;
    }
    if (gap == QStringLiteral("large")) {
        return 16;
    }
    return 8;
}

Qt::Alignment crossAxisAlignment(const QString &layoutType, const QString &align)
{
    if (align == QStringLiteral("stretch")) {
        return Qt::Alignment();
    }
    if (layoutType == QStringLiteral("Row")) {
        if (align == QStringLiteral("start")) {
            return Qt::AlignTop;
        }
        if (align == QStringLiteral("center")) {
            return Qt::AlignVCenter;
        }
        return Qt::AlignBottom;
    }
    if (align == QStringLiteral("start")) {
        return Qt::AlignLeft;
    }
    if (align == QStringLiteral("center")) {
        return Qt::AlignHCenter;
    }
    return Qt::AlignRight;
}

void addWidget(QBoxLayout *layout, QWidget *widget, int weight, Qt::Alignment alignment)
{
    layout->addWidget(widget, weight, alignment);
}

void addUnweightedWidgets(QBoxLayout *layout,
                          const QList<QWidget *> &widgets,
                          const QList<Qt::Alignment> &alignments,
                          const QString &justify)
{
    if (widgets.isEmpty()) {
        return;
    }
    QString effective = justify;
    if (widgets.size() == 1 && justify == QStringLiteral("spaceBetween")) {
        effective = QStringLiteral("start");
    } else if (widgets.size() == 1
               && (justify == QStringLiteral("spaceAround")
                   || justify == QStringLiteral("spaceEvenly"))) {
        effective = QStringLiteral("center");
    }

    if (effective == QStringLiteral("center") || effective == QStringLiteral("end")
            || effective == QStringLiteral("spaceAround")
            || effective == QStringLiteral("spaceEvenly")) {
        layout->addStretch(1);
    }
    for (int index = 0; index < widgets.size(); ++index) {
        addWidget(layout, widgets.at(index), 0, alignments.at(index));
        if (index + 1 < widgets.size()) {
            if (effective == QStringLiteral("spaceBetween")
                    || effective == QStringLiteral("spaceEvenly")) {
                layout->addStretch(1);
            } else if (effective == QStringLiteral("spaceAround")) {
                layout->addStretch(2);
            }
        }
    }
    if (effective == QStringLiteral("start") || effective == QStringLiteral("center")
            || effective == QStringLiteral("spaceAround")
            || effective == QStringLiteral("spaceEvenly")) {
        layout->addStretch(1);
    }
}

} // namespace

namespace a2ui {

SurfaceApplyResult::SurfaceApplyResult()
    : applied(false)
{
}

SurfaceRenderer::SurfaceRenderer(QWidget *host, const WidgetRegistry *registry,
                                 const SurfaceValidator &validator)
    : host_(host),
      registry_(registry),
      validator_(validator),
      activeSurface_(0)
{
    if (host_ && !host_->layout()) {
        QVBoxLayout *layout = new QVBoxLayout(host_);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
    }
}

SurfaceRenderer::~SurfaceRenderer()
{
}

SurfaceApplyResult SurfaceRenderer::apply(const QByteArray &surfaceJson)
{
    SurfaceApplyResult result;
    const SurfaceValidationResult validation = validator_.validate(surfaceJson);
    if (!validation.isValid) {
        result.diagnostics = validation.diagnostics;
        return result;
    }
    if (!host_ || !host_->layout() || !registry_) {
        result.diagnostics.append(QStringLiteral("renderer host or registry is unavailable"));
        return result;
    }

    const QJsonObject document = QJsonDocument::fromJson(surfaceJson).object();
    const QJsonArray nodeArray = document.value(QStringLiteral("nodes")).toArray();
    QMap<QString, QJsonObject> nodes;
    for (int index = 0; index < nodeArray.size(); ++index) {
        const QJsonObject node = nodeArray.at(index).toObject();
        nodes.insert(node.value(QStringLiteral("id")).toString(), node);
    }

    QWidget *stagingOwner = new QWidget;
    QMap<QString, QString> targetTypes;
    std::function<void(const QString &)> collectLeaves = [&](const QString &id) {
        const QJsonObject node = nodes.value(id);
        const QString type = node.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("Row") || type == QStringLiteral("Column")) {
            const QJsonArray children = node.value(QStringLiteral("children")).toArray();
            for (int index = 0; index < children.size(); ++index) {
                collectLeaves(children.at(index).toString());
            }
            return;
        }
        result.plan.orderedLeafIds.append(id);
        targetTypes.insert(id, type);
    };
    collectLeaves(document.value(QStringLiteral("root")).toString());

    for (QStringList::const_iterator it = result.plan.orderedLeafIds.constBegin();
         it != result.plan.orderedLeafIds.constEnd(); ++it) {
        const QString id = *it;
        const QString type = targetTypes.value(id);
        if (activeLeaves_.contains(id) && activeTypes_.value(id) == type) {
            result.plan.reusedIds.append(id);
            buildReuseIds_.insert(id);
            continue;
        }
        if (activeLeaves_.contains(id)) {
            result.plan.replacedIds.append(id);
        } else {
            result.plan.createdIds.append(id);
        }
        QString error;
        QWidget *leaf = registry_->create(type, stagingOwner, &error);
        if (!leaf) {
            result.diagnostics.append(QStringLiteral("failed to stage %1 (%2): %3")
                                      .arg(id, type, error));
            delete stagingOwner;
            buildLeaves_.clear();
            buildReuseIds_.clear();
            return result;
        }
        buildLeaves_.insert(id, leaf);
    }
    for (QMap<QString, QWidget *>::const_iterator it = activeLeaves_.constBegin();
         it != activeLeaves_.constEnd(); ++it) {
        if (!targetTypes.contains(it.key())) {
            result.plan.removedIds.append(it.key());
        }
    }

    QWidget *staged = buildNode(document.value(QStringLiteral("root")).toString(),
                                nodes, stagingOwner, &result.diagnostics);
    if (!staged) {
        delete stagingOwner;
        buildLeaves_.clear();
        buildPlaceholders_.clear();
        buildReuseIds_.clear();
        return result;
    }

    QPointer<QWidget> focusedWidget = QApplication::focusWidget();
    for (QStringList::const_iterator it = result.plan.reusedIds.constBegin();
         it != result.plan.reusedIds.constEnd(); ++it) {
        QWidget *leaf = activeLeaves_.value(*it);
        QWidget *placeholder = buildPlaceholders_.value(*it);
        QWidget *oldParent = leaf ? leaf->parentWidget() : 0;
        if (oldParent && oldParent->layout()) {
            oldParent->layout()->removeWidget(leaf);
        }
        leaf->setParent(host_);
        if (placeholder == staged) {
            placeholder->setParent(0);
            delete placeholder;
            staged = leaf;
        } else {
            QWidget *newParent = placeholder->parentWidget();
            QLayoutItem *placeholderItem = newParent->layout()->replaceWidget(
                placeholder, leaf, Qt::FindDirectChildrenOnly);
            leaf->setParent(newParent);
            delete placeholderItem;
            delete placeholder;
        }
    }
    staged->setParent(host_);
    delete stagingOwner;
    host_->layout()->addWidget(staged);
    QWidget *previous = activeSurface_;
    const bool previousIsReusedLeaf = previous
        && result.plan.reusedIds.contains(
            previous->property("a2uiNodeId").toString());
    activeSurface_ = staged;
    if (previous && !previousIsReusedLeaf) {
        host_->layout()->removeWidget(previous);
        delete previous;
    }
    QMap<QString, QWidget *> committedLeaves;
    for (QStringList::const_iterator it = result.plan.orderedLeafIds.constBegin();
         it != result.plan.orderedLeafIds.constEnd(); ++it) {
        committedLeaves.insert(*it, buildReuseIds_.contains(*it)
                               ? activeLeaves_.value(*it)
                               : buildLeaves_.value(*it));
    }
    activeLeaves_ = committedLeaves;
    activeTypes_ = targetTypes;
    buildLeaves_.clear();
    buildPlaceholders_.clear();
    buildReuseIds_.clear();
    if (focusedWidget) {
        focusedWidget->setFocus(Qt::OtherFocusReason);
    }
    result.applied = true;
    return result;
}

QWidget *SurfaceRenderer::activeSurface() const
{
    return activeSurface_;
}

QWidget *SurfaceRenderer::buildNode(const QString &nodeId,
                                    const QMap<QString, QJsonObject> &nodes,
                                    QWidget *parent,
                                    QStringList *diagnostics)
{
    const QJsonObject node = nodes.value(nodeId);
    const QString type = node.value(QStringLiteral("type")).toString();
    QWidget *widget = 0;
    if (type == QStringLiteral("Row") || type == QStringLiteral("Column")) {
        widget = buildLayoutNode(node, nodes, parent, diagnostics);
    } else {
        if (buildReuseIds_.contains(nodeId)) {
            widget = new QWidget(parent);
            buildPlaceholders_.insert(nodeId, widget);
        } else {
            widget = buildLeaves_.value(nodeId);
            if (widget) {
                widget->setParent(parent);
            }
        }
    }
    if (!widget) {
        diagnostics->append(QStringLiteral("failed to build node: ") + nodeId);
        return 0;
    }
    widget->setProperty("a2uiNodeId", nodeId);
    widget->setProperty("a2uiNodeType", type);
    return widget;
}

QWidget *SurfaceRenderer::buildLayoutNode(const QJsonObject &node,
                                          const QMap<QString, QJsonObject> &nodes,
                                          QWidget *parent,
                                          QStringList *diagnostics)
{
    QWidget *container = new QWidget(parent);
    const QString type = node.value(QStringLiteral("type")).toString();
    QBoxLayout *layout = type == QStringLiteral("Row")
        ? static_cast<QBoxLayout *>(new QHBoxLayout(container))
        : static_cast<QBoxLayout *>(new QVBoxLayout(container));
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(gapPixels(node.value(QStringLiteral("gap")).toString(QStringLiteral("medium"))));

    QList<QWidget *> widgets;
    QList<int> weights;
    QList<Qt::Alignment> alignments;
    const QString align = node.value(QStringLiteral("align")).toString(QStringLiteral("stretch"));
    const QJsonArray children = node.value(QStringLiteral("children")).toArray();
    bool hasPositiveWeight = false;
    for (int index = 0; index < children.size(); ++index) {
        const QString childId = children.at(index).toString();
        QWidget *child = buildNode(childId, nodes, container, diagnostics);
        if (!child) {
            delete container;
            return 0;
        }
        const int weight = nodes.value(childId).value(QStringLiteral("weight")).toInt(0);
        widgets.append(child);
        weights.append(weight);
        alignments.append(crossAxisAlignment(type, align));
        hasPositiveWeight = hasPositiveWeight || weight > 0;
    }

    if (hasPositiveWeight) {
        for (int index = 0; index < widgets.size(); ++index) {
            addWidget(layout, widgets.at(index), weights.at(index), alignments.at(index));
        }
    } else {
        addUnweightedWidgets(layout, widgets, alignments,
                             node.value(QStringLiteral("justify")).toString(QStringLiteral("start")));
    }
    return container;
}

} // namespace a2ui
