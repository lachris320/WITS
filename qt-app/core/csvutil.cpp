#include "csvutil.h"

namespace csvutil {

QString escapeField(const QString &field)
{
    const bool needsQuote = field.contains(QLatin1Char(','))
                         || field.contains(QLatin1Char('"'))
                         || field.contains(QLatin1Char('\n'))
                         || field.contains(QLatin1Char('\r'));
    if (!needsQuote)
        return field;
    QString q = field;
    q.replace(QLatin1Char('"'), QLatin1String("\"\""));
    return QLatin1Char('"') + q + QLatin1Char('"');
}

QString joinRow(const QStringList &cells)
{
    QStringList out;
    out.reserve(cells.size());
    for (const QString &c : cells)
        out << escapeField(c);
    return out.join(QLatin1Char(',')) + QLatin1String("\r\n");
}

} // namespace csvutil
