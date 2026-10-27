//! Unit tests for coupon rules: creation guards, percent/amount math,
//! inactivity, expiry, usage limits and minimum order.
#include <QtTest>

#include "databasehandler.h"

class CouponTests : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        auto &db = DataBaseHandler::instance();
        QVERIFY(db.openDataBase(m_dir.filePath(QStringLiteral("coupon_test.db"))));
        QVERIFY(db.createTables());
    }

    void creationGuards()
    {
        auto &db = DataBaseHandler::instance();
        QVERIFY(db.createCoupon(QStringLiteral("P20"), QStringLiteral("percent"), 20.0,
                                100'000.0, QString(), 5));
        QVERIFY(!db.createCoupon(QString(), QStringLiteral("percent"), 10.0, 0.0,
                                 QString(), 0));                              // no code
        QVERIFY(!db.createCoupon(QStringLiteral("   "), QStringLiteral("percent"),
                                 10.0, 0.0, QString(), 0));
        QVERIFY(!db.createCoupon(QStringLiteral("Z"), QStringLiteral("percent"),
                                 0.0, 0.0, QString(), 0));  // no value
        QVERIFY(!db.createCoupon(QStringLiteral("Z"), QStringLiteral("percent"),
                                 101.0, 0.0, QString(), 0)); // >100%
        QVERIFY(!db.createCoupon(QStringLiteral("Z"), QStringLiteral("weird"),
                                 10.0, 0.0, QString(), 0));    // bad kind
    }

    //! Codes are trimmed and upper-cased, so the client can send any case.
    void codeIsNormalized()
    {
        auto &db = DataBaseHandler::instance();
        QVERIFY(db.createCoupon(QStringLiteral("  mixedcase  "), QStringLiteral("percent"),
                                10.0, 0.0, QString(), 0));
        const CouponResult r = db.validateCoupon(QStringLiteral("MixedCase"), 50'000.0);
        QVERIFY(r.ok);
        QCOMPARE(r.discount, 5'000.0);
    }

    void percentAndAmountMath()
    {
        auto &db = DataBaseHandler::instance();
        QVERIFY(db.createCoupon(QStringLiteral("CPCT"), QStringLiteral("percent"), 25.0,
                                0.0, QString(), 0));
        const CouponResult pct = db.validateCoupon(QStringLiteral("CPCT"), 80'000.0);
        QVERIFY(pct.ok);
        QCOMPARE(pct.discount, 20'000.0);

        QVERIFY(db.createCoupon(QStringLiteral("CAMT"), QStringLiteral("amount"), 15'000.0,
                                0.0, QString(), 0));
        const CouponResult amt = db.validateCoupon(QStringLiteral("CAMT"), 80'000.0);
        QVERIFY(amt.ok);
        QCOMPARE(amt.discount, 15'000.0);

        //! A fixed discount can never exceed the order total.
        const CouponResult capped = db.validateCoupon(QStringLiteral("CAMT"), 9'000.0);
        QVERIFY(capped.ok);
        QCOMPARE(capped.discount, 9'000.0);
    }

    void unknownCodeIsRejected()
    {
        const CouponResult r =
            DataBaseHandler::instance().validateCoupon(QStringLiteral("NOPE"), 50'000.0);
        QVERIFY(!r.ok);
        QVERIFY(r.couponId < 0);
        QVERIFY(!r.message.isEmpty());
    }

    void inactiveCouponIsRejected()
    {
        auto &db = DataBaseHandler::instance();
        QVERIFY(db.createCoupon(QStringLiteral("SLEEP"), QStringLiteral("percent"), 30.0,
                                0.0, QString(), 0));
        const int id = db.findCouponId(QStringLiteral("SLEEP"));
        QVERIFY(id > 0);
        QVERIFY(db.setCouponActive(id, false));
        QVERIFY(!db.validateCoupon(QStringLiteral("SLEEP"), 50'000.0).ok);
        QVERIFY(!db.consumeCoupon(id));
        QVERIFY(db.setCouponActive(id, true));
        QVERIFY(db.validateCoupon(QStringLiteral("SLEEP"), 50'000.0).ok);
    }

    void expiredCouponIsRejected()
    {
        auto &db = DataBaseHandler::instance();
        QVERIFY(db.createCoupon(QStringLiteral("OLDY"), QStringLiteral("percent"), 50.0,
                                0.0, QStringLiteral("2000-01-01 00:00:00"), 0));
        QVERIFY(!db.validateCoupon(QStringLiteral("OLDY"), 50'000.0).ok);
    }

    void minimumOrderIsEnforced()
    {
        auto &db = DataBaseHandler::instance();
        QVERIFY(db.createCoupon(QStringLiteral("MIN100"), QStringLiteral("percent"), 10.0,
                                100'000.0, QString(), 0));
        QVERIFY(db.validateCoupon(QStringLiteral("MIN100"), 100'000.0).ok);
        QVERIFY(!db.validateCoupon(QStringLiteral("MIN100"), 99'999.0).ok);
    }

    //! consumeCoupon() increments used_count and the limit then closes
    //! the code, mirroring what place_order does after a successful order.
    void usageLimitIsTracked()
    {
        auto &db = DataBaseHandler::instance();
        QVERIFY(db.createCoupon(QStringLiteral("ONCE"), QStringLiteral("percent"), 10.0,
                                0.0, QString(), 1));
        const int id = db.findCouponId(QStringLiteral("ONCE"));
        QVERIFY(id > 0);

        QVERIFY(db.validateCoupon(QStringLiteral("ONCE"), 10'000.0).ok);
        QVERIFY(db.consumeCoupon(id));
        QVERIFY(!db.consumeCoupon(id));           // second use denied
        QVERIFY(!db.validateCoupon(QStringLiteral("ONCE"), 10'000.0).ok);
    }
};

QTEST_MAIN(CouponTests)
#include "coupon_tests.moc"
