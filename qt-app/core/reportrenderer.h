#ifndef REPORTRENDERER_H
#define REPORTRENDERER_H

#include <QImage>
#include <QPixmap>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QSize>
#include <QString>

#include "reportanalytics.h"   // ReportAnalytics — passed in; renderer never re-aggregates
#include "reportdata.h"
#include "xlsxdocument.h"   // QXlsx::Document

class QPagedPaintDevice;
class QChart;

// Stateless renderer. No QSettings, no ui->, no member state — every method is a
// pure function of its arguments. Charts/PDF/Excel bodies are verbatim ports from
// adminwindow.cpp with the QSettings reads replaced by ReportHeaderInfo params and
// getPalette(...) resolved by the caller into `palette`.
class ReportRenderer
{
public:
    // Scales a legacy ~96-DPI pixel literal to the paged device's `resolution`,
    // so a layout tuned at 96 DPI keeps the same physical proportions at any DPI
    // (QPdfWriter defaults to 1200). Fonts are point-sized and scale on their
    // own; this is for the raw device-pixel advances/rects that do not.
    static int scaledPx(double basePx, int resolution);

    // Pure aggregation helpers (factored out of the chart makers).
    static QMap<QString, int>           aggregateVisitsByCourse(const QJsonArray &data);
    static QMap<QString, QMap<int, int>> aggregateVisitsByCourseHour(const QJsonArray &data,
                                                                     int openHour, int closeHour);

    // Screen-safe chart raster size (see the .cpp): a modest base — square=false →
    // ~16:10 landscape (bar/line), square=true → square (pie) — shrunk to fit the
    // available screen so the QChartView (a QWidget) is never clamped; paintReport
    // upscales it to fill the page. usableWidth is unused (kept for call-site clarity).
    static QSize chartImageSize(int usableWidth, bool square);

    // Scales `src` to fully cover a diameter×diameter box (KeepAspectRatioByExpanding),
    // center-crops the overflow, and clips to an inscribed circle — the report-header
    // logo look matching the on-screen LLogoCircle. Returns a diameter×diameter pixmap
    // with transparent corners; a null/empty src yields a fully transparent box, and
    // diameter<=0 yields a null (draw-nothing) pixmap.
    static QPixmap circularLogoPixmap(const QPixmap &src, int diameter);

    static QImage makeBarChartImage(const QJsonArray &data, QSize size,
                                    const ReportPalette &palette);
    static QImage makePieChartImage(const QJsonArray &data, QSize size,
                                    const ReportPalette &palette);
    static QImage makeLineChartImage(const QJsonArray &data, QSize size,
                                     const ReportPalette &palette,
                                     int openHour, int closeHour);

    // "When?" bar-chart makers (spec 4b-iv-b §8.2). Structurally identical to
    // makeBarChartImage; sized from chartImageSize(usableWidth,false) and
    // upscaled by drawFullscreenChart (NEVER an arbitrary/print size — the
    // QChartView screen-clamp hazard, §8.3). Both show one unique label per bar
    // (hourly: all 24 "12A".."11P"; weekday: all 7 Mon→Sun) — duplicate empty
    // categories collapse QBarCategoryAxis's plot range and blank the bars, so
    // hourly no longer thins to every 3rd label. Peak caption rides in the title.
    static QImage makeHourlyBarChartImage(const ReportTimeExport &t, QSize size,
                                          const ReportPalette &palette);
    static QImage makeWeekdayBarChartImage(const ReportTimeExport &t, QSize size,
                                           const ReportPalette &palette);

    static bool paintReport(QPagedPaintDevice *device, int resolution,
                            const QJsonArray &data, const QJsonObject &filters,
                            const ReportPalette &palette,
                            const ReportHeaderInfo &info,
                            const ReportAnalytics &analytics, bool includeRoster,
                            const ReportTimeExport &timeExport);

    static bool writeReportToXlsx(QXlsx::Document &xlsx,
                                  const QJsonArray &rows,
                                  const QJsonObject &filters,
                                  const ReportHeaderInfo &info,
                                  const ReportAnalytics &analytics, bool includeRoster,
                                  const ReportTimeExport &timeExport);

private:
    // Renders a configured chart into an ARGB32 image of `size` (shared tail of the chart makers).
    static QImage renderChartToImage(QChart *chart, QSize size);
};

#endif // REPORTRENDERER_H
