#pragma once
#include <QByteArray>
#include <QString>

// Pure helpers of the console's hex mode (no widgets, easy to test).
namespace hexutil {

// "48 65 6C 6C 6F", "0x48,0x65", "48:65:6c", "A B C" (a single digit is one byte) -> bytes.
// Returns false and fills `error` (translated) on bad input or when nothing was given.
bool parse(const QString &text, QByteArray *out, QString *error);

// One dump row: "0010  48 65 6C 6C 6F 20 77 6F  72 6C 64 0A              |Hello world.|".
// `row` holds up to 16 bytes; shorter rows are padded so the ASCII column stays aligned.
QString formatRow(qint64 offset, const QByteArray &row);

constexpr int kBytesPerRow = 16;

} // namespace hexutil
