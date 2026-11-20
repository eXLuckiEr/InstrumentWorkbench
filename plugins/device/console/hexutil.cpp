#include "hexutil.h"

#include <QCoreApplication>
#include <QRegularExpression>

namespace hexutil {

bool parse(const QString &text, QByteArray *out, QString *error)
{
    static const QRegularExpression sep(QStringLiteral("[\\s,;:\\-]+"));
    static const QRegularExpression hexOnly(QStringLiteral("^[0-9A-Fa-f]+$"));
    QByteArray result;
    const QStringList tokens = text.split(sep, Qt::SkipEmptyParts);
    for (QString tok : tokens) {
        if (tok.startsWith(QLatin1String("0x"), Qt::CaseInsensitive))
            tok.remove(0, 2);
        const bool valid = !tok.isEmpty() && hexOnly.match(tok).hasMatch()
                           && (tok.size() == 1 || tok.size() % 2 == 0);
        if (!valid) {
            if (error)
                *error = QCoreApplication::translate("hexutil", "Not a hex byte sequence: \"%1\"").arg(tok);
            return false;
        }
        if (tok.size() == 1)
            tok.prepend(QLatin1Char('0'));
        result += QByteArray::fromHex(tok.toLatin1());
    }
    if (result.isEmpty()) {
        if (error)
            *error = QCoreApplication::translate("hexutil", "Nothing to send");
        return false;
    }
    *out = result;
    return true;
}

QString formatRow(qint64 offset, const QByteArray &row)
{
    QString hex;
    QString ascii;
    for (int i = 0; i < kBytesPerRow; ++i) {
        if (i < row.size()) {
            const uchar c = uchar(row.at(i));
            hex += QStringLiteral("%1 ").arg(uint(c), 2, 16, QLatin1Char('0')).toUpper();
            ascii += (c >= 0x20 && c < 0x7f) ? QChar(c) : QLatin1Char('.');
        } else {
            hex += QStringLiteral("   ");
        }
        if (i == kBytesPerRow / 2 - 1)
            hex += QLatin1Char(' ');
    }
    return QStringLiteral("%1  %2 |%3|")
        .arg(offset, 4, 16, QLatin1Char('0')).toUpper().arg(hex, ascii);
}

} // namespace hexutil
