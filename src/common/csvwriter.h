#ifndef CSVWRITER_H
#define CSVWRITER_H

#include <QList>
#include <QString>
#include <QStringList>

/*!
 * v2.7: minimal, correct CSV rendering for the admin console's export
 * buttons (reports and the order browser).
 *
 * - RFC 4180 quoting: a field is wrapped in double quotes as soon as it
 *   contains a comma, a quote, or a newline; embedded quotes double up.
 *   Persian text passes through unchanged (UTF-8 everywhere).
 * - Rows end with CRLF per the RFC; Excel, LibreOffice and every CSV
 *   parser in a data course accept it.
 * - With bomForExcel a UTF-8 BOM (EF BB BF) is prepended: without it,
 *   Excel on Windows decodes UTF-8 Persian headers as Latin-1 mojibake.
 *   ("UTF-8 CSV opens garbled in Excel" is the classic bug this kills.)
 *
 * Reading is deliberately NOT implemented - the admin exports data, it
 * never imports it, and a round-trip parser is a whole other test suite.
 */
class CsvWriter
{
public:
    //! Quoting per RFC 4180.  Fields without special characters are
    //! left bare (matching how Excel re-saves such files).
    static QString escapeField(const QString &raw)
    {
        const bool needsQuoting = raw.contains(QLatin1Char(',')) ||
                                  raw.contains(QLatin1Char('"')) ||
                                  raw.contains(QLatin1Char('\n')) ||
                                  raw.contains(QLatin1Char('\r'));
        if (!needsQuoting)
            return raw;
        QString escaped = raw;
        escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        return QLatin1Char('"') + escaped + QLatin1Char('"');
    }

    //! Renders header + rows as one CSV string.  Empty tables still
    //! emit the header line, so the file is never zero bytes of nothing.
    static QString tableToCsv(const QStringList &headers,
                              const QList<QStringList> &rows,
                              bool bomForExcel = true)
    {
        QList<QStringList> all;
        all.reserve(rows.size() + 1);
        all << headers;
        for (const QStringList &row : rows)
            all << row;

        QStringList lines;
        lines.reserve(all.size());
        for (const QStringList &row : std::as_const(all)) {
            QStringList fields;
            fields.reserve(row.size());
            for (const QString &field : row)
                fields << escapeField(field);
            lines << fields.join(QLatin1Char(','));
        }
        QString csv = lines.join(QStringLiteral("\r\n")) + QStringLiteral("\r\n");
        if (bomForExcel)
            // U+FEFF as an explicit character.  NOT QString::fromUtf8("\xEF\xBB\xBF"):
            // Qt 6's UTF-8 decoder treats a leading BOM as encoding metadata and
            // drops it, silently returning an EMPTY string (and an empty needle
            // makes startsWith() vacuously true in tests).  QString(QChar(0xFEFF))
            // survives, and toUtf8() re-encodes it to the 3-byte EF BB BF mark.
            csv.prepend(QString(QChar(0xFEFF)));
        return csv;
    }
};

#endif // CSVWRITER_H
