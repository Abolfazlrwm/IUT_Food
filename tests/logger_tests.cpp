//! Unit tests for the rotating file Logger (v2.4): level filtering,
//! size-based rotation with a bounded number of generations, the
//! stderr-only fallback and the command-line level parser.
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <QtTest>

#include "logger.h"

namespace {

QString readAll(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    QTextStream stream(&file);
    return stream.readAll();
}

} // namespace

class LoggerTests : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;

    QString logPath() const
    {
        return m_dir.filePath(QStringLiteral("server.log"));
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
    }

    //! A configured minimum level silences everything quieter, while
    //! warning and error always reach the file.
    void levelFiltering()
    {
        const QString path = logPath();
        Logger::instance().configure(path, Logger::Level::Warning);

        LOG_DEBUG("test", "quiet debug line");
        LOG_INFO("test", "quiet info line");
        LOG_WARNING("test", "loud warning line");
        LOG_ERROR("test", "loud error line");

        const QString content = readAll(path);
        QVERIFY(!content.contains("quiet debug line"));
        QVERIFY(!content.contains("quiet info line"));
        QVERIFY(content.contains("[warning] test: loud warning line"));
        QVERIFY(content.contains("[error] test: loud error line"));
    }

    //! Passing rotateBytes shrinks the rotation threshold: after enough
    //! lines the active file shifts to .1 and .2, never growing a .3
    //! with only two generations kept.
    void sizeRotation()
    {
        const QString path = m_dir.filePath(QStringLiteral("rotate.log"));
        Logger::instance().configure(path, Logger::Level::Info,
                                     /*rotateBytes=*/400, /*keepFiles=*/2);

        // ~20 lines x ~40 bytes each - enough for two shifts.
        for (int i = 0; i < 20; ++i)
            LOG_INFO("bench", QStringLiteral("request get_menu_items 1.234ms #%1").arg(i));

        QVERIFY(QFile::exists(path));                  // active file, fresh
        QVERIFY(QFile::exists(path + ".1"));           // first generation
        QVERIFY(QFile::exists(path + ".2"));           // second generation
        QVERIFY(!QFile::exists(path + ".3"));          // bounded by keepFiles

        const QString active = readAll(path);
        QVERIFY(active.contains("#19"));               // newest line survives
        QVERIFY(!QFile::exists(path + ".3"));
    }

    //! An empty file path means stderr-only logging; nothing is written
    //! to disk and the logger stays fully functional.
    void stderrOnlyMode()
    {
        const QString missing = m_dir.filePath(QStringLiteral("never_created.log"));
        Logger::instance().configure(QString(), Logger::Level::Info);
        QVERIFY(!Logger::instance().writesToFile());

        LOG_INFO("test", "this only goes to stderr");
        QVERIFY(!QFile::exists(missing));
    }

    //! The --log-level parser accepts the documented spellings and falls
    //! back to info for anything unknown (a typo must never mute the
    //! server).
    void levelParsing()
    {
        QCOMPARE(Logger::levelFromString(QStringLiteral("debug")),
                 Logger::Level::Debug);
        QCOMPARE(Logger::levelFromString(QStringLiteral("INFO")),
                 Logger::Level::Info);
        QCOMPARE(Logger::levelFromString(QStringLiteral("warn")),
                 Logger::Level::Warning);
        QCOMPARE(Logger::levelFromString(QStringLiteral("error")),
                 Logger::Level::Error);
        QCOMPARE(Logger::levelFromString(QStringLiteral("nonsense")),
                 Logger::Level::Info);
    }
};

QTEST_MAIN(LoggerTests)
#include "logger_tests.moc"
