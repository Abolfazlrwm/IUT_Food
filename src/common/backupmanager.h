#ifndef BACKUPMANAGER_H
#define BACKUPMANAGER_H

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QTimer>
#include <QVariantList>

//! Rolling SQLite snapshots for a long-running server (v2.5).  Every
//! interval the manager runs SQLite's `VACUUM INTO` against the live
//! database - the statement takes a consistent read snapshot, so the
//! backup is always a valid, fully compacted database even while
//! orders are being written in WAL mode.
//!
//! Files land in the configured directory as
//!   iut_food-backup-YYYYMMDD-HHMMSS.db
//! and only the newest \p keepFiles generations survive; pruning runs
//! after every successful snapshot, so the directory size is bounded.
//!
//! The manager opens its own short-lived SQLite connection per backup
//! instead of touching DataBaseHandler's connection, which keeps the
//! unit tests free of global state.  A backup that fails (unwritable
//! directory, missing file) is logged and simply skipped - a failed
//! backup must never take the food server down.
class BackupManager : public QObject
{
    Q_OBJECT

public:
    explicit BackupManager(QObject *parent = nullptr) : QObject(parent) {}

    //! \p dbPath may be empty (backups disabled).  \p keepFiles has a
    //! floor of 1 so a bad value can never wipe the directory.
    void configure(const QString &dbPath, const QString &directory,
                   int intervalMinutes, int keepFiles)
    {
        m_dbPath = dbPath;
        m_dir = directory;
        m_intervalMinutes = qMax(1, intervalMinutes);
        m_keepFiles = qMax(1, keepFiles);
    }

    //! Writes one snapshot and prunes old generations.  Returns false
    //! (with \p error filled in) when backups are disabled or the
    //! VACUUM failed; the destination path is returned via \p outPath.
    //! Every failure also lands in lastError(), so the backupFinished
    //! signal never carries an empty message.
    bool backupNow(QString *outPath = nullptr, QString *error = nullptr)
    {
        const auto fail = [this, error](const QString &message) {
            m_lastError = message;
            if (error)
                *error = message;
            return false;
        };
        if (m_dbPath.isEmpty() || m_dir.isEmpty())
            return fail(QStringLiteral("backups disabled: no database or directory"));
        // Refuse loudly when the source is gone: letting SQLite create
        // an empty file on connect would "back up" nothing silently.
        if (!QFileInfo::exists(m_dbPath))
            return fail(QStringLiteral("source database does not exist: %1").arg(m_dbPath));
        QDir().mkpath(m_dir);
        // Millisecond resolution: two snapshots within the same second
        // (timer due + shutdown snapshot) must never collide, because
        // VACUUM INTO refuses to overwrite an existing file.  Fixed
        // width keeps lexical order == chronological order for pruning.
        const QString stamp =
            QDateTime::currentDateTime().toString(
                QStringLiteral("yyyyMMdd-HHmmss-zzz"));
        QString dest = m_dir + QStringLiteral("/iut_food-backup-%1.db").arg(stamp);
        // v2.8 fix: a coarse clock tick can hand two rapid snapshots the
        // same millisecond stamp, and VACUUM INTO refuses to overwrite an
        // existing file (it showed up as a flaky retention test under
        // ctest).  Zero-padded uniquifiers keep lexical order ==
        // chronological order, so pruning stays correct.
        for (int bump = 2; QFileInfo::exists(dest) && bump < 1000; ++bump) {
            dest = m_dir + QStringLiteral("/iut_food-backup-%1-%2.db")
                       .arg(stamp)
                       .arg(bump, 3, 10, QLatin1Char('0'));
        }

        // A throwaway named connection: addDatabase with the same name
        // twice would warn, so uniquify with a millisecond counter.
        static quint64 sequence = 0;
        const QString connection =
            QStringLiteral("iut-food-backup-%1").arg(quint64(sequence++));
        QString failure;
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"), connection);
            db.setDatabaseName(m_dbPath);
            if (!db.open()) {
                failure = QStringLiteral("cannot open %1: %2")
                              .arg(m_dbPath, db.lastError().text());
            } else {
                QSqlQuery vacuum(db);
                vacuum.prepare(QStringLiteral("VACUUM INTO :dest"));
                vacuum.bindValue(QStringLiteral(":dest"), dest);
                if (!vacuum.exec()) {
                    failure = QStringLiteral("VACUUM INTO failed: %1")
                                  .arg(vacuum.lastError().text());
                }
            }
        }
        // v2.6 fix: the throwaway connection is dropped on EVERY path.
        // The old code returned early on failures, so every failed
        // snapshot left a dead named connection in Qt's global registry -
        // a server backing up to an unwritable directory accumulated
        // stale connections without bound (and Qt warns about each one
        // when the next snapshot reuses the default driver).
        QSqlDatabase::removeDatabase(connection);
        if (!failure.isEmpty()) {
            m_lastError = failure;
            if (error)
                *error = failure;
            return false;
        }

        if (QFile(dest).size() <= 0)
            return fail(QStringLiteral("snapshot %1 is empty").arg(dest));
        m_lastPath = dest;
        m_lastError.clear();
        if (outPath)
            *outPath = dest;
        pruneOldGenerations();
        return true;
    }

    //! Starts the periodic timer on the server's event loop.  One
    //! snapshot is taken immediately so a fresh deployment starts with
    //! a baseline instead of waiting a full interval.
    void start()
    {
        auto *timer = new QTimer(this);
        timer->setInterval(m_intervalMinutes * 60 * 1000);
        connect(timer, &QTimer::timeout, this, &BackupManager::backupDue);
        timer->start();
        const bool ok = backupNow(); // baseline snapshot for fresh deploys
        emit backupFinished(ok, ok ? m_lastPath : m_lastError);
    }

    QString lastPath() const { return m_lastPath; }
    QString lastError() const { return m_lastError; }

signals:
    //! Emitted after every snapshot attempt; \p ok is false on failure
    //! and \p message carries either the new path or the error text.
    void backupFinished(bool ok, const QString &message);

private slots:
    void backupDue()
    {
        const bool ok = backupNow();
        emit backupFinished(ok, ok ? m_lastPath : m_lastError);
    }

private:
    //! Keeps only the newest m_keepFiles snapshot files; snapshot names
    //! are zero-padded timestamps, so lexical order == chronological.
    void pruneOldGenerations()
    {
        QFileInfoList snaps = QDir(m_dir)
                                  .entryInfoList(
                                      QStringList{QStringLiteral("iut_food-backup-*.db")},
                                      QDir::Files, QDir::Name);
        while (snaps.size() > m_keepFiles) {
            QFile::remove(snaps.takeFirst().absoluteFilePath());
        }
    }

    QString m_dbPath;
    QString m_dir;
    int m_intervalMinutes = 60;
    int m_keepFiles = 5;
    QString m_lastPath;
    QString m_lastError;
};

#endif // BACKUPMANAGER_H
