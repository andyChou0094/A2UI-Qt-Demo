#ifndef A2UI_DEMO_WIDGET_REGISTRATION_H
#define A2UI_DEMO_WIDGET_REGISTRATION_H

class QString;

namespace a2ui {

class WidgetRegistry;
class CalculationService;

bool registerDemoWidgetFactories(WidgetRegistry &registry,
                                 QString *errorMessage = 0);
bool registerDemoWidgetFactories(WidgetRegistry &registry,
                                 CalculationService *service,
                                 QString *errorMessage = 0);

} // namespace a2ui

#endif
