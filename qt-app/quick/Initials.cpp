#include "Initials.h"

#include <QChar>
#include <QStringList>

namespace Initials {

QString of(const QString &name)
{
    QString out;
    const QStringList parts = name.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString &p : parts) {
        out.append(p.at(0).toUpper());
        if (out.size() == 2)
            break;
    }
    return out;
}

} // namespace Initials
