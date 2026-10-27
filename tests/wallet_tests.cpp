//! Unit tests for the wallet: top-up, atomic payment, overdraw guard
//! and the transaction audit trail.  Runs against a scratch SQLite file.
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

#include "databasehandler.h"

class WalletTests : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;

    //! The user id of the freshly registered wallet_owner row.
    int ownerId()
    {
        const int id =
            DataBaseHandler::instance().getUserId(QStringLiteral("wallet_owner"));
        if (id <= 0)
            QTest::qFail("wallet_owner must be registered in initTestCase",
                         __FILE__, __LINE__);
        return id;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        auto &db = DataBaseHandler::instance();
        QVERIFY(db.openDataBase(m_dir.filePath(QStringLiteral("wallet_test.db"))));
        QVERIFY(db.createTables());
        QVERIFY(db.registerUser(QStringLiteral("wallet_owner"), QStringLiteral("pw"),
                                QStringLiteral("customer")));
    }

    void cleanup()
    {
        // Reset the ledger for the next test, but keep the user itself:
        // ownerId() is shared by every test in this suite.
        QSqlQuery q(QSqlDatabase::database());
        q.exec(QStringLiteral("DELETE FROM wallet_transactions"));
        q.exec(QStringLiteral("UPDATE users SET balance = 0 "
                              "WHERE username = 'wallet_owner'"));
    }

    void chargeIncreasesBalance()
    {
        auto &db = DataBaseHandler::instance();
        const int id = ownerId();
        QCOMPARE(db.getWalletBalance(id), 0.0);

        QVERIFY(db.chargeWallet(id, 100'000.0, QStringLiteral("test top-up")));
        QCOMPARE(db.getWalletBalance(id), 100'000.0);
        QVERIFY(db.chargeWallet(id, 25'500.0));
        QCOMPARE(db.getWalletBalance(id), 125'500.0);
    }

    void rejectsInvalidCharges()
    {
        auto &db = DataBaseHandler::instance();
        const int id = ownerId();
        const double before = db.getWalletBalance(id);

        QVERIFY(!db.chargeWallet(id, 0.0));
        QVERIFY(!db.chargeWallet(id, -5'000.0));
        QVERIFY(!db.chargeWallet(-1, 10'000.0));          // unknown user
        QVERIFY(!db.chargeWallet(999'999, 10'000.0));     // never registered
        QCOMPARE(db.getWalletBalance(id), before);
    }

    //! payFromWallet must be atomic: either the full amount moves, or
    //! nothing changes — and the balance can never go negative.
    void paymentIsAtomicAndCannotOverdraw()
    {
        auto &db = DataBaseHandler::instance();
        const int id = ownerId();
        QVERIFY(db.chargeWallet(id, 60'000.0));

        QVERIFY(db.payFromWallet(id, 40'000.0, 1));
        QCOMPARE(db.getWalletBalance(id), 20'000.0);

        // Overdraw attempt: rejected, balance untouched.
        QVERIFY(!db.payFromWallet(id, 20'001.0, 2));
        QCOMPARE(db.getWalletBalance(id), 20'000.0);

        // The exact remaining balance may be spent down to zero.
        QVERIFY(db.payFromWallet(id, 20'000.0, 3));
        QCOMPARE(db.getWalletBalance(id), 0.0);

        QVERIFY(!db.payFromWallet(id, 1.0, 4));
        QVERIFY(!db.payFromWallet(-3, 1.0, 5));
        QVERIFY(!db.payFromWallet(id, 0.0, 6));
    }

    //! Every top-up and payment leaves one auditable row.
    void transactionLogIsWritten()
    {
        auto &db = DataBaseHandler::instance();
        const int id = ownerId();
        QVERIFY(db.chargeWallet(id, 50'000.0, QStringLiteral("note-a")));
        QVERIFY(db.payFromWallet(id, 10'000.0, 42));

        QSqlQuery tx = db.getWalletTransactions(id);
        int topups = 0, payments = 0;
        while (tx.next()) {
            if (tx.value(QStringLiteral("kind")).toString() == QLatin1String("topup"))
                ++topups;
            if (tx.value(QStringLiteral("kind")).toString() == QLatin1String("payment"))
                ++payments;
        }
        QCOMPARE(topups, 1);
        QCOMPARE(payments, 1);

        // Re-open the query to check the note column survived.
        QSqlQuery tx2 = db.getWalletTransactions(id);
        bool sawNote = false;
        while (tx2.next())
            sawNote |= tx2.value(QStringLiteral("note")).toString() == QLatin1String("note-a");
        QVERIFY(sawNote);
    }
};

QTEST_MAIN(WalletTests)
#include "wallet_tests.moc"
