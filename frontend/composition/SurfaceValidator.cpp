#include "SurfaceValidator.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QMap>
#include <QRegularExpression>
#include <QSet>

#include <cmath>
#include <functional>

namespace {

const QSet<QString> kRootKeys = QSet<QString>()
    << QStringLiteral("version") << QStringLiteral("surfaceId")
    << QStringLiteral("root") << QStringLiteral("nodes");
const QSet<QString> kLayoutKeys = QSet<QString>()
    << QStringLiteral("id") << QStringLiteral("type") << QStringLiteral("children")
    << QStringLiteral("gap") << QStringLiteral("align") << QStringLiteral("justify")
    << QStringLiteral("weight");
const QSet<QString> kBusinessKeys = QSet<QString>()
    << QStringLiteral("id") << QStringLiteral("type") << QStringLiteral("weight");
const QSet<QString> kLayoutTypes = QSet<QString>()
    << QStringLiteral("Row") << QStringLiteral("Column");
const QSet<QString> kGaps = QSet<QString>()
    << QStringLiteral("none") << QStringLiteral("small")
    << QStringLiteral("medium") << QStringLiteral("large");
const QSet<QString> kAligns = QSet<QString>()
    << QStringLiteral("start") << QStringLiteral("center")
    << QStringLiteral("end") << QStringLiteral("stretch");
const QSet<QString> kJustifies = QSet<QString>()
    << QStringLiteral("start") << QStringLiteral("center") << QStringLiteral("end")
    << QStringLiteral("spaceBetween") << QStringLiteral("spaceAround")
    << QStringLiteral("spaceEvenly");

bool hasOnlyKeys(const QJsonObject &object, const QSet<QString> &allowed)
{
    const QStringList keys = object.keys();
    for (QStringList::const_iterator it = keys.constBegin(); it != keys.constEnd(); ++it) {
        if (!allowed.contains(*it)) {
            return false;
        }
    }
    return true;
}

bool hasRequiredKeys(const QJsonObject &object, const QSet<QString> &required)
{
    for (QSet<QString>::const_iterator it = required.constBegin(); it != required.constEnd(); ++it) {
        if (!object.contains(*it)) {
            return false;
        }
    }
    return true;
}

bool isNodeId(const QJsonValue &value)
{
    static const QRegularExpression expression(QStringLiteral("^[A-Za-z][A-Za-z0-9_-]{0,63}$"));
    return value.isString() && expression.match(value.toString()).hasMatch();
}

bool isIntegerInRange(const QJsonValue &value, int minimum, int maximum)
{
    if (!value.isDouble()) {
        return false;
    }
    const double number = value.toDouble();
    return std::floor(number) == number && number >= minimum && number <= maximum;
}

void addError(a2ui::SurfaceValidationResult *result, const QString &message)
{
    result->diagnostics.append(message);
}

} // namespace

namespace a2ui {

SurfaceValidationResult::SurfaceValidationResult()
    : isValid(false)
{
}

SurfaceValidator::Limits::Limits()
    : maximumNodes(32),
      maximumDepth(8)
{
}

SurfaceValidator::SurfaceValidator(const QMap<QString, bool> &catalogMultiplicity,
                                   const Limits &limits)
    : catalogMultiplicity_(catalogMultiplicity),
      limits_(limits)
{
}

QMap<QString, bool> SurfaceValidator::demoCatalog()
{
    QMap<QString, bool> catalog;
    catalog.insert(QStringLiteral("Calculator"), true);
    catalog.insert(QStringLiteral("CalculationHistory"), true);
    catalog.insert(QStringLiteral("CalculationStats"), true);
    catalog.insert(QStringLiteral("Clock"), true);
    catalog.insert(QStringLiteral("NotePad"), true);
    return catalog;
}

SurfaceValidationResult SurfaceValidator::validate(const QByteArray &json) const
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        SurfaceValidationResult result;
        addError(&result, QStringLiteral("invalid JSON: %1").arg(parseError.errorString()));
        return result;
    }
    return validate(document);
}

