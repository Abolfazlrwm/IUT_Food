#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QString>

/*!
 * Application-wide light/dark theme switch.
 *
 * The stylesheets live here as raw strings (no extra resources needed)
 * and are applied with qApp->setStyleSheet(), so every window - the
 * launcher, the customer area, dashboards and dialogs - switches live
 * without an application restart.  The choice is persisted in QSettings
 * and restored in main() before the first window is shown.
 */
class ThemeManager
{
public:
    static void init();                 //!< apply the persisted theme
    static void apply(const QString &name); //!< "light" or "dark"
    static void toggle();
    static QString current();
    static bool isDark();

private:
    static QString s_current;
    static QString styleSheetFor(const QString &name);
};

#endif // THEMEMANAGER_H
