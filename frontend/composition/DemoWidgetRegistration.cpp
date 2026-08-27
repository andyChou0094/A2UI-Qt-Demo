#include "DemoWidgetRegistration.h"

#include "DemoWidgets.h"
#include "WidgetRegistry.h"

namespace a2ui {

bool registerDemoWidgetFactories(WidgetRegistry &registry,
                                 QString *errorMessage)
{
    return registerDemoWidgetFactories(registry, 0, errorMessage);
}

bool registerDemoWidgetFactories(WidgetRegistry &registry,
                                 CalculationService *service,
                                 QString *errorMessage)
{
    if (!registry.registerFactory(
            QStringLiteral("Calculator"),
            [service](QWidget *parent) { return new Calculator(service, parent); },
            errorMessage)) {
        return false;
    }
    if (!registry.registerFactory(
            QStringLiteral("CalculationHistory"),
            [service](QWidget *parent) { return new CalculationHistory(service, parent); },
            errorMessage)) {
        return false;
    }
    if (!registry.registerFactory(
            QStringLiteral("CalculationStats"),
            [service](QWidget *parent) { return new CalculationStats(service, parent); },
            errorMessage)) {
        return false;
    }
    if (!registry.registerFactory(
            QStringLiteral("Clock"),
            [](QWidget *parent) { return new Clock(parent); },
            errorMessage)) {
        return false;
    }
    return registry.registerFactory(
        QStringLiteral("NotePad"),
        [](QWidget *parent) { return new NotePad(parent); },
        errorMessage);
}

} // namespace a2ui
