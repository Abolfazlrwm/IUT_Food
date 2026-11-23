#pragma once
/*
 * Platform-aware locations for the per-user application data.
 *
 * The desktop story stays exactly what it has always been: one SQLite
 * file next to the binary keeps the demo portable (run_demo.sh relies
 * on it).  Sandboxed platforms get a writable per-user directory:
 *   - Android: applicationDirPath() points inside the read-only APK,
 *     so the database must live in QStandardPaths::AppDataLocation.
 *   - macOS: the client ships as an .app bundle inside a DMG; writing
 *     next to the binary would wipe data on every update.
 * The server never ships to Android and keeps the desktop behaviour.
 */
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QString>

namespace AppDirs {

//! Directory that is guaranteed writable and stable across launches.
inline QString dataDir()
{
#if defined(Q_OS_ANDROID) || defined(Q_OS_MACOS)
    const QString dir = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    if (!dir.isEmpty() && !QDir(dir).exists())
        QDir().mkpath(dir);
    return dir;
#else
    return QCoreApplication::applicationDirPath();
#endif
}

//! Full path of the demo/primary SQLite database.
inline QString defaultDatabasePath()
{
    return dataDir() + QStringLiteral("/iut_food.db");
}

} // namespace AppDirs
