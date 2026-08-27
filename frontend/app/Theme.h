#ifndef A2UI_THEME_H
#define A2UI_THEME_H

#include <QString>

class QApplication;

namespace a2ui {

struct ThemeResult
{
    bool loaded;
    bool chineseFontAvailable;
    QString fontFamily;
    QString diagnostic;
};

ThemeResult applyHostTheme(QApplication *application);

} // namespace a2ui

#endif
