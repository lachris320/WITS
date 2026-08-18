#ifndef REPORTRENDERER_H
#define REPORTRENDERER_H

#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QSize>
#include <QString>

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

    static QImage makeBarChartImage(const QJsonArray &data, QSize size,
                                    const ReportPalette &palette);
    static QImage makePieChartImage(const QJsonArray &data, QSize size,
                                    const ReportPalette &palette);
    static QImage makeLineChartImage(const QJsonArray &data, QSize size,
                                     const ReportPalette &palette,
                                     int openHour, int closeHour);

    static bool paintReport(QPagedPaintDevice *device, int resolution,
                            const QJsonArray &data, const QJsonObject &filters,
                            const ReportPalette &palette,
                            const ReportHeaderInfo &info);

    static bool writeReportToXlsx(QXlsx::Document &xlsx,
                                  const QJsonArray &rows,
                                  const QJsonObject &filters,
                                  const ReportHeaderInfo &info);

private:
    // Renders a configured chart into an ARGB32 image of `size` (shared tail of the chart makers).
    static QImage renderChartToImage(QChart *chart, QSize size);
};

#endif // REPORTRENDERER_H
