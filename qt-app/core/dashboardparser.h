#ifndef DASHBOARDPARSER_H
#define DASHBOARDPARSER_H

#include <QByteArray>
#include <QString>
#include "dashboarddata.h"

// Pure, network-free decoder for dashboard_summary.php (spec §5.1). Returns
// true and fills `out` on a status=="success" object; otherwise returns false
// and sets `outError` to a user-safe message (never a backend internal).
namespace DashboardParser {
bool parse(const QByteArray &raw, DashboardSummary &out, QString &outError);
}

#endif // DASHBOARDPARSER_H
