#ifndef A2UI_WIDGET_REGISTRY_H
#define A2UI_WIDGET_REGISTRY_H

#include <QMap>
#include <QString>

#include <functional>

class QWidget;

namespace a2ui {

class WidgetRegistry
{
public:
    typedef std::function<QWidget *(QWidget *parent)> Factory;

    enum RegistrationKind {
        BusinessWidget,
        ProjectMaintainedAdapter
    };

    bool registerFactory(const QString &componentType,
                         const Factory &factory,
                         QString *errorMessage = 0);
    bool registerAdapterFactory(const QString &componentType,
                                const Factory &factory,
                                QString *errorMessage = 0);
    QWidget *create(const QString &componentType,
                    QWidget *parent,
                    QString *errorMessage = 0) const;
    bool contains(const QString &componentType) const;
    RegistrationKind registrationKind(const QString &componentType,
                                      bool *found = 0) const;

private:
    struct Entry {
        Entry();
        Entry(const Factory &factoryValue, RegistrationKind kindValue);

        Factory factory;
        RegistrationKind kind;
    };

    bool registerEntry(const QString &componentType,
                       const Factory &factory,
                       RegistrationKind kind,
                       QString *errorMessage);
    static bool isGuiThread(QString *errorMessage);
    static void setError(QString *errorMessage, const QString &message);

    QMap<QString, Entry> entries_;
};

} // namespace a2ui

#endif
