#include <QApplication>
#include <QFontDatabase>
#include <QMessageBox>
#include <QDir>
#include <QDebug>

#include "appdirs.h"
#include "databasehandler.h"
#include "demodata.h"
#include "mainwindow.h"
#include "thememanager.h"
#include "version.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("IUT-Food"));
    QApplication::setOrganizationName(QStringLiteral("IUT"));
    QApplication::setApplicationVersion(QStringLiteral(IUT_FOOD_VERSION));

    // Ship the Vazirmatn typeface so Persian text renders identically on
    // Windows, Linux and macOS instead of falling back to Tahoma.
    for (const QString &fontFile : { QStringLiteral(":/fonts/Vazirmatn-Regular.ttf"),
                                     QStringLiteral(":/fonts/Vazirmatn-Bold.ttf") }) {
        if (QFontDatabase::addApplicationFont(fontFile) < 0)
            qWarning() << "Could not load bundled font" << fontFile;
    }
    QFont appFont(QStringLiteral("Vazirmatn"), 10);
    appFont.setStyleStrategy(QFont::PreferAntialias);
    QApplication::setFont(appFont);

    // Restore the persisted light/dark theme before any window shows up.
    ThemeManager::init();

    // Desktop: one SQLite file next to the executable keeps the demo
    // portable.  Android/macOS (AppDirs) use the per-user app-data dir,
    // because the bundle/ABI location is read-only there.
    const QString dbPath = AppDirs::defaultDatabasePath();

    DataBaseHandler &db = DataBaseHandler::instance();
    if (!db.openDataBase(dbPath)) {
        QMessageBox::critical(nullptr, QStringLiteral("خطای دیتابیس"),
                              QStringLiteral("برنامه قادر به باز کردن پایگاه داده در مسیر زیر نیست:\n%1")
                                  .arg(dbPath));
        return 1;
    }
    db.createTables();
    seedDemoData(db); // no-op unless the database is fresh

    MainWindow launcher;
    launcher.show();

    return app.exec();
}
