#ifndef RFIDCODEINSPECT_H
#define RFIDCODEINSPECT_H

#include <QString>

// Pure, header-only diagnostic for an RFID scan value. Renders the raw string,
// its length, and a space-separated Latin-1 hex dump so leading zeros and
// otherwise-invisible characters (stray spaces, a trailing CR, encoding quirks)
// are directly comparable against the stored students.code. Header-only and
// dependency-free (no Widgets/network) so it links into both the app and the
// unit-test target. See apiconfig.h for the same header-only pattern.
//
// NOTE: the hex dump is Latin-1 — any code point > 0xFF renders as 3f ('?').
// Harmless for ASCII-digit keyboard-wedge output (the realistic case); if a
// reader ever emits non-ASCII, value= shows the real char while hex= shows 3f,
// which is itself a useful signal.
namespace RfidCodeInspect {

inline QString describe(const QString &code)
{
    const QString hex = QString::fromLatin1(code.toLatin1().toHex(' '));
    return QStringLiteral("value=\"%1\" len=%2 hex=%3")
        .arg(code)
        .arg(code.length())
        .arg(hex);
}

} // namespace RfidCodeInspect

#endif // RFIDCODEINSPECT_H
