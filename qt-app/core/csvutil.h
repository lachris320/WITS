#ifndef CSVUTIL_H
#define CSVUTIL_H

#include <QString>
#include <QStringList>

// Shared RFC-4180 CSV kernel. Pure formatting only — no domain-specific
// policy (e.g. CSV-formula-injection neutralization) belongs here; that
// stays at whichever call site needs it (see StudentController::toCsv).
namespace csvutil {

// Quotes `field` iff it contains a comma, double-quote, CR, or LF; any inner
// double-quote is doubled first. A field needing no quoting is returned
// unchanged.
QString escapeField(const QString &field);

// escapeField()s each cell, joins them with ',', and appends a CRLF
// ("\r\n") line terminator.
QString joinRow(const QStringList &cells);

} // namespace csvutil

#endif // CSVUTIL_H
