//! Unit tests for the v2.7 CSV export used by the admin console:
//! RFC 4180 quoting, CRLF rows and the UTF-8 BOM that keeps Persian
//! headers readable in Excel on Windows.  Pure string tests - no
//! dialogs, no network.
#include <QStringList>
#include <QtTest>

#include "csvwriter.h"

class CsvTests : public QObject
{
    Q_OBJECT

private slots:
    //! Fields without special characters must stay bare (exactly what
    //! Excel re-saves), so the output stays diff-friendly.
    void plainFieldsStayBare()
    {
        QCOMPARE(CsvWriter::escapeField(QStringLiteral("سلام")),
                 QStringLiteral("سلام"));
        QCOMPARE(CsvWriter::escapeField(QStringLiteral("pizza_margherita")),
                 QStringLiteral("pizza_margherita"));
        QCOMPARE(CsvWriter::escapeField(QString()), QString());
        QCOMPARE(CsvWriter::escapeField(QStringLiteral("1234.50")),
                 QStringLiteral("1234.50"));
    }

    //! Comma, quote and newline each force quoting; embedded quotes
    //! double up per RFC 4180.  This is the bug class that silently
    //! corrupts row boundaries in naive writers.
    void quotingRules()
    {
        QCOMPARE(CsvWriter::escapeField(QStringLiteral("a,b")),
                 QStringLiteral("\"a,b\""));
        QCOMPARE(CsvWriter::escapeField(QStringLiteral("he said \"hi\"")),
                 QStringLiteral("\"he said \"\"hi\"\"\""));
        QCOMPARE(CsvWriter::escapeField(QStringLiteral("line1\nline2")),
                 QStringLiteral("\"line1\nline2\""));
        QCOMPARE(CsvWriter::escapeField(QStringLiteral("cr\rin")),
                 QStringLiteral("\"cr\rin\""));
        // Quote-only quoting: a field that merely contains a quote still
        // must be wrapped, not just have its quote doubled.
        QCOMPARE(CsvWriter::escapeField(QStringLiteral("x\"y")),
                 QStringLiteral("\"x\"\"y\""));
    }

    //! The table rendering: BOM first, CRLF between rows and a trailing
    //! CRLF so `wc -l` counts every line on Unix tools too.
    void bomAndCrlfShape()
    {
        const QString csv = CsvWriter::tableToCsv(
            {QStringLiteral("رستوران"), QStringLiteral("درآمد (تومان)")},
            {{QStringLiteral("دانش‌پز"), QStringLiteral("1250000")},
             {QStringLiteral("فست فود \"قهرمان\""), QStringLiteral("9,700")}});

        QVERIFY(csv.startsWith(QString(QChar(0xFEFF)))); // the real UTF-8 BOM
        QVERIFY(csv.endsWith(QStringLiteral("\r\n")));
        QCOMPARE(csv.count(QStringLiteral("\r\n")), 3); // 2 data rows + header
        QVERIFY(csv.contains(QStringLiteral("\"فست فود \"\"قهرمان\"\"\",\"9,700\"")));
    }

    //! Persian text (RTL, ZWNJ) must pass through byte-identical - the
    //! export is a report attachment, not a re-encoding machine.
    void persianTextSurvives()
    {
        const QStringList header = {QStringLiteral("وضعیت"), QStringLiteral("تعداد سفارش")};
        const QString csv = CsvWriter::tableToCsv(
            header, {{QStringLiteral("در حال آماده‌سازی"), QStringLiteral("3")}},
            false);
        QVERIFY(csv.contains(QStringLiteral("در حال آماده‌سازی")));
        QVERIFY(csv.contains(QStringLiteral("تعداد سفارش")));
    }

    //! bomForExcel=false drops the BOM (for tools that would echo it as
    //! text) while everything else stays identical.
    void noBomVariant()
    {
        const QStringList header = {QStringLiteral("a")};
        const QString withBom =
            CsvWriter::tableToCsv(header, {}, true);
        const QString withoutBom =
            CsvWriter::tableToCsv(header, {}, false);
        // NOTE: the needle must be U+FEFF as a character.  A needle built
        // by QString::fromUtf8("\xEF\xBB\xBF") comes back EMPTY (Qt 6's
        // decoder drops the BOM), and an empty prefix matches everything.
        const QString bom = QString(QChar(0xFEFF));
        QVERIFY(withBom.startsWith(bom));
        QVERIFY(!withoutBom.startsWith(bom));
        QCOMPARE(withoutBom, QStringLiteral("a\r\n"));
        // An empty table still emits the header line: never a 0-byte file.
        // ("a\r\n" is 3 characters: 'a', CR, LF.)
        QCOMPARE(withoutBom.size(), 3);
    }
};

#include "csv_tests.moc"

QTEST_MAIN(CsvTests)
