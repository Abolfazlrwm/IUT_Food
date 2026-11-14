//! Unit tests for the v2.5 AuditLog: one JSON object per line with a
//! stable field set, size-based rotation with bounded generations, and
//! a disabled mode that writes nothing.
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <QtTest>

#include "auditlog.h"

namespace {

QStringList readLines(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QTextStream stream(&file);
    QStringList lines;
    while (!stream.atEnd())
        lines << stream.readLine();
    return lines;
}

} // namespace

class AuditTests : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;

    QString trailPath() const
    {
        return m_dir.filePath(QStringLiteral("audit.jsonl"));
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
    }

    //! Every line parses as JSON and carries ts / kind / actor; the
    //! extra payload survives as a nested object and order is kept.
    void jsonlShapeAndOrdering()
    {
        const QString path = trailPath();
        AuditLog::instance().configure(path);

        AUDIT(QStringLiteral("login_ok"), QStringLiteral("ali"),
              QJsonObject({{QStringLiteral("ip"), QStringLiteral("127.0.0.1")}}));
        AUDIT(QStringLiteral("login_failed"), QStringLiteral("mallory"),
              QJsonObject());
        AUDIT(QStringLiteral("wallet_topup"), QStringLiteral("ali"),
              QJsonObject({{QStringLiteral("amount"), 25000.0},
               {QStringLiteral("ip"), QStringLiteral("::ffff:127.0.0.1")}}));

        const QStringList lines = readLines(path);
        QCOMPARE(lines.size(), 3);

        const QJsonObject first =
            QJsonDocument::fromJson(lines[0].toUtf8()).object();
        QCOMPARE(first.value(QLatin1String("kind")).toString(),
                 QStringLiteral("login_ok"));
        QCOMPARE(first.value(QLatin1String("actor")).toString(),
                 QStringLiteral("ali"));
        QCOMPARE(first.value(QLatin1String("extra")).toObject()
                     .value(QLatin1String("ip")).toString(),
                 QStringLiteral("127.0.0.1"));
        // Timestamp shape: "yyyy-MM-dd hh:mm:ss.zzz" - 23 characters.
        QCOMPARE(first.value(QLatin1String("ts")).toString().size(), 23);

        const QJsonObject second =
            QJsonDocument::fromJson(lines[1].toUtf8()).object();
        QCOMPARE(second.value(QLatin1String("kind")).toString(),
                 QStringLiteral("login_failed"));
        QVERIFY(!second.contains(QLatin1String("extra"))); // omitted when empty

        const QJsonObject third =
            QJsonDocument::fromJson(lines[2].toUtf8()).object();
        QCOMPARE(third.value(QLatin1String("extra")).toObject()
                     .value(QLatin1String("amount")).toDouble(),
                 25000.0);
    }

    //! Rotation keeps the newest event in the active file and bounds
    //! the number of generation files.
    void sizeRotation()
    {
        const QString path = m_dir.filePath(QStringLiteral("rotate.jsonl"));
        AuditLog::instance().configure(path, /*rotateBytes=*/400,
                                       /*keepFiles=*/2);

        for (int i = 0; i < 12; ++i)
            AUDIT(QStringLiteral("order_placed"), QStringLiteral("cust%1").arg(i),
                  QJsonObject({{QStringLiteral("total"), 120000.0 + i},
                   {QStringLiteral("ip"), QStringLiteral("10.0.0.9")}}));

        // A final event the fresh active file can hold without
        // rotating again, so the active file is not empty.
        AUDIT(QStringLiteral("order_placed"), QStringLiteral("cust_final"),
              QJsonObject({{QStringLiteral("total"), 999999.0}}));

        QVERIFY(QFile::exists(path));
        QVERIFY(QFile::exists(path + ".1"));
        QVERIFY(QFile::exists(path + ".2"));
        QVERIFY(!QFile::exists(path + ".3"));

        // The active file holds the most recent events; every line in
        // every generation still parses as JSON.
        const QStringList active = readLines(path);
        QVERIFY(!active.isEmpty());
        QVERIFY(active.last().contains(QStringLiteral("cust_final")));
        for (const QString &file : {path, path + ".1", path + ".2"}) {
            for (const QString &line : readLines(file)) {
                const QJsonObject obj =
                    QJsonDocument::fromJson(line.toUtf8()).object();
                QVERIFY(!obj.isEmpty());
                QVERIFY(obj.contains(QLatin1String("kind")));
            }
        }
    }

    //! Reconfiguring to an empty path disables the trail: nothing is
    //! written and previously configured files are closed cleanly.
    void disabledModeWritesNothing()
    {
        const QString missing = m_dir.filePath(QStringLiteral("never.jsonl"));
        AuditLog::instance().configure(QString());
        QVERIFY(!AuditLog::instance().writesToFile());
        AUDIT(QStringLiteral("login_ok"), QStringLiteral("nobody"), QJsonObject());
        QVERIFY(!QFile::exists(missing));

        // Back to enabled: events flow again, appended to the file.
        AuditLog::instance().configure(missing);
        QVERIFY(AuditLog::instance().writesToFile());
        AUDIT(QStringLiteral("logout"), QStringLiteral("nobody"), QJsonObject());
        QCOMPARE(readLines(missing).size(), 1);
    }
};

QTEST_MAIN(AuditTests)
#include "audit_tests.moc"
