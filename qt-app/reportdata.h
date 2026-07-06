#ifndef REPORTDATA_H
#define REPORTDATA_H

#include <QColor>
#include <QString>
#include <QVector>

// Moved verbatim out of adminwindow.h (was adminwindow.h:54-62).
struct ReportPalette {
    QColor headerBg;
    QColor headerText;
    QColor rowEvenBg;
    QColor rowOddBg;
    QColor rowText;
    QVector<QColor> chartColors;
};

// Environment the View reads from QSettings and passes into ReportRenderer,
// so the renderer stays stateless (no QSettings, no ui->). Defaults reproduce
// the QSettings defaults at paintReport (school/admin) and makeLineChartImage
// (library hours).
struct ReportHeaderInfo {
    QString schoolName;        // "school/name"      (default "Your School Name")
    QString address;           // "school/address"   (default "Your Address")
    QString logoPath;          // "school/logoPath"  (default "")
    QString librarian;         // "admin/name"       (default "")
    QString position;          // "admin/position"   (default "")
    int     openHour  = 7;     // "library/openHour"  (default 7)
    int     closeHour = 21;    // "library/closeHour" (default 21)
};

// Result of decoding a get_report_data.php POST response.
enum class ReportDataOutcome {
    Success,          // status == "success"; data array in outData
    NotSuccess,       // status != "success"; message in outMessage
    InvalidResponse   // body is not a JSON object
    // NetworkError is decided by the caller from reply->error(), not here.
};

// Pure output of the duration -> date-range computation.
struct DateRange {
    QString start;   // "yyyy-MM-dd"
    QString end;     // "yyyy-MM-dd"
    bool    valid = false;
};

#endif // REPORTDATA_H
