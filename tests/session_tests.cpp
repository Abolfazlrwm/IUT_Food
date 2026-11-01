//! Unit tests for the v2.2 server-side SessionManager: token minting,
//! sliding expiry, logout revocation, sweeping and the store cap.
#include <QSet>
#include <QtTest>

#include "sessionmanager.h"

class SessionTests : public QObject
{
    Q_OBJECT

private slots:
    //! Tokens are 64 lowercase hex characters (256-bit) and unique.
    void tokenShapeAndUniqueness()
    {
        QSet<QString> seen;
        for (int i = 0; i < 500; ++i) {
            const QString token = SessionManager::mintToken();
            QCOMPARE(token.size(), 64);
            for (const QChar &c : token)
                QVERIFY2(c.isDigit() || (c >= QLatin1Char('a') && c <= QLatin1Char('f')),
                         qPrintable(QStringLiteral("non-hex char: %1").arg(c)));
            QVERIFY(!seen.contains(token));
            seen.insert(token);
        }
    }

    //! An opened session resolves back to the identity it was opened for.
    void openAndResolve()
    {
        SessionManager sm;
        const QString token = sm.openSession("demo_customer", "customer", 2, 1000);
        SessionManager::Session s;
        QVERIFY(sm.resolve(token, &s, 2000));
        QCOMPARE(s.username, QStringLiteral("demo_customer"));
        QCOMPARE(s.role, QStringLiteral("customer"));
        QCOMPARE(s.userId, 2);
    }

    //! Unknown or malformed tokens are rejected.
    void unknownTokenRejected()
    {
        SessionManager sm;
        const QString token = sm.openSession("admin", "admin", 1, 1000);
        SessionManager::Session s;
        QVERIFY(!sm.resolve(QString(), &s, 2000));
        QVERIFY(!sm.resolve(QStringLiteral("deadbeef"), &s, 2000));
        QVERIFY(!sm.resolve(token + QStringLiteral("0"), &s, 2000)); // 65 chars
    }

    //! An idle session dies after the lifetime; an active one survives.
    void expiryAfterLifetime()
    {
        SessionManager sm(60 * 1000); // 1 minute lifetime
        const QString token = sm.openSession("demo_customer", "customer", 2, 0);

        SessionManager::Session s;
        QVERIFY(sm.resolve(token, &s, 60 * 1000));       // exactly at the edge
        // That resolve refreshed the window, so it now ends one lifetime
        // later - one ms past THAT boundary the token is dead.
        QVERIFY(!sm.resolve(token, &s, 120 * 1000 + 1));

        // A fresh session opened later is unaffected.
        const QString fresh = sm.openSession("admin", "admin", 1, 130 * 1000);
        QVERIFY(sm.resolve(fresh, &s, 160 * 1000));
    }

    //! Every accepted request refreshes the sliding window.
    void slidingWindowRefresh()
    {
        SessionManager sm(100 * 1000);
        const QString token = sm.openSession("demo_customer", "customer", 2, 0);

        // Keep using it every 90 s for 20 rounds = 30 minutes total,
        // far beyond the 100 s idle lifetime.
        qint64 now = 0;
        for (int i = 0; i < 20; ++i) {
            now += 90 * 1000;
            SessionManager::Session s;
            QVERIFY2(sm.resolve(token, &s, now),
                     qPrintable(QStringLiteral("died after round %1").arg(i)));
        }
    }

    //! Logout revokes the token immediately - also for a second resolve.
    void revokeOnLogout()
    {
        SessionManager sm;
        const QString token = sm.openSession("demo_customer", "customer", 2, 1000);
        QVERIFY(sm.revoke(token));
        SessionManager::Session s;
        QVERIFY(!sm.resolve(token, &s, 1100));
        QVERIFY(!sm.revoke(token)); // already gone - no double-logout success
    }

    //! sweep() drops exactly the expired sessions and keeps the rest.
    void sweepRemovesOnlyExpired()
    {
        SessionManager sm(100 * 1000);
        const QString keep = sm.openSession("admin", "admin", 1, 0);
        const QString drop = sm.openSession("demo_customer", "customer", 2, 0);
        const QString fresh = sm.openSession("r1", "restaurant", 3, 90 * 1000);

        sm.resolve(keep, nullptr, 90 * 1000); // refresh: lastSeen 90 s

        QCOMPARE(sm.sweep(150 * 1000), 1); // only `drop` (idle since 0)
        SessionManager::Session s;
        QVERIFY(sm.resolve(keep, &s, 160 * 1000));   // 70 s idle - alive
        QVERIFY(sm.resolve(fresh, &s, 160 * 1000));  // 70 s idle - alive
        QVERIFY(!sm.resolve(drop, &s, 160 * 1000));  // swept
    }

    //! activeCount() counts only sessions inside their window.
    void activeCount()
    {
        SessionManager sm(100 * 1000);
        sm.openSession("a", "admin", 1, 0);
        sm.openSession("b", "customer", 2, 0);
        QCOMPARE(sm.activeCount(10 * 1000), 2);
        QCOMPARE(sm.activeCount(150 * 1000), 0);
    }

    //! The store never grows beyond maxSessions (login flood guard).
    void capEvictsOldest()
    {
        SessionManager sm(60 * 60 * 1000, 8);
        QString newest;
        for (int i = 0; i < 8; ++i)
            sm.openSession(QStringLiteral("u%1").arg(i), "customer", i, i * 1000);
        newest = sm.openSession("late", "customer", 99, 100 * 1000);

        QCOMPARE(sm.size(), 8);
        SessionManager::Session s;
        QVERIFY(sm.resolve(newest, &s, 101 * 1000)); // the newest survived
        QVERIFY(!sm.resolve("u0", &s, 101 * 1000));  // the oldest was evicted
    }

    //! totalOpened() counts every successful open, evicted or not.
    void totalOpenedCounter()
    {
        SessionManager sm(60 * 60 * 1000, 4);
        for (int i = 0; i < 10; ++i)
            sm.openSession(QStringLiteral("u%1").arg(i), "customer", i, i);
        QCOMPARE(quint64(sm.totalOpened()), quint64(10));
    }
};

QTEST_MAIN(SessionTests)
#include "session_tests.moc"
