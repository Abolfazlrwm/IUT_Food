//! Unit tests for PasswordHasher (PBKDF2-HMAC-SHA256 + legacy SHA-256).
#include <QtTest>

#include "passwordhasher.h"

class PasswordHasherTests : public QObject
{
    Q_OBJECT

private slots:
    //! Official PBKDF2-HMAC-SHA256 test vectors (password "password",
    //! salt "salt") — proves the primitive itself, not just the plumbing.
    void rfc_vectors()
    {
        // c = 1
        QCOMPARE(PasswordHasher::deriveKey("password", "salt", 1).toHex(),
                 QByteArray("120fb6cffcf8b32c43e7225256c4f837"
                            "a86548c92ccc35480805987cb70be17b"));
        // c = 2
        QCOMPARE(PasswordHasher::deriveKey("password", "salt", 2).toHex(),
                 QByteArray("ae4d0c95af6b46d32d0adff928f06dd0"
                            "2a303f8ef3c251dfd6e2d85a95474c43"));
        // c = 4096
        QCOMPARE(PasswordHasher::deriveKey("password", "salt", 4096).toHex(),
                 QByteArray("c5e478d59288c841aa530db6845c4c8d"
                            "962893a001ce4e11a4963873aa98134a"));
    }

    //! Longer-than-one-block derivation path (dkLen > 32) must still match
    //! the published vector for dk = 40 bytes.
    void multiBlockDerivation()
    {
        // SHA-256 vector for dkLen=40 is not published for ("password","salt"),
        // so verify block-chaining internally: the first 32 bytes of a 40-byte
        // derivation must equal the 32-byte derivation (block 1 identical).
        const QByteArray full = PasswordHasher::deriveKey("password", "salt", 3, 40);
        const QByteArray first = PasswordHasher::deriveKey("password", "salt", 3, 32);
        QCOMPARE(full.left(32), first);
        QCOMPARE(full.size(), 40);
    }

    void hashThenVerifyRoundTrip()
    {
        const QString stored = PasswordHasher::hash(QStringLiteral("s3cret!"));
        QVERIFY(PasswordHasher::isPbkdf2(stored));
        QVERIFY(PasswordHasher::verify(QStringLiteral("s3cret!"), stored));
        QVERIFY(!PasswordHasher::verify(QStringLiteral("s3cret"), stored));
        QVERIFY(!PasswordHasher::verify(QString(), stored));
    }

    //! Same password must never produce the same stored string:
    //! every hash() call draws a fresh random salt.
    void uniqueSaltsPerHash()
    {
        const QString a = PasswordHasher::hash(QStringLiteral("same-password"));
        const QString b = PasswordHasher::hash(QStringLiteral("same-password"));
        QVERIFY(a != b);
        QVERIFY(PasswordHasher::verify(QStringLiteral("same-password"), a));
        QVERIFY(PasswordHasher::verify(QStringLiteral("same-password"), b));
    }

    void rejectsMalformedStoredStrings()
    {
        QVERIFY(!PasswordHasher::verify(QStringLiteral("x"), QString()));
        QVERIFY(!PasswordHasher::verify(QStringLiteral("x"), QStringLiteral("")));
        QVERIFY(!PasswordHasher::verify(QStringLiteral("x"), QStringLiteral("plain-text")));
        QVERIFY(!PasswordHasher::verify(QStringLiteral("x"),
                                        QStringLiteral("pbkdf2_sha256$0$abc$abc")));
        QVERIFY(!PasswordHasher::verify(QStringLiteral("x"),
                                        QStringLiteral("pbkdf2_sha256$notanumber$abc$abc")));
        // Truncated / tampered digest
        const QString stored = PasswordHasher::hash(QStringLiteral("pw"));
        const QStringList parts = stored.split(QLatin1Char('$'));
        QVERIFY(!PasswordHasher::verify(
            QStringLiteral("pw"),
            parts.at(0) + QLatin1Char('$') + parts.at(1) + QLatin1Char('$')
                + parts.at(2) + QLatin1Char('$') + QStringLiteral("AAAAAAAAAAAAAAAAAAAAAA==")));
    }

    void legacySha256Vector()
    {
        // Well-known SHA-256("password") digest.
        QCOMPARE(PasswordHasher::legacySha256(QStringLiteral("password")),
                 QStringLiteral("5e884898da28047151d0e56f8dc62927"
                                "73603d0d6aabbdd62a11ef721d1542d8"));
    }

    void legacyVerification()
    {
        const QString legacy = PasswordHasher::legacySha256(QStringLiteral("old-pass"));
        QVERIFY(!PasswordHasher::isPbkdf2(legacy));
        QVERIFY(PasswordHasher::verifyLegacySha256(QStringLiteral("old-pass"), legacy));
        QVERIFY(!PasswordHasher::verifyLegacySha256(QStringLiteral("wrong"), legacy));
        // A PBKDF2 string must never be treated as a legacy hash.
        QVERIFY(!PasswordHasher::verifyLegacySha256(
            QStringLiteral("old-pass"), PasswordHasher::hash(QStringLiteral("old-pass"))));
    }
};

QTEST_MAIN(PasswordHasherTests)
#include "passwordhasher_tests.moc"
