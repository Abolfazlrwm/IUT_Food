//! Unit tests for the v2.5 BackupManager: VACUUM INTO snapshots are
//! real, openable SQLite databases, retention pruning is bounded, and
//! the disabled mode fails politely instead of throwing.
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QtTest>

#include "backupmanager.h"

#include "databasehandler.h"

class BackupTests : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    QString m_dbPath;
    QString m_backupDir;

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        m_dbPath = m_dir.filePath(QStringLiteral("source.db"));
        m_backupDir = m_dir.filePath(QStringLiteral("backups"));

        // A real database with content worth backing up.
        auto &db = DataBaseHandler::instance();
        QVERIFY(db.openDataBase(m_dbPath));
        QVERIFY(db.createTables());
        QVERIFY(db.registerRestaurant(QStringLiteral("kebab_house"),
                                      QStringLiteral("pw"),
                                      QStringLiteral("Kebab House"),
                                      QStringLiteral("traditional"),
                                      QStringLiteral("Tehran"), 2));
    }

    //! A snapshot is a valid SQLite file that contains the production
    //! schema and the rows written before the backup.
    void snapshotIsOpenableDatabase()
    {
        BackupManager manager;
        manager.configure(m_dbPath, m_backupDir, 60, 5);

        QString out;
        QVERIFY(manager.backupNow(&out));
        QVERIFY(QFile::exists(out));
        QVERIFY(QFile(out).size() > 0);
        QCOMPARE(manager.lastPath(), out);

        // Open the snapshot through a second connection and inspect it.
        {
            QSqlDatabase snap = QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"), QStringLiteral("snapshot-check"));
            snap.setDatabaseName(out);
            QVERIFY(snap.open());
            QSqlQuery tables(snap);
            QVERIFY(tables.exec(QStringLiteral(
                "SELECT name FROM sqlite_master WHERE type='table' "
                "ORDER BY name")));
            QStringList names;
            while (tables.next())
                names << tables.value(0).toString();
            QVERIFY(names.contains(QStringLiteral("users")));
            QVERIFY(names.contains(QStringLiteral("foods")));

            QSqlQuery rows(snap);
            QVERIFY(rows.exec(QStringLiteral(
                "SELECT COUNT(*) FROM users WHERE username='kebab_house'")));
            QVERIFY(rows.next());
            QCOMPARE(rows.value(0).toInt(), 1);
            tables.finish();
            rows.finish();
            snap.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("snapshot-check"));
    }

    //! Old generations are pruned after every snapshot: with keep=3 the
    //! directory never holds more than three backup files, no matter
    //! how many runs happened (pre-existing older files count too).
    void retentionPrunesOldestGenerations()
    {
        const QString dir = m_dir.filePath(QStringLiteral("rotating"));
        QDir().mkpath(dir);

        // Seed the directory with three "old" snapshots (older names
        // sort first because the timestamps are zero-padded).
        QStringList stale;
        for (const char *stamp : {"20251101-000001", "20251101-000002",
                                  "20251101-000003"}) {
            const QString path =
                dir + QStringLiteral("/iut_food-backup-%1.db").arg(QLatin1String(stamp));
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("stale");
            stale << path;
        }

        BackupManager manager;
        manager.configure(m_dbPath, dir, 60, /*keepFiles=*/3);

        QString out;
        QVERIFY2(manager.backupNow(&out),
                 qPrintable(manager.lastError()));
        const QFileInfoList kept = QDir(dir).entryInfoList(
            QStringList{QStringLiteral("iut_food-backup-*.db")}, QDir::Files);
        QCOMPARE(kept.size(), 3); // bounded, never grows beyond keep
        QVERIFY(!QFile::exists(stale[0])); // oldest generation is gone
        QVERIFY(QFile::exists(out));       // newest snapshot survives

        // Every further run evicts the then-oldest file and keeps the
        // bound steady.
        QVERIFY2(manager.backupNow(&out),
                 qPrintable(manager.lastError()));
        QCOMPARE(QDir(dir).entryList(
                     QStringList{QStringLiteral("iut_food-backup-*.db")},
                     QDir::Files).size(), 3);
        QVERIFY(!QFile::exists(stale[1]));
        QVERIFY(QFile::exists(stale[2])); // still within the newest three
    }

    //! Without a configured directory (or database) the manager refuses
    //! to run and reports why, instead of crashing or creating junk.
    void disabledModeFailsPolitely()
    {
        BackupManager manager;
        manager.configure(m_dbPath, QString(), 60, 5);
        QString error;
        QVERIFY(!manager.backupNow(nullptr, &error));
        QVERIFY(error.contains(QLatin1String("disabled")));
        QVERIFY(manager.lastPath().isEmpty());

        BackupManager noDb;
        noDb.configure(QString(), m_backupDir, 60, 5);
        QVERIFY(!noDb.backupNow(nullptr, &error));
        QVERIFY(error.contains(QLatin1String("disabled")));
    }

    //! A missing source database reports the failure through the
    //! return value and the error string - never an exception.
    void missingSourceReportsError()
    {
        BackupManager manager;
        manager.configure(m_dir.filePath(QStringLiteral("nope.db")), m_backupDir, 60, 5);
        QString error;
        QVERIFY(!manager.backupNow(nullptr, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!manager.lastError().isEmpty());
    }

    //! v2.6 regression: a failed snapshot must not leak its named
    //! connection into Qt's registry.  The old code returned early on
    //! open()/VACUUM failures, so a server backing up to an unwritable
    //! directory accumulated stale connections without bound - and the
    //! backupFinished signal carried an empty message, because
    //! m_lastError was never written on the failure paths.
    void failureDropsTheConnectionAndSaysWhy()
    {
        // A read-only directory makes QSQLITE's open() fail for real:
        // SQLite cannot create the destination-side journal there.
        const QString readOnlyDir = m_dir.filePath(QStringLiteral("read-only"));
        QDir().mkpath(readOnlyDir);
        QFile::setPermissions(readOnlyDir, QFile::ReadOwner | QFile::ExeOwner);

        BackupManager manager;
        manager.configure(m_dbPath, readOnlyDir, 60, 5);
        QString error;
        QVERIFY(!manager.backupNow(nullptr, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!manager.lastError().isEmpty());

        const QStringList leftovers = QSqlDatabase::connectionNames().filter(
            QStringLiteral("iut-food-backup-"));
        QVERIFY2(leftovers.isEmpty(),
                 qPrintable(QStringLiteral("leaked connections: %1")
                                .arg(leftovers.join(QLatin1Char(',')))));

        // Restore the permissions so QTemporaryDir can clean up.
        QFile::setPermissions(readOnlyDir,
                              QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    }
};

QTEST_MAIN(BackupTests)
#include "backup_tests.moc"
