#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

#include <QHash>
#include <QRandomGenerator>
#include <QString>
#include <QtGlobal>

#include <atomic>

/*!
 * Server-side session store for the v2.2 authenticated protocol.
 *
 * A successful login mints a 256-bit random session token.  Every
 * subsequent request must present that token; the server resolves it
 * back to (userId, role, username) instead of trusting client-sent ids.
 *
 * Properties:
 *   - Tokens come from QRandomGenerator::system() (the OS CSPRNG:
 *     getrandom(2) / arc4random), 32 bytes hex-encoded -> 64 chars.
 *   - Sliding expiry: every accepted request refreshes lastSeen, so an
 *     active session is not dropped mid-use; an idle one dies after
 *     lifetimeMs (default 30 minutes).
 *   - revoke() implements logout; sweep() drops expired sessions in
 *     bulk; the store is capped so thousands of logins cannot grow it
 *     without bound (oldest-inserted entries are evicted).
 *   - Time is injected by the caller (milliseconds), which keeps the
 *     class deterministic and directly unit-testable.
 *
 * Sessions live in server memory only: a restart simply means the
 * clients log in again, and nothing about a session is persisted to
 * the SQLite file.
 */
class SessionManager
{
public:
    struct Session
    {
        QString username;
        QString role;
        int userId = -1;
        qint64 lastSeenMs = 0;
    };

    explicit SessionManager(qint64 lifetimeMs = DefaultLifetimeMs, int maxSessions = 512)
        : m_lifetimeMs(lifetimeMs), m_maxSessions(maxSessions)
    {
        Q_ASSERT(lifetimeMs > 0);
        Q_ASSERT(maxSessions > 0);
    }

    //! Mints a new token for \p identity and returns it.
    QString openSession(const QString &username, const QString &role,
                        int userId, qint64 nowMs)
    {
        // Keep the store bounded even under a login flood: the oldest
        // entry (by insertion order, QHash iteration is fine here since
        // exact fairness does not matter for an eviction heuristic) is
        // dropped once the cap is reached.
        if (m_sessions.size() >= m_maxSessions) {
            const QString victim = oldestToken(nowMs);
            if (!victim.isEmpty())
                m_sessions.remove(victim);
        }

        const QString token = mintToken();
        Session s;
        s.username = username;
        s.role = role;
        s.userId = userId;
        s.lastSeenMs = nowMs;
        m_sessions.insert(token, s);
        m_opened.fetch_add(1, std::memory_order_relaxed);
        return token;
    }

    //! Validates \p token at \p nowMs.  A hit refreshes the sliding
    //! window and optionally hands the identity back.
    bool resolve(const QString &token, Session *identity, qint64 nowMs)
    {
        const auto it = m_sessions.find(token);
        if (it == m_sessions.end())
            return false;
        if (nowMs - it->lastSeenMs > m_lifetimeMs) {
            m_sessions.erase(it); // expired on arrival
            return false;
        }
        it->lastSeenMs = nowMs;
        if (identity)
            *identity = *it;
        return true;
    }

    //! Logout: the token becomes unusable immediately.
    bool revoke(const QString &token) { return m_sessions.remove(token) > 0; }

    //! Drops every session idle for more than the lifetime.  Called
    //! periodically by the server; returns the number of removals.
    int sweep(qint64 nowMs)
    {
        int removed = 0;
        for (auto it = m_sessions.begin(); it != m_sessions.end();) {
            if (nowMs - it->lastSeenMs > m_lifetimeMs) {
                it = m_sessions.erase(it);
                ++removed;
            } else {
                ++it;
            }
        }
        return removed;
    }

    int activeCount(qint64 nowMs) const
    {
        int n = 0;
        for (auto it = m_sessions.constBegin(); it != m_sessions.constEnd(); ++it)
            if (nowMs - it->lastSeenMs <= m_lifetimeMs)
                ++n;
        return n;
    }

    void setLifetimeMs(qint64 lifetimeMs)
    {
        Q_ASSERT(lifetimeMs > 0);
        m_lifetimeMs = lifetimeMs;
    }
    qint64 lifetimeMs() const { return m_lifetimeMs; }
    int size() const { return m_sessions.size(); }

    //! Total sessions ever opened (server-stats counter).
    quint64 totalOpened() const { return m_opened.load(std::memory_order_relaxed); }

    //! Default inactivity lifetime: 30 minutes.
    static constexpr qint64 DefaultLifetimeMs = 30 * qint64(60) * 1000;

    static QString mintToken()
    {
        // 32 bytes from the operating system CSPRNG, hex encoded.
        QString hex;
        hex.reserve(64);
        QRandomGenerator *rng = QRandomGenerator::system();
        for (int i = 0; i < 8; ++i) {
            quint32 word = rng->generate();
            hex += QStringLiteral("%1").arg(word, 8, 16, QLatin1Char('0'));
        }
        return hex;
    }

private:
    QString oldestToken(qint64 nowMs) const
    {
        qint64 best = nowMs + 1;
        QString token;
        for (auto it = m_sessions.constBegin(); it != m_sessions.constEnd(); ++it) {
            if (it->lastSeenMs < best) {
                best = it->lastSeenMs;
                token = it.key();
            }
        }
        return token;
    }

    QHash<QString, Session> m_sessions;
    qint64 m_lifetimeMs = DefaultLifetimeMs;
    int m_maxSessions = 512;
    std::atomic<quint64> m_opened{0};
};

#endif // SESSIONMANAGER_H
