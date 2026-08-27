#ifndef REPORTDATA_H
#define REPORTDATA_H

#include <QColor>
#include <QList>
#include <QString>
#include <QStringList>
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

// Presentation state of the exported "When?" block. Exactly FOUR states.
enum class TimeAnalyticsExportState {
    Disabled,  // caller opts out entirely (legacy WITS.exe) — renderer omits the section
    Data,      // time fetch succeeded and hasData — render charts (PDF) / table (Excel)
    Empty,     // time fetch succeeded but all-zero range — render the "no activity" note
    Error      // time fetch failed — render the "could not be loaded" note (DISTINCT from Empty)
};

// Presentation-ready carrier for the exported time analytics. Assembled by the
// ViewModel (the single owner of hour/weekday formatting); consumed verbatim by
// ReportRenderer, which does NO hour/weekday math of its own. Default-constructed
// value is state == Disabled with empty lists — the legacy WITS.exe payload.
struct ReportTimeExport {
    TimeAnalyticsExportState state = TimeAnalyticsExportState::Disabled;

    QStringList hourLabels;      // 24 entries, VM-formatted (e.g. "12A","1A", … "11P")
    QList<int>  hourCounts;      // 24 entries, index = hour 0..23
    QStringList weekdayLabels;   // 7 entries, Mon→Sun, VM-formatted short names ("Mon".."Sun")
    QList<int>  weekdayCounts;   // 7 entries, index 0=Mon .. 6=Sun

    QString     busiestHourLabel;  // VM peak VALUE, e.g. "2–3 PM"  (empty unless state==Data)
    QString     busiestDayLabel;   // VM peak VALUE, e.g. "Wednesday" (empty unless state==Data)
};

#endif // REPORTDATA_H
