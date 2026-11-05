//! Unit tests for the token-bucket RateLimiter used by the TCP server.
#include <QtTest>

#include "ratelimiter.h"

class RateLimiterTests : public QObject
{
    Q_OBJECT

private slots:
    //! A fresh bucket admits exactly `burst` requests and then denies,
    //! even when all of them arrive at the same millisecond.
    void burstCapacity()
    {
        RateLimiter limiter(5, 10.0);
        for (int i = 0; i < 5; ++i)
            QVERIFY2(limiter.allow(1000), qPrintable(QStringLiteral("request %1").arg(i)));
        QVERIFY(!limiter.allow(1000));
        QVERIFY(!limiter.allow(1000));
    }

    //! Tokens refill over time at refillPerSecond.
    void refillOverTime()
    {
        RateLimiter limiter(10, 10.0); // 1 token per 100 ms
        for (int i = 0; i < 10; ++i)
            QVERIFY(limiter.allow(0));
        QVERIFY(!limiter.allow(0));

        // 500 ms pass -> 5 tokens available.
        QVERIFY(limiter.allow(500));
        QVERIFY(limiter.allow(500));
        QVERIFY(limiter.allow(500));
        QVERIFY(limiter.allow(500));
        QVERIFY(limiter.allow(500));
        QVERIFY(!limiter.allow(500)); // drained again
    }

    //! Refilling never exceeds the burst capacity, no matter how long
    //! the connection idles.
    void refillCappedAtBurst()
    {
        RateLimiter limiter(4, 50.0);
        for (int i = 0; i < 4; ++i)
            QVERIFY(limiter.allow(0));
        QVERIFY(!limiter.allow(0));

        QVERIFY(limiter.allow(60'000)); // an idle minute refills to the cap
        QVERIFY(limiter.allow(60'000));
        QVERIFY(limiter.allow(60'000));
        QVERIFY(limiter.allow(60'000));
        QVERIFY(!limiter.allow(60'000));
    }

    //! Repeated calls with a frozen clock only drain the initial bucket.
    void frozenClockDrains()
    {
        RateLimiter limiter(3, 25.0);
        QVERIFY(limiter.allow(7));
        QVERIFY(limiter.allow(7));
        QVERIFY(limiter.allow(7));
        QVERIFY(!limiter.allow(7));
        QVERIFY(!limiter.allow(7));
    }

    //! The realistic flood pattern: 60-request burst admitted, then the
    //! connection is throttled — matches the server defaults.
    void serverDefaultsShedFloods()
    {
        RateLimiter limiter(60, 25.0);
        int admitted = 0;
        for (int i = 0; i < 500; ++i) // a flood with no gaps
            admitted += limiter.allow(10'000) ? 1 : 0;
        QCOMPARE(admitted, 60);
    }
};

QTEST_MAIN(RateLimiterTests)
#include "ratelimiter_tests.moc"
