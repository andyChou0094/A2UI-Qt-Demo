#include "WidgetRegistry.h"

#include <QCoreApplication>
#include <QThread>
#include <QWidget>

namespace a2ui {

WidgetRegistry::Entry::Entry()
    : kind(BusinessWidget)
{
}

WidgetRegistry::Entry::Entry(const Factory &factoryValue,
                             RegistrationKind kindValue)
    : factory(factoryValue), kind(kindValue)
{
}

bool WidgetRegistry::registerFactory(const QString &componentType,
                                     const Factory &factory,
                                     QString *errorMessage)
{
    return registerEntry(componentType, factory, BusinessWidget, errorMessage);
}

bool WidgetRegistry::registerAdapterFactory(const QString &componentType,
                                            const Factory &factory,
                                            QString *errorMessage)
{
    return registerEntry(componentType,
                         factory,
                         ProjectMaintainedAdapter,
                         errorMessage);
}

bool WidgetRegistry::registerEntry(const QString &componentType,
                                   const Factory &factory,
                                   RegistrationKind kind,
                                   QString *errorMessage)
{
    if (!isGuiThread(errorMessage)) {
        return false;
    }
    if (componentType.trimmed().isEmpty()) {
        setError(errorMessage, QStringLiteral("Component type must not be empty"));
        return false;
    }
    if (!factory) {
        setError(errorMessage, QStringLiteral("Factory must be callable"));
        return false;
    }
    if (entries_.contains(componentType)) {
        setError(errorMessage,
                 QStringLiteral("Component type is already registered: %1")
                     .arg(componentType));
        return false;
    }
    entries_.insert(componentType, Entry(factory, kind));
    return true;
}

QWidget *WidgetRegistry::create(const QString &componentType,
                                QWidget *parent,
                                QString *errorMessage) const
{
    if (!isGuiThread(errorMessage)) {
        return 0;
    }
    if (!parent) {
        setError(errorMessage, QStringLiteral("An embedding parent is required"));
        return 0;
    }
    const QMap<QString, Entry>::const_iterator entry = entries_.constFind(componentType);
    if (entry == entries_.constEnd()) {
        setError(errorMessage,
                 QStringLiteral("Unknown component type: %1").arg(componentType));
        return 0;
    }

    QWidget *widget = 0;
    try {
        widget = entry.value().factory(parent);
    } catch (...) {
        setError(errorMessage,
                 QStringLiteral("Factory threw while creating: %1").arg(componentType));
        return 0;
    }
    if (!widget) {
        setError(errorMessage,
                 QStringLiteral("Factory returned null for: %1").arg(componentType));
        return 0;
    }
    if (widget->isWindow() || (widget->windowFlags() & Qt::Window)) {
        setError(errorMessage,
                 QStringLiteral("Factory returned a top-level window: %1")
                     .arg(componentType));
        delete widget;
        return 0;
    }
    if (widget->parentWidget() != parent) {
        setError(errorMessage,
                 QStringLiteral("Factory did not honor QWidget parent ownership: %1")
                     .arg(componentType));
        delete widget;
        return 0;
    }
    return widget;
}

bool WidgetRegistry::contains(const QString &componentType) const
{
    return entries_.contains(componentType);
}

WidgetRegistry::RegistrationKind WidgetRegistry::registrationKind(
    const QString &componentType,
    bool *found) const
{
    const QMap<QString, Entry>::const_iterator entry = entries_.constFind(componentType);
    const bool hasEntry = entry != entries_.constEnd();
    if (found) {
        *found = hasEntry;
    }
    return hasEntry ? entry.value().kind : BusinessWidget;
}

bool WidgetRegistry::isGuiThread(QString *errorMessage)
{
    QCoreApplication *application = QCoreApplication::instance();
    if (!application || QThread::currentThread() != application->thread()) {
        setError(errorMessage, QStringLiteral("Widget factories require the GUI thread"));
        return false;
    }
    return true;
}

void WidgetRegistry::setError(QString *errorMessage, const QString &message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
}

} // namespace a2ui
