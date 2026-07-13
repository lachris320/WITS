#ifndef VISITLOGPARSER_H
#define VISITLOGPARSER_H

#include <QByteArray>
#include <QList>
#include <QString>
#include "visitlogdata.h"

// Pure, network-free decoder for get_library_visits.php (spec §5.2). Returns
// true + fills out/outCount on status=="success"; else false + user-safe error.
namespace VisitLogParser {
bool parse(const QByteArray &raw, QList<StudentVisitRecord> &out,
           int &outCount, QString &outError);
}

#endif // VISITLOGPARSER_H
