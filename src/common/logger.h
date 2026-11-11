#ifndef LOGGER_H
#define LOGGER_H

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QString>
#include <QTextStream>

//! Minimal rotating file logger (v2.4).  Everything the server wants to
//! remember - handshakes, request lines, rejections, shutdowns - lands
//! in a size-rotated log file so a long-running deployment can be
//! debugged after the fact.  Lines are always mirrored to stderr, so
//! the console demo experience is unchanged.
//!
//! Levels: debug < info < warning < error.  A configured minimum level
//! filters out anything quieter (per-request lines are logged as debug,
//! so `--log-level info` keeps the file readable while
//! `--log-level debug` gives the full trace).
//!
//! Rotation is by size: once the active file reaches rotateBytes it is
//! shifted to `<name>.1`, the old `.1` becomes `.2`, and so on up to
//! keepFiles generations.  Header-only on purpose, matching
//! passwordhash.h / ratelimiter.h / sessionmanager.h.
class Logger
{
public:
    enum class Level { Debug = 0, Info = 1, Warning = 2, Error = 3 };

    static Logger &instance()
    {
        static Logger logger;
        return logger;
    }

    //! \p filePath may be empty: the logger then only writes to stderr.
    void configure(const QString &filePath, Level minLevel,
                   qint64 rotateBytes = 5 * 1024 * 1024, int keepFiles = 3)
    {
        QMutexLocker lock(&m_mutex);
        m_minLevel = minLevel;
        m_maxBytes = qMax<qint64>(256, rotateBytes); // guard rail, small enough for tests
        m_keepFiles = qMax(1, keepFiles);
        openFileLocked(filePath);
    }

    void log(Level level, const QString &component, const QString &message)
    {
        if (level < m_minLevel)
            return;
        const QString line = QStringLiteral("%1 [%2] %3: %4")
                .arg(QDateTime::currentDateTime().toString(
                         QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz")),
                     QString::fromLatin1(levelName(level)), component, message);
        QMutexLocker lock(&m_mutex);
        QTextStream(stderr) << line << '\n';
        if (m_file.isOpen()) {
            m_stream << line << '\n';
            m_stream.flush();
            if (m_file.size() >= m_maxBytes)
                rotateLocked();
        }
    }

    //! True while a log file is attached (used by the unit tests).
    bool writesToFile() const
    {
        QMutexLocker lock(&m_mutex);
        return m_file.isOpen();
    }

    static const char *levelName(Level level)
    {
        switch (level) {
        case Level::Debug:   return "debug";
        case Level::Info:    return "info";
        case Level::Warning: return "warning";
        case Level::Error:   return "error";
        }
        return "info";
    }

    //! Accepts the spellings offered on the command line; unknown values
    //! fall back to info so a typo never silences the server.
    static Level levelFromString(const QString &value)
    {
        const QString v = value.trimmed().toLower();
        if (v == QLatin1String("debug"))   return Level::Debug;
        if (v == QLatin1String("warning") || v == QLatin1String("warn"))
            return Level::Warning;
        if (v == QLatin1String("error"))   return Level::Error;
        return Level::Info;
    }

private:
    Logger() = default;
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    void openFileLocked(const QString &filePath)
    {
        if (m_file.isOpen()) {
            m_stream.setDevice(nullptr);
            m_file.close();
        }
        m_file.setFileName(filePath);
        if (filePath.isEmpty())
            return; // stderr-only mode
        QDir().mkpath(QFileInfo(filePath).absolutePath());
        if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append
                         | QIODevice::Text))
            return; // unwritable path: stay on stderr instead of dying
        m_stream.setDevice(&m_file);
    }

    //! Requires m_mutex.  Shifts name -> name.1 -> ... -> name.N and
    //! reopens a fresh active file.
    void rotateLocked()
    {
        m_stream.setDevice(nullptr);
        m_file.close();
        const QString base = m_file.fileName();
        for (int i = m_keepFiles - 1; i >= 1; --i) {
            const QString from = QStringLiteral("%1.%2").arg(base).arg(i);
            const QString to = QStringLiteral("%1.%2").arg(base).arg(i + 1);
            QFile::remove(to);
            QFile::rename(from, to);
        }
        QFile::remove(base + QStringLiteral(".1"));
        QFile::rename(base, base + QStringLiteral(".1"));
        openFileLocked(base);
    }

    mutable QMutex m_mutex; // guards state also for the const test accessor
    QFile m_file;
    QTextStream m_stream;
    Level m_minLevel = Level::Info;
    qint64 m_maxBytes = 5 * 1024 * 1024;
    int m_keepFiles = 3;
};

//! Short call-site helpers: LOG_INFO("server", "listening on 1234").
#define LOG_DEBUG(component, message) \
    Logger::instance().log(Logger::Level::Debug, (component), (message))
#define LOG_INFO(component, message) \
    Logger::instance().log(Logger::Level::Info, (component), (message))
#define LOG_WARNING(component, message) \
    Logger::instance().log(Logger::Level::Warning, (component), (message))
#define LOG_ERROR(component, message) \
    Logger::instance().log(Logger::Level::Error, (component), (message))

#endif // LOGGER_H
