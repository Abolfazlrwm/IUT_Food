//! Unit tests for user accounts: registration, login, blocking and the
//! transparent legacy-SHA-256 -> PBKDF2 password migration.
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

#include "databasehandler.h"
#include "passwordhasher.h"

class UsersTests : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        auto &db = DataBaseHandler::instance();
        QVERIFY(db.openDataBase(m_dir.filePath(QStringLiteral("users_test.db"))));
        QVERIFY(db.createTables());
    }

    void registrationAndLogin()
    {
        auto &db = DataBaseHandler::instance();
        QVERIFY(db.registerUser(QStringLiteral("alice"), QStringLiteral("wonderland"),
                                QStringLiteral("customer")));
        QVERIFY(!db.registerUser(QStringLiteral("alice"), QStringLiteral("other"),
                                 QStringLiteral("customer"))); // duplicate

        QString role;
        int id = -1;
        QVERIFY(db.loginUser(QStringLiteral("alice"), QStringLiteral("wonderland"),
                             role, id));
        QCOMPARE(role, QStringLiteral("customer"));
        QCOMPARE(id, db.getUserId(QStringLiteral("alice")));
        QCOMPARE(db.getUserRole(QStringLiteral("alice")), QStringLiteral("customer"));

        QVERIFY(!db.loginUser(QStringLiteral("alice"), QStringLiteral("wrong-pw"),
                              role, id));
        QVERIFY(!db.loginUser(QStringLiteral("ghost"), QStringLiteral("whatever"),
                              role, id));
    }

    //! Registered passwords are stored salted — never in plain text and
    //! never as a bare unsalted digest.
    void storedHashIsPbkdf2()
    {
        auto &db = DataBaseHandler::instance();
        QVERIFY(db.registerUser(QStringLiteral("bob"), QStringLiteral("builder"),
                                QStringLiteral("customer")));
        QSqlQuery q(QSqlDatabase::database());
        q.prepare(QStringLiteral("SELECT password FROM users WHERE username = :u"));
        q.bindValue(QStringLiteral(":u"), QStringLiteral("bob"));
        QVERIFY(q.exec() && q.next());
        const QString stored = q.value(0).toString();
        QVERIFY(PasswordHasher::isPbkdf2(stored));
        QVERIFY(!stored.contains(QStringLiteral("builder")));
    }

    //! The v2.0 -> v2.1 story: a row carrying a bare SHA-256 digest logs in
    //! fine and is transparently re-hashed to PBKDF2 on that first login.
    void legacyHashIsMigratedOnLogin()
    {
        auto &db = DataBaseHandler::instance();
        QVERIFY(db.registerUser(QStringLiteral("carol"), QStringLiteral("temp-pw"),
                                QStringLiteral("customer")));

        // Simulate a pre-v2.1 row.
        QSqlQuery q(QSqlDatabase::database());
        q.prepare(QStringLiteral("UPDATE users SET password = :p WHERE username = :u"));
        q.bindValue(QStringLiteral(":p"),
                    PasswordHasher::legacySha256(QStringLiteral("temp-pw")));
        q.bindValue(QStringLiteral(":u"), QStringLiteral("carol"));
        QVERIFY(q.exec());

        QString role;
        int id = -1;
        QVERIFY(db.loginUser(QStringLiteral("carol"), QStringLiteral("temp-pw"),
                             role, id));
        QVERIFY(id > 0);

        // The stored hash has been upgraded in place.
        QVERIFY(q.exec(QStringLiteral("SELECT password FROM users WHERE id = ") +
                       QString::number(id)));
        QVERIFY(q.next());
        QVERIFY2(PasswordHasher::isPbkdf2(q.value(0).toString()),
                 "legacy hash must be replaced by PBKDF2 after first login");

        // And the account keeps working with the same password.
        QVERIFY(db.loginUser(QStringLiteral("carol"), QStringLiteral("temp-pw"),
                             role, id));
        QVERIFY(!db.loginUser(QStringLiteral("carol"), QStringLiteral("temp-p"),
                              role, id));
    }

    void blockUnblockCycle()
    {
        auto &db = DataBaseHandler::instance();
        QVERIFY(db.registerUser(QStringLiteral("dave"), QStringLiteral("pw"),
                                QStringLiteral("customer")));
        int id = db.getUserId(QStringLiteral("dave"));

        QString role;
        QVERIFY(db.loginUser(QStringLiteral("dave"), QStringLiteral("pw"), role, id));
        QVERIFY(db.blockUser(id));
        QVERIFY(!db.loginUser(QStringLiteral("dave"), QStringLiteral("pw"), role, id));
        QVERIFY(db.unblockUser(id));
        QVERIFY(db.loginUser(QStringLiteral("dave"), QStringLiteral("pw"), role, id));
    }

    void customerProfileFields()
    {
        auto &db = DataBaseHandler::instance();
        QVERIFY(db.registerCustomer(QStringLiteral("erin"), QStringLiteral("pw"),
                                    QStringLiteral("Erin E."),
                                    QStringLiteral("Tehran, Vanak")));
        QSqlQuery details = db.getUserDetails(db.getUserId(QStringLiteral("erin")));
        QVERIFY(details.next());
        QCOMPARE(details.value(QStringLiteral("name")).toString(),
                 QStringLiteral("Erin E."));
        QCOMPARE(details.value(QStringLiteral("address")).toString(),
                 QStringLiteral("Tehran, Vanak"));

        QVERIFY(db.updateUserDetails(db.getUserId(QStringLiteral("erin")),
                                     QStringLiteral("erin_updated"),
                                     QStringLiteral("Tehran, Niavaran")));
        // updateUserDetails renames the login name; the display name the
        // profile dialog shows is carried in `name` and survives the edit.
        QCOMPARE(db.getUserName(db.getUserId(QStringLiteral("erin_updated"))),
                 QStringLiteral("erin_updated"));
        QSqlQuery renamed(QSqlDatabase::database());
        QVERIFY(renamed.exec(QStringLiteral("SELECT name FROM users WHERE username = 'erin_updated'")));
        QVERIFY(renamed.next());
        QCOMPARE(renamed.value(0).toString(), QStringLiteral("Erin E."));
        QCOMPARE(db.getUserName(db.getUserId(QStringLiteral("ghost_username"))),
                 QString()); // unknown id resolves to an empty name
    }
};

QTEST_MAIN(UsersTests)
#include "users_tests.moc"
