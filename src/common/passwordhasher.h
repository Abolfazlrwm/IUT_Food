#ifndef PASSWORDHASHER_H
#define PASSWORDHASHER_H

#include <QByteArray>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>
#include <QString>
#include <QtEndian>

/*!
 * Salted, slow password hashing: PBKDF2-HMAC-SHA256.
 *
 * Stored format (self-describing, so the iteration count can grow later):
 * \code
 *   pbkdf2_sha256$<iterations>$<salt base64>$<hash base64>
 * \endcode
 *
 * Databases created before v2.1 stored a bare, unsalted SHA-256 hex digest.
 * PasswordHasher::verify() rejects those (the format does not match), while
 * DataBaseHandler::loginUser() uses verifyLegacySha256() for them and
 * transparently re-hashes with PBKDF2 on the first successful login — so a
 * v2.0 iut_food.db upgrades itself in place, without a reset.
 *
 * Implemented on top of Qt's QMessageAuthenticationCode to keep the project
 * dependency-free (no OpenSSL/botan linkage for a hashing primitive).
 */
class PasswordHasher
{
public:
    //! ~90 ms per hash/verify on a modern laptop — a deliberate knob.
    static constexpr int DefaultIterations = 30000;
    static constexpr int SaltBytes = 16;
    static constexpr int HashBytes = 32;   // SHA-256 output

    //! Hash \p password with a fresh random salt.
    static QString hash(const QString &password, int iterations = DefaultIterations)
    {
        QByteArray salt(SaltBytes, Qt::Uninitialized);
        for (int i = 0; i < SaltBytes; ++i)
            salt[i] = static_cast<char>(QRandomGenerator::system()->generate() & 0xFF);
        const QByteArray key = deriveKey(password.toUtf8(), salt, iterations, HashBytes);
        return QStringLiteral("pbkdf2_sha256$%1$%2$%3")
            .arg(iterations)
            .arg(QString::fromLatin1(salt.toBase64()))
            .arg(QString::fromLatin1(key.toBase64()));
    }

    //! True when \p password matches the stored PBKDF2 string.
    //! Comparison is constant-time in the digest bytes.
    static bool verify(const QString &password, const QString &stored)
    {
        const QStringList parts = stored.split(QLatin1Char('$'));
        if (parts.size() != 4 || parts.at(0) != QLatin1String("pbkdf2_sha256"))
            return false;
        bool ok = false;
        const int iterations = parts.at(1).toInt(&ok);
        if (!ok || iterations < 1 || iterations > 10'000'000)
            return false;
        const QByteArray salt = QByteArray::fromBase64(parts.at(2).toLatin1());
        const QByteArray expected = QByteArray::fromBase64(parts.at(3).toLatin1());
        if (salt.size() < 8 || expected.size() != HashBytes)
            return false;
        const QByteArray actual = deriveKey(password.toUtf8(), salt, iterations, HashBytes);
        return constantTimeEquals(expected, actual);
    }

    //! True when the stored string is a PBKDF2 hash (rather than a legacy one).
    static bool isPbkdf2(const QString &stored)
    {
        return stored.startsWith(QLatin1String("pbkdf2_sha256$"));
    }

    //! The pre-v2.1 scheme: a bare unsalted SHA-256 hex digest.
    static QString legacySha256(const QString &password)
    {
        return QString::fromLatin1(
            QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex());
    }

    //! Verifies against the legacy format. Used only for the one-time
    //! migration path in loginUser(); never produces stored hashes.
    static bool verifyLegacySha256(const QString &password, const QString &stored)
    {
        if (isPbkdf2(stored) || stored.size() != HashBytes * 2)
            return false;
        return constantTimeEquals(stored.toLatin1(), legacySha256(password).toLatin1());
    }

    //! PBKDF2 (RFC 8018) with HMAC-SHA256 as the PRF.  Public so the unit
    //! tests can check it against the official RFC vectors.
    static QByteArray deriveKey(const QByteArray &password, const QByteArray &salt,
                                int iterations, int dkLen = HashBytes)
    {
        const int hLen = 32;
        QByteArray derived;
        derived.reserve(dkLen);
        quint32 block = 1;
        while (derived.size() < dkLen) {
            QByteArray message = salt;
            const quint32 be = qToBigEndian(block);
            message.append(reinterpret_cast<const char *>(&be), sizeof(be));

            QByteArray u = QMessageAuthenticationCode::hash(
                message, password, QCryptographicHash::Sha256);
            QByteArray t = u;
            for (int i = 1; i < iterations; ++i) {
                u = QMessageAuthenticationCode::hash(
                    u, password, QCryptographicHash::Sha256);
                for (int k = 0; k < hLen; ++k)
                    t[k] = static_cast<char>(t[k] ^ u[k]);
            }
            derived += t;
            ++block;
        }
        return derived.left(dkLen);
    }

private:
    static bool constantTimeEquals(const QByteArray &a, const QByteArray &b)
    {
        if (a.size() != b.size())
            return false;
        char diff = 0;
        for (int i = 0; i < a.size(); ++i)
            diff |= static_cast<char>(a[i] ^ b[i]);
        return diff == 0;
    }
};

#endif // PASSWORDHASHER_H