SurfaceValidationResult SurfaceValidator::validate(const QJsonDocument &document) const
{
    SurfaceValidationResult result;
    if (!document.isObject()) {
        addError(&result, QStringLiteral("document must be an object"));
        return result;
    }

    const QJsonObject rootObject = document.object();
    if (!hasOnlyKeys(rootObject, kRootKeys) || !hasRequiredKeys(rootObject, kRootKeys)) {
        addError(&result, QStringLiteral("document fields must be exactly version, surfaceId, root, nodes"));
    }
    if (!rootObject.value(QStringLiteral("version")).isString()
            || rootObject.value(QStringLiteral("version")).toString() != QStringLiteral("0.1")) {
        addError(&result, QStringLiteral("version must be 0.1"));
    }
    if (!rootObject.value(QStringLiteral("surfaceId")).isString()
            || rootObject.value(QStringLiteral("surfaceId")).toString() != QStringLiteral("main")) {
        addError(&result, QStringLiteral("surfaceId must be main"));
    }
    if (!isNodeId(rootObject.value(QStringLiteral("root")))) {
        addError(&result, QStringLiteral("root must be a valid node ID"));
    }
    if (!rootObject.value(QStringLiteral("nodes")).isArray()) {
        addError(&result, QStringLiteral("nodes must be an array"));
        return result;
    }

    const QString rootId = rootObject.value(QStringLiteral("root")).toString();
    const QJsonArray nodes = rootObject.value(QStringLiteral("nodes")).toArray();
    if (nodes.size() > limits_.maximumNodes) {
        addError(&result, QStringLiteral("node limit exceeded"));
    }

    QMap<QString, QJsonObject> byId;
    QMap<QString, QStringList> childrenById;
    QMap<QString, int> typeCounts;
    for (int index = 0; index < nodes.size(); ++index) {
        const QString label = QStringLiteral("nodes[%1]").arg(index);
        if (!nodes.at(index).isObject()) {
            addError(&result, label + QStringLiteral(" must be an object"));
            continue;
        }
        const QJsonObject node = nodes.at(index).toObject();
        if (!isNodeId(node.value(QStringLiteral("id")))) {
            addError(&result, label + QStringLiteral(" has an invalid id"));
            continue;
        }
        const QString id = node.value(QStringLiteral("id")).toString();
        if (byId.contains(id)) {
            addError(&result, QStringLiteral("duplicate node id: ") + id);
        }
        byId.insert(id, node);

        if (!node.value(QStringLiteral("type")).isString()) {
            addError(&result, label + QStringLiteral(" has an invalid type"));
            childrenById.insert(id, QStringList());
            continue;
        }
        const QString type = node.value(QStringLiteral("type")).toString();
        if (kLayoutTypes.contains(type)) {
            const QSet<QString> required = QSet<QString>()
                << QStringLiteral("id") << QStringLiteral("type") << QStringLiteral("children");
            if (!hasRequiredKeys(node, required)) {
                addError(&result, label + QStringLiteral(" is missing layout fields"));
            }
            if (!hasOnlyKeys(node, kLayoutKeys)) {
                addError(&result, label + QStringLiteral(" contains forbidden layout fields"));
            }
            QStringList childIds;
            const QJsonValue childrenValue = node.value(QStringLiteral("children"));
            if (!childrenValue.isArray()) {
                addError(&result, label + QStringLiteral(".children must be an array of IDs"));
            } else {
                const QJsonArray childArray = childrenValue.toArray();
                if (childArray.size() > limits_.maximumNodes) {
                    addError(&result, label + QStringLiteral(".children exceeds the node limit"));
                }
                for (int childIndex = 0; childIndex < childArray.size(); ++childIndex) {
                    if (!isNodeId(childArray.at(childIndex))) {
                        addError(&result, label + QStringLiteral(".children must contain valid IDs"));
                    } else {
                        childIds.append(childArray.at(childIndex).toString());
                    }
                }
            }
            childrenById.insert(id, childIds);
            if (node.contains(QStringLiteral("gap"))
                    && (!node.value(QStringLiteral("gap")).isString()
                        || !kGaps.contains(node.value(QStringLiteral("gap")).toString()))) {
                addError(&result, label + QStringLiteral(" has an invalid gap"));
            }
            if (node.contains(QStringLiteral("align"))
                    && (!node.value(QStringLiteral("align")).isString()
                        || !kAligns.contains(node.value(QStringLiteral("align")).toString()))) {
                addError(&result, label + QStringLiteral(" has an invalid align"));
            }
            if (node.contains(QStringLiteral("justify"))
                    && (!node.value(QStringLiteral("justify")).isString()
                        || !kJustifies.contains(node.value(QStringLiteral("justify")).toString()))) {
                addError(&result, label + QStringLiteral(" has an invalid justify"));
            }
        } else if (catalogMultiplicity_.contains(type)) {
            const QSet<QString> required = QSet<QString>()
                << QStringLiteral("id") << QStringLiteral("type");
            if (!hasRequiredKeys(node, required)) {
                addError(&result, label + QStringLiteral(" is missing business fields"));
            }
            if (!hasOnlyKeys(node, kBusinessKeys)) {
                addError(&result, label + QStringLiteral(" contains forbidden business fields"));
            }
            typeCounts[type] = typeCounts.value(type, 0) + 1;
            childrenById.insert(id, QStringList());
        } else {
            addError(&result, label + QStringLiteral(" has an unknown or forbidden type"));
            childrenById.insert(id, QStringList());
        }
        if (node.contains(QStringLiteral("weight"))
                && !isIntegerInRange(node.value(QStringLiteral("weight")), 0, 10)) {
            addError(&result, label + QStringLiteral(" has an invalid weight"));
        }
    }

    if (!byId.contains(rootId)) {
        addError(&result, QStringLiteral("root does not reference a declared node"));
    } else if (byId.value(rootId).contains(QStringLiteral("weight"))) {
        addError(&result, QStringLiteral("root must not declare weight"));
    }

    QMap<QString, int> parentCount;
    for (QMap<QString, QJsonObject>::const_iterator it = byId.constBegin(); it != byId.constEnd(); ++it) {
        parentCount.insert(it.key(), 0);
    }
    for (QMap<QString, QStringList>::const_iterator it = childrenById.constBegin();
         it != childrenById.constEnd(); ++it) {
        bool hasPositiveWeight = false;
        const QStringList childIds = it.value();
        for (QStringList::const_iterator child = childIds.constBegin(); child != childIds.constEnd(); ++child) {
            if (!byId.contains(*child)) {
                addError(&result, it.key() + QStringLiteral(" references unknown child ") + *child);
                continue;
            }
            parentCount[*child] = parentCount.value(*child, 0) + 1;
            if (byId.value(*child).value(QStringLiteral("weight")).toInt(0) > 0) {
                hasPositiveWeight = true;
            }
        }
        if (hasPositiveWeight
                && byId.value(it.key()).value(QStringLiteral("justify")).toString(QStringLiteral("start"))
                    != QStringLiteral("start")) {
            addError(&result, it.key() + QStringLiteral(" combines positive child weight with non-start justify"));
        }
    }
    for (QMap<QString, int>::const_iterator it = parentCount.constBegin(); it != parentCount.constEnd(); ++it) {
        const int expected = it.key() == rootId ? 0 : 1;
        if (it.value() != expected) {
            addError(&result, QStringLiteral("%1 has %2 parents; expected %3")
                     .arg(it.key()).arg(it.value()).arg(expected));
        }
    }

    QSet<QString> visited;
    QSet<QString> active;
    std::function<void(const QString &, int)> visit = [&](const QString &id, int depth) {
        if (active.contains(id)) {
            addError(&result, QStringLiteral("cycle detected at ") + id);
            return;
        }
        if (visited.contains(id) || !byId.contains(id)) {
            return;
        }
        if (depth > limits_.maximumDepth) {
            addError(&result, QStringLiteral("depth limit exceeded at ") + id);
        }
        active.insert(id);
        const QStringList childIds = childrenById.value(id);
        for (QStringList::const_iterator it = childIds.constBegin(); it != childIds.constEnd(); ++it) {
            visit(*it, depth + 1);
        }
        active.remove(id);
        visited.insert(id);
    };
    if (byId.contains(rootId)) {
        visit(rootId, 1);
    }
    QSet<QString> unreachable;
    for (QMap<QString, QJsonObject>::const_iterator it = byId.constBegin(); it != byId.constEnd(); ++it) {
        if (!visited.contains(it.key())) {
            unreachable.insert(it.key());
        }
    }
    if (!unreachable.isEmpty()) {
        QStringList ids = unreachable.values();
        ids.sort();
        addError(&result, QStringLiteral("unreachable nodes: ") + ids.join(QStringLiteral(", ")));
    }

    for (QMap<QString, bool>::const_iterator it = catalogMultiplicity_.constBegin();
         it != catalogMultiplicity_.constEnd(); ++it) {
        if (!it.value() && typeCounts.value(it.key(), 0) > 1) {
            addError(&result, it.key() + QStringLiteral(" does not allow multiple instances"));
        }
    }

    result.isValid = result.diagnostics.isEmpty();
    return result;
}

} // namespace a2ui
