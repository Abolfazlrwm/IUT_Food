#ifndef AUDITLOG_H
#define AUDITLOG_H

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMutex>
#include <QMutexLocker>
#include <QString>
#include <QTextStream>

//! Append-only JSON Lines audit trail (v2.5), separate from the human
//! readable application log.  Every security-relevant action - logins,
//! admin moderation, wallet movements, order placement - lands as one
//! compact JSON object per line:
//!
//!   {"actor":"ali","extra":{"amount":25000},"kind":"wallet_topup",
//!    "ts":"2026-11-14 21:04:11.123"}
//!
//! One event per line keeps the file streamable: `jq`, `tail -f` and
//! any log shipper can consume it without a parser.  Rotation is by
//! size (same scheme as the v2.4 application logger: 5 MiB per file,
//! 3 generations kept as `.1`, `.2`, `.3`).  With no file configured
//! the audit trail is a no-op, so tests and minimal deployments pay
//! nothing.  Header-only, matching logger.h / backupmanager.h.
class AuditLog
{
public:
    static AuditLog &instance()
    {
        static AuditLog log;
        return log;
    }

    //! Empty \p filePath disables the trail (events are dropped).
    void configure(const QString &filePath,
                   qint64 rotateBytes = 5 * 1024 * 1024, int keepFiles = 3)
    {
        QMutexLocker lock(&m_mutex);
        m_maxBytes = qMax<qint64>(256, rotateBytes); // guard rail for tests
        m_keepFiles = qMax(1, keepFiles);
        openFileLocked(filePath);
    }

    //! Records one event.  \p kind is a stable snake_case identifier
    //! (login_ok, wallet_topup, ...), \p actor the acting username (or
    //! the peer address when no session exists), \p extra optional
    //! structured context (amounts, ids, the peer address).
    void event(const QString &kind, const QString &actor,
               const QJsonObject &extra = {})
    {
        QJsonObject record;
        record[QLatin1String("ts")] = QDateTime::currentDateTime().toString(
            QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz"));
        record[QLatin1String("kind")] = kind;
        record[QLatin1String("actor")] = actor;
        if (!extra.isEmpty())
            record[QLatin1String("extra")] = extra;

        const QString line = QString::fromUtf8(
            QJsonDocument(record).toJson(QJsonDocument::Compact));
        QMutexLocker lock(&m_mutex);
        if (!m_file.isOpen())
            return; // disabled or unwritable: drop quietly
        m_stream << line << '\n';
        m_stream.flush();
        if (m_file.size() >= m_maxBytes)
            rotateLocked();
    }

    //! True while an audit file is attached (used by the unit tests).
    bool writesToFile() const
    {
        QMutexLocker lock(&m_mutex);
        return m_file.isOpen();
    }

private:
    AuditLog() = default;
    AuditLog(const AuditLog &) = delete;
    AuditLog &operator=(const AuditLog &) = delete;

    void openFileLocked(const QString &filePath)
    {
        if (m_file.isOpen()) {
            m_stream.setDevice(nullptr);
            m_file.close();
        }
        m_file.setFileName(filePath);
        if (filePath.isEmpty())
            return; // disabled mode
        QDir().mkpath(QFileInfo(filePath).absolutePath());
        if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append
                         | QIODevice::Text))
            return; // unwritable path: stay disabled instead of dying
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
    qint64 m_maxBytes = 5 * 1024 * 1024;
    int m_keepFiles = 3;
};

//! Short call-site helper: AUDIT("login_ok", user, {{"ip", ip}}).
#define AUDIT(kind, actor, extra) \
    AuditLog::instance().event((kind), (actor), (extra))

#endif // AUDITLOG_H
