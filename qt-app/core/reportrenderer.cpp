#include "reportrenderer.h"

#include <QChart>
#include <QChartView>
#include <QBarSet>
#include <QBarSeries>
#include <QBarCategoryAxis>
#include <QPieSeries>
#include <QPieSlice>
#include <QLineSeries>
#include <QValueAxis>

#include <QDate>
#include <QDebug>
#include <QFont>
#include <QFontMetrics>
#include <QGraphicsLayout>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QScreen>
#include <QMarginsF>
#include <QPagedPaintDevice>
#include <QPainter>
#include <QPixmap>
#include <QRect>
#include <QScopeGuard>
#include <QTime>

#include "xlsxformat.h"     // QXlsx::Format  (match adminwindow.cpp's include)
#include "xlsxcellrange.h"  // QXlsx::CellRange (mergeCells)

// Diagnostic logging for the report renderer. Silent by default (QtInfoMsg
// floor suppresses qCDebug); re-enable when troubleshooting with
//   QT_LOGGING_RULES="wits.report.render.debug=true"
Q_LOGGING_CATEGORY(lcReportRender, "wits.report.render", QtInfoMsg)

namespace {
// Guards writeReportToXlsx's Excel cells against formula/CSV injection. The
// vendored QXlsx::Worksheet::write() treats any string starting with '='
// as a live FORMULA (and Excel itself treats a leading + - @ or a leading
// tab/CR the same way once opened), so network-derived text (a student
// name, course, or department pulled from the backend) could smuggle a
// formula (e.g. =HYPERLINK(...) or a DDE payload) that the librarian's
// Excel evaluates on open. Prefixing a single apostrophe is the standard
// Excel text-escape: it forces the cell to render as literal text.
QString sanitizeXlsxText(const QString &s) {
    if (s.isEmpty())
        return s;
    const QChar lead = s.at(0);
    if (lead == QLatin1Char('=') || lead == QLatin1Char('+') || lead == QLatin1Char('-')
        || lead == QLatin1Char('@') || lead == QLatin1Char('\t') || lead == QLatin1Char('\r')) {
        return QLatin1Char('\'') + s;
    }
    return s;
}

// Shared "When?" export wording (spec 4b-iv-b §8.4/§10) — the section title,
// the Empty/Error note, and the peak captions are each written from three
// separate sites (PDF path, Excel path, chart makers); centralizing the
// literals here keeps those sites byte-identical without copy-paste drift.
QString whenSectionTitle() { return QStringLiteral("When do students visit?"); }

QString timeExportNoteText(TimeAnalyticsExportState state) {
    return state == TimeAnalyticsExportState::Error
               ? QStringLiteral("Visit-time data could not be loaded")
               : QStringLiteral("No visit activity in this range");
}

QString peakHourCaption(const ReportTimeExport &t) { return QStringLiteral("Peak Hour: %1").arg(t.busiestHourLabel); }
QString peakDayCaption(const ReportTimeExport &t)  { return QStringLiteral("Busiest Day: %1").arg(t.busiestDayLabel); }
} // namespace

// Scales a legacy ~96-DPI pixel literal to the paged device's resolution.
// paintReport's vertical advances/rects were raw device-pixel literals tuned
// for ~96 DPI; on a QPdfWriter (1200 DPI default) they no longer clear the
// point-sized glyph boxes, so rows overlapped. Scaling by resolution/96 keeps
// the original 96-DPI proportions at any DPI.
int ReportRenderer::scaledPx(double basePx, int resolution) {
    return qRound(basePx * resolution / 96.0);
}

// Factored out of make{Bar,Pie}ChartImage's aggregation loop (legacy cpp:125-131 / 193-199).
QMap<QString, int> ReportRenderer::aggregateVisitsByCourse(const QJsonArray &data) {
    QMap<QString, int> courseCounts;
    for (const QJsonValue &v : data) {
        const QJsonObject obj = v.toObject();
        courseCounts[obj["course"].toString()] += obj["visits"].toInt();
    }
    return courseCounts;
}

// Factored out of makeLineChartImage's aggregation loop (legacy cpp:253-275), with the
// QSettings library-hours reads replaced by openHour/closeHour params. Skips
// invalid login_time and hours outside [openHour, closeHour], exactly as legacy.
QMap<QString, QMap<int, int>> ReportRenderer::aggregateVisitsByCourseHour(
        const QJsonArray &data, int openHour, int closeHour) {
    QMap<QString, QMap<int, int>> courseTimeCounts;
    for (const QJsonValue &v : data) {
        const QJsonObject obj = v.toObject();
        const QString course = obj["course"].toString();
        const QString loginTime = obj["login_time"].toString();
        const QTime time = QTime::fromString(loginTime, "HH:mm:ss");
        if (!time.isValid()) {
            continue;
        }
        const int hour = time.hour();
        if (hour < openHour || hour > closeHour)
            continue;
        courseTimeCounts[course][hour] += 1;
    }
    return courseTimeCounts;
}

// The chart raster size the QChartView is rendered at. A QChartView is a QWidget
// the window system clamps to the physical screen, so rendering at print
// resolution (~9000px) made the chart lay out in a corner with giant fonts.
// Render at a modest size that FITS THE ACTUAL SCREEN (so the widget is never
// clamped), and let paintReport scale the raster up to fill the page. square →
// 1:1 (pie); otherwise ~16:10 landscape (bar/line). Chart fonts (see the makers)
// are sized to this height, so they read correctly once scaled onto the page.
QSize ReportRenderer::chartImageSize(int usableWidth, bool square) {
    Q_UNUSED(usableWidth);
    QSize base = square ? QSize(1000, 1000) : QSize(1600, 1000);
    // Shrink (keeping aspect) to fit the available screen, so a narrow display
    // (e.g. a 1366-wide laptop or a kiosk) can't clamp the QChartView.
    if (const QScreen *scr = QGuiApplication::primaryScreen()) {
        const QSize avail = scr->availableSize() * 0.85;
        if (avail.width() >= 320 && avail.height() >= 240
            && (base.width() > avail.width() || base.height() > avail.height())) {
            base.scale(avail, Qt::KeepAspectRatio);
        }
    }
    return base;
}

// Shared render tail of the three chart makers: paint a configured QChart into
// an ARGB32 QImage of the requested size, via a local QChartView that owns `chart`
// and deletes it when this returns.
//
// IMPORTANT: `size` must stay MODEST (screen-sized, not print-sized). A QChartView
// is a QWidget and the window system clamps a widget to the physical screen, so
// rendering it directly at print resolution (~9000px) made the chart lay out at
// ~screen size in the top-left with huge fonts and a blank remainder. chartImageSize
// therefore returns a screen-safe size and paintReport scales the raster up to fill
// the page. (Rendering via a QGraphicsScene instead avoids the clamp but does not
// lay the bars out — it produced a solid block — so QChartView is the right tool.)
QImage ReportRenderer::renderChartToImage(QChart *chart, QSize size) {
    QImage chartImage(size, QImage::Format_ARGB32);
    chartImage.fill(Qt::white);
    QPainter painter(&chartImage);
    QChartView view(chart);
    view.setRenderHint(QPainter::Antialiasing);
    view.resize(size);
    view.show();
    view.chart()->resize(size);
    view.render(&painter);
    return chartImage;
}

// --- Bar Chart ---
// Verbatim port of adminWindow::makeBarChartImage (legacy adminwindow.cpp:123-187),
// with the inline aggregation loop replaced by aggregateVisitsByCourse(data).
QImage ReportRenderer::makeBarChartImage(const QJsonArray &data, QSize size, const ReportPalette &palette) {
    // Aggregate visits by course
    QMap<QString, int> courseCounts = aggregateVisitsByCourse(data);

    // Fonts are sized to the image height (not fixed points) so labels stay
    // legible after this large raster is scaled down onto the page.
    const int h = size.height();
    QFont titleFont("Arial");  titleFont.setPixelSize(qMax(10, qRound(h * 0.032)));  titleFont.setBold(true);
    QFont labelFont("Arial");  labelFont.setPixelSize(qMax(8,  qRound(h * 0.024)));

    // One bar per course, spread across the category axis: a SINGLE QBarSet
    // holding every course's value (one category each). The legacy build used a
    // separate one-value barset per course, so all bars piled into category 0 and
    // the rest of the chart width sat empty. One set + N categories fills the width.
    QBarSet *set = new QBarSet("Visits");
    QStringList categories;
    for (auto it = courseCounts.begin(); it != courseCounts.end(); ++it) {
        *set << it.value();
        categories << it.key();
    }
    set->setBrush(palette.chartColors.isEmpty() ? QBrush(palette.headerBg)
                                                : QBrush(palette.chartColors.first()));

    QBarSeries *series = new QBarSeries();
    series->append(set);

    // Create chart
    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("Library Visits by Course");
    chart->setTitleFont(titleFont);

    QBarCategoryAxis *axisX = new QBarCategoryAxis();
    axisX->append(categories);
    axisX->setLabelsFont(labelFont);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("Number of Visits");
    axisY->setLabelsFont(labelFont);
    axisY->setTitleFont(labelFont);
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    // Single series → the x-axis course labels identify the bars; a legend would
    // just repeat "Visits", so hide it and give the chart the full width.
    chart->legend()->setVisible(false);

    // ✅ Remove margins and force chart to fill
    chart->setMargins(QMargins(0, 0, 0, 0));
    chart->layout()->setContentsMargins(0, 0, 0, 0);
    chart->setBackgroundRoundness(0);

    return renderChartToImage(chart, size);
}


// --- Pie Chart ---
// Verbatim port of adminWindow::makePieChartImage (legacy adminwindow.cpp:191-241),
// with the inline aggregation loop replaced by aggregateVisitsByCourse(data).
QImage ReportRenderer::makePieChartImage(const QJsonArray &data, QSize size, const ReportPalette &palette) {
    // Aggregate visits by course
    QMap<QString, int> courseCounts = aggregateVisitsByCourse(data);

    // Fonts are sized to the image height (not fixed points) so labels stay
    // legible after this large raster is scaled down onto the page.
    const int h = size.height();
    QFont titleFont("Arial");  titleFont.setPixelSize(qMax(10, qRound(h * 0.032)));  titleFont.setBold(true);
    QFont labelFont("Arial");  labelFont.setPixelSize(qMax(8,  qRound(h * 0.024)));

    // Create pie series
    QPieSeries *series = new QPieSeries();
    QVector<QColor> colors = { palette.rowEvenBg, palette.rowOddBg, palette.headerBg };
    int colorIndex = 0;
    for (auto it = courseCounts.begin(); it != courseCounts.end(); ++it) {
        QPieSlice *slice = series->append(it.key(), it.value());
        slice->setLabel(QString("%1: %2").arg(it.key()).arg(it.value()));
        slice->setLabelVisible(true);
        slice->setBrush(palette.chartColors[colorIndex % palette.chartColors.size()]);
        slice->setLabelFont(labelFont);
        colorIndex++;
    }

    // Create chart
    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("Library Visits by Course");
    chart->setTitleFont(titleFont);
    chart->legend()->setVisible(true);
    chart->legend()->setFont(labelFont);
    chart->legend()->setAlignment(Qt::AlignRight);

    // ✅ Remove extra margins so chart fills the image
    chart->setMargins(QMargins(0, 0, 0, 0));
    chart->layout()->setContentsMargins(0, 0, 0, 0);
    chart->setBackgroundRoundness(0);

    return renderChartToImage(chart, size);
}


// --- Line Chart ---
// Verbatim port of adminWindow::makeLineChartImage (legacy adminwindow.cpp:246-342), with:
//  - the QSettings library-hours reads replaced by the openHour/closeHour params
//  - the aggregation + globalMax loop replaced by aggregateVisitsByCourseHour(...) plus
//    an equivalent globalMax scan over its result (same [openHour,closeHour] filter,
//    same invalid-time skip, same "count rows" semantics — not the "visits" field).
QImage ReportRenderer::makeLineChartImage(const QJsonArray &data, QSize size, const ReportPalette &palette,
                                          int openHour, int closeHour) {
    // Aggregate visits per course per hour
    QMap<QString, QMap<int, int>> courseTimeCounts =
        aggregateVisitsByCourseHour(data, openHour, closeHour);

    // Fonts are sized to the image height (not fixed points) so labels stay
    // legible after this large raster is scaled down onto the page.
    const int h = size.height();
    QFont titleFont("Arial");  titleFont.setPixelSize(qMax(10, qRound(h * 0.032)));  titleFont.setBold(true);
    QFont labelFont("Arial");  labelFont.setPixelSize(qMax(8,  qRound(h * 0.024)));

    int globalMax = 0;
    for (auto it = courseTimeCounts.begin(); it != courseTimeCounts.end(); ++it)
        for (int count : it.value())
            if (count > globalMax)
                globalMax = count;

    QChart *chart = new QChart();
    chart->setTitle("Library Peak Hours by Course");
    chart->setTitleFont(titleFont);

    // One line series per course
    QVector<QColor> colors = { palette.rowEvenBg, palette.rowOddBg, palette.headerBg };
    int colorIndex = 0;
    for (auto it = courseTimeCounts.begin(); it != courseTimeCounts.end(); ++it) {
        QLineSeries *series = new QLineSeries();
        series->setName(it.key());

        // ✅ Only loop within open–close hours
        for (int h = openHour; h <= closeHour; ++h) {
            int count = it.value().value(h, 0);
            series->append(h, count);
        }
        series->setColor(palette.chartColors[colorIndex % palette.chartColors.size()]);
        chart->addSeries(series);
        colorIndex++;
    }

    // X axis = only library hours
    QValueAxis *axisX = new QValueAxis();
    axisX->setTitleText("Hour of Day");
    axisX->setRange(openHour, closeHour);  // ✅ restrict to library hours
    axisX->setTickCount(closeHour - openHour + 1);
    axisX->setLabelFormat("%d:00");   // shows "7:00", "8:00", etc.
    axisX->setLabelsFont(labelFont);
    axisX->setTitleFont(labelFont);
    chart->addAxis(axisX, Qt::AlignBottom);

    // Y axis = number of students (auto-scale)
    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("Number of Students");
    axisY->setLabelsFont(labelFont);
    axisY->setTitleFont(labelFont);
    axisY->setRange(0, globalMax + 1);
    axisY->setTickCount(globalMax + 2);
    chart->addAxis(axisY, Qt::AlignLeft);

    // Attach axes
    for (QAbstractSeries *s : chart->series()) {
        s->attachAxis(axisX);
        s->attachAxis(axisY);
    }

    chart->legend()->setVisible(true);
    chart->legend()->setFont(labelFont);

    // ✅ Keep your centered & unclipped layout
    chart->setMargins(QMargins(0, 0, 0, 0));
    chart->layout()->setContentsMargins(0, 0, 0, 0);
    chart->setBackgroundRoundness(0);

    return renderChartToImage(chart, size);
}

// --- Hourly Bar Chart ("When?" — 24 bars, one label per bar) ---
// Mirrors makeBarChartImage. All 24 of the VM's finished hour labels are shown,
// one per category (by POSITION) — the maker never re-derives an hour string.
// An earlier version thinned this to every 3rd label, leaving the other 16
// categories as duplicate empty strings; QBarCategoryAxis derives its plot
// range from the min/max category label, so the duplicates collapsed the range
// and the bars never drew (title/axes still rendered, masking the bug). The peak
// caption rides in the chart TITLE because drawFullscreenChart exposes no seam
// to place a caption below the image (§8.4).
QImage ReportRenderer::makeHourlyBarChartImage(const ReportTimeExport &t, QSize size,
                                               const ReportPalette &palette) {
    const int h = size.height();
    QFont titleFont("Arial");  titleFont.setPixelSize(qMax(10, qRound(h * 0.032)));  titleFont.setBold(true);
    QFont labelFont("Arial");  labelFont.setPixelSize(qMax(8,  qRound(h * 0.024)));

    QBarSet *set = new QBarSet("Visits");
    QStringList categories;
    for (int i = 0; i < t.hourCounts.size(); ++i) {
        *set << t.hourCounts.at(i);
        categories << (i < t.hourLabels.size() ? t.hourLabels.at(i) : QString());
    }
    set->setBrush(palette.chartColors.isEmpty() ? QBrush(palette.headerBg)
                                                : QBrush(palette.chartColors.first()));

    QBarSeries *series = new QBarSeries();
    series->append(set);

    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle(peakHourCaption(t));
    chart->setTitleFont(titleFont);

    QBarCategoryAxis *axisX = new QBarCategoryAxis();
    axisX->append(categories);
    axisX->setLabelsFont(labelFont);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("Number of Visits");
    axisY->setLabelsFont(labelFont);
    axisY->setTitleFont(labelFont);
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    chart->legend()->setVisible(false);
    chart->setMargins(QMargins(0, 0, 0, 0));
    chart->layout()->setContentsMargins(0, 0, 0, 0);
    chart->setBackgroundRoundness(0);

    return renderChartToImage(chart, size);
}

// --- Weekday Bar Chart ("When?" — 7 bars, Mon→Sun) ---
// Mirrors makeBarChartImage; the carrier's weekdayLabels are already Mon→Sun so
// all 7 labels are shown. Peak caption rides in the chart TITLE (§8.4).
QImage ReportRenderer::makeWeekdayBarChartImage(const ReportTimeExport &t, QSize size,
                                                const ReportPalette &palette) {
    const int h = size.height();
    QFont titleFont("Arial");  titleFont.setPixelSize(qMax(10, qRound(h * 0.032)));  titleFont.setBold(true);
    QFont labelFont("Arial");  labelFont.setPixelSize(qMax(8,  qRound(h * 0.024)));

    QBarSet *set = new QBarSet("Visits");
    QStringList categories;
    for (int i = 0; i < t.weekdayCounts.size(); ++i) {
        *set << t.weekdayCounts.at(i);
        categories << (i < t.weekdayLabels.size() ? t.weekdayLabels.at(i) : QString());
    }
    set->setBrush(palette.chartColors.isEmpty() ? QBrush(palette.headerBg)
                                                : QBrush(palette.chartColors.first()));

    QBarSeries *series = new QBarSeries();
    series->append(set);

    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle(peakDayCaption(t));
    chart->setTitleFont(titleFont);

    QBarCategoryAxis *axisX = new QBarCategoryAxis();
    axisX->append(categories);
    axisX->setLabelsFont(labelFont);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("Number of Visits");
    axisY->setLabelsFont(labelFont);
    axisY->setTitleFont(labelFont);
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    chart->legend()->setVisible(false);
    chart->setMargins(QMargins(0, 0, 0, 0));
    chart->layout()->setContentsMargins(0, 0, 0, 0);
    chart->setBackgroundRoundness(0);

    return renderChartToImage(chart, size);
}

// Verbatim port of adminWindow::paintReport (legacy adminwindow.cpp:2018-2291), with:
//  - the top-level QSettings librarian/position reads replaced by info.librarian/info.position
//  - the drawHeader lambda's QSettings schoolName/address/logoPath reads replaced by
//    info.schoolName/info.address/info.logoPath (lambda capture list extended to see info)
//  - the local `ReportPalette palette = getPalette(...)` deleted; the palette parameter
//    is used for every downstream `palette.` reference
//  - both makeLineChartImage(...) call sites (the "All" branch and the non-"All" branch)
//    gaining trailing `, info.openHour, info.closeHour` arguments
bool ReportRenderer::paintReport(QPagedPaintDevice *device, int resolution,
                                 const QJsonArray &data, const QJsonObject &filters,
                                 const ReportPalette &palette,
                                 const ReportHeaderInfo &info,
                                 const ReportAnalytics &analytics, bool includeRoster,
                                 const ReportTimeExport &timeExport)
{
    QPainter painter;
    if (!painter.begin(device)) {
        return false;
    }

    auto finalize = qScopeGuard([&]() {
        if (painter.isActive()) {
            painter.end();
            qCDebug(lcReportRender) << "Report paint finalized successfully.";
        }
    });

    auto safeText = [](const QString &s) -> QString {
        QString clean = s;
        clean.replace(QChar(0xFFFD), "?");
        return clean;
    };

    // Scales a legacy ~96-DPI pixel literal to this device's resolution, so every
    // vertical advance/rect below keeps its original 96-DPI proportion at any DPI
    // (QPdfWriter defaults to 1200; raw literals overlapped there — see scaledPx).
    auto vs = [&](double px) { return ReportRenderer::scaledPx(px, resolution); };

    QRectF pageRect = device->pageLayout().paintRectPixels(resolution);
    int pageWidth  = pageRect.width();
    int pageHeight = pageRect.height();
    int margin     = pageWidth * 0.03;
    int usableWidth  = pageWidth - 2*margin;
    int usableHeight = pageHeight - 2*margin;
    int y = margin;
    int currentPage = 1;
    QString librarian = info.librarian;
    QString position  = info.position;

    auto drawFooter = [&](int pageNum) {
        QFont footerFont("Arial", 8);
        footerFont.setItalic(true);
        painter.setFont(footerFont);
        painter.setPen(Qt::black);

        // Left side: system-generated text
        painter.drawText(QRect(margin, pageHeight - margin - vs(20),
                               usableWidth, vs(20)),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         "This is a system generated report. LOAMS.2 (Library Occupancy and Attendance Monitoring System), WITS 2016.");

        // Right side: page number
        QString footerText = QString("Page %1").arg(pageNum);
        painter.drawText(QRect(margin, pageHeight - margin - vs(20),
                               usableWidth, vs(20)),
                         Qt::AlignRight | Qt::AlignVCenter, footerText);
    };


    qCDebug(lcReportRender) << "Paint Width:" << pageWidth << "Height:" << pageHeight;
    qCDebug(lcReportRender) << "Calculated margin:" << margin;
    qCDebug(lcReportRender) << "Paint Resolution:" << resolution;

    // ===== HEADER =====
    auto drawHeader = [&](int &y) {
        QString schoolName = info.schoolName;
        QString address    = info.address;
        QString logoPath   = info.logoPath;

        int logoSize = pageWidth * 0.08;
        if (!logoPath.isEmpty()) {
            QPixmap logo(logoPath);
            if (!logo.isNull()) {
                QPixmap scaledLogo = logo.scaled(logoSize, logoSize,
                                                 Qt::KeepAspectRatio,
                                                 Qt::SmoothTransformation);
                painter.drawPixmap(QRect(margin, y, logoSize, logoSize),
                                   scaledLogo, scaledLogo.rect());
            }
        }

        int textLeft = margin + logoSize + vs(15);
        int textWidth = usableWidth - logoSize - vs(15);

        painter.setFont(QFont("Times New Roman", 16, QFont::Bold));
        painter.drawText(QRect(textLeft, y, textWidth, vs(30)),
                         Qt::AlignLeft | Qt::AlignVCenter, safeText(schoolName));

        painter.setFont(QFont("Times New Roman", 11));
        painter.drawText(QRect(textLeft, y + vs(25), textWidth, vs(30)),
                         Qt::AlignLeft | Qt::AlignVCenter, safeText(address));

        QString dateStr = QDate::currentDate().toString("dddd, MMMM d, yyyy");
        QString timeStr = QTime::currentTime().toString("hh:mm:ss AP");
        painter.setFont(QFont("Arial", 9));
        painter.drawText(QRect(margin, y, usableWidth, vs(20)), Qt::AlignRight, dateStr);
        painter.drawText(QRect(margin, y + vs(15), usableWidth, vs(20)), Qt::AlignRight, timeStr);

        // Line under header
        y += logoSize + vs(20);
        painter.setPen(Qt::black);
        painter.drawLine(margin, y, pageWidth - margin, y);
        y += vs(30);  // spacing after header
    };
    drawHeader(y);

    // Advances to a fresh page when the next block would overflow the usable
    // area. Declared after drawFooter/drawHeader so both captured lambdas exist.
    auto newPageIfNeeded = [&](int needed) {
        if (y > usableHeight - vs(needed)) {
            drawFooter(currentPage);
            device->newPage();
            currentPage++;
            y = margin;
            drawHeader(y);
            painter.setFont(QFont("Arial", 10));
        }
    };


    // ===== FILTERS =====
    painter.setFont(QFont("Arial", 10));
    QString filtersLine = QString("Department: %1 | Course: %2 | Period: %3 - %4 | School Year: %5")
                              .arg(safeText(filters["department"].toString()))
                              .arg(safeText(filters["course"].toString()))
                              .arg(safeText(filters["start"].toString()))
                              .arg(safeText(filters["end"].toString()))
                              .arg(safeText(filters["schoolYear"].toString()));
    painter.drawText(QRect(margin, y, usableWidth, vs(30)), Qt::AlignLeft, filtersLine);
    y += vs(40);

    // ===== KPI SUMMARY (spec §8: after context, before rankings) =====
    painter.setFont(QFont("Arial", 13, QFont::Bold));
    painter.setPen(Qt::black);
    painter.drawText(QRect(margin, y, usableWidth, vs(24)), Qt::AlignLeft, "Summary");
    y += vs(30);
    painter.setFont(QFont("Arial", 11));
    auto kpiLine = [&](const QString &label, const QString &value) {
        painter.drawText(QRect(margin, y, usableWidth, vs(22)), Qt::AlignLeft,
                         QString("%1: %2").arg(label, value));
        y += vs(24);
    };
    kpiLine("Total Visits", QString::number(analytics.kpis.totalVisits));
    kpiLine("Unique Visitors", QString::number(analytics.kpis.uniqueVisitors));
    kpiLine("Avg. Visits / Visitor", QString::number(analytics.kpis.avgVisitsPerVisitor, 'f', 1));
    kpiLine("Top Department",
            QString("%1 (%2 visits)")
                .arg(analytics.kpis.hasData ? safeText(analytics.kpis.topDepartment) : QStringLiteral("—"))
                .arg(analytics.kpis.topDepartmentVisits));
    y += vs(16);

    // ===== RANKINGS (spec §8) =====
    auto drawRanking = [&](const QString &heading, const QList<RankingEntry> &entries,
                           bool withSublabel, bool withPercent) {
        newPageIfNeeded(220);
        painter.setFont(QFont("Arial", 12, QFont::Bold));
        painter.setPen(Qt::black);
        painter.drawText(QRect(margin, y, usableWidth, vs(22)), Qt::AlignLeft, heading);
        y += vs(28);
        painter.setFont(QFont("Arial", 10));
        const QFontMetrics rfm = painter.fontMetrics();
        const int pitch = qMax(vs(20), rfm.height() + vs(4));
        int idx = 0;
        for (const RankingEntry &e : entries) {
            newPageIfNeeded(80);
            const int cRank = margin;
            const int cLabel = margin + int(usableWidth * 0.10);
            const int cSub = margin + int(usableWidth * 0.55);
            const int cVisits = margin + int(usableWidth * 0.78);
            const int cPct = margin + int(usableWidth * 0.90);
            painter.fillRect(QRect(margin, y - rfm.ascent(), usableWidth, pitch),
                             (idx % 2 == 0) ? palette.rowEvenBg : palette.rowOddBg);
            painter.setPen(palette.rowText);
            painter.drawText(cRank, y, QString::number(e.rank));
            painter.drawText(cLabel, y, rfm.elidedText(safeText(e.label), Qt::ElideRight, cSub - cLabel - vs(5)));
            if (withSublabel)
                painter.drawText(cSub, y, rfm.elidedText(safeText(e.sublabel), Qt::ElideRight, cVisits - cSub - vs(5)));
            painter.drawText(cVisits, y, QString::number(e.visits));
            if (withPercent)
                painter.drawText(cPct, y, QString::number(e.percentOfTotal, 'f', 1) + "%");
            y += pitch;
            idx++;
        }
        y += vs(16);
    };
    drawRanking("Top 10 Students", analytics.topStudents, true, false);
    drawRanking("Top 10 Courses", analytics.topCourses, false, true);
    drawRanking("Top 10 Departments", analytics.topDepartments, false, true);

    // ===== CHARTS: each chart placed on its own page and scaled to fill almost whole page =====
    auto drawFullscreenChart = [&](const QString &label, const QImage &img) {
        if (img.isNull()) {
            qCDebug(lcReportRender) << label << "is null, skipping.";
            return;
        }

        drawFooter(currentPage);
        device->newPage();
        currentPage++;
        y = margin;        // reset Y for the new page
        drawHeader(y);
        qCDebug(lcReportRender) << "New page created for chart (" << label << "), page:" << currentPage;

        // Compute area for chart (leave space for footer)
        const int bottomReserve = vs(60);
        QRect targetArea(margin, y, usableWidth, pageHeight - y - margin - bottomReserve);

        // Scale preserving aspect ratio
        QSize scaledSize = img.size().scaled(targetArea.size(), Qt::KeepAspectRatio);

        // Center in target area
        int x = targetArea.left() + (targetArea.width() - scaledSize.width()) / 2;
        int y = targetArea.top() + (targetArea.height() - scaledSize.height()) / 2;
        QRect drawRect(x, y, scaledSize.width(), scaledSize.height());

        // Draw the image at the calculated rect
        painter.drawImage(drawRect, img);

        qCDebug(lcReportRender) << "Chart" << label << "drawn at rect:" << drawRect
                 << "from image size:" << img.size();
    };

    QString chartChoice = filters["chartType"].toString();

    // Chart raster sizes key off usableWidth via chartImageSize (bar/line = 5:3
    // landscape, pie = square) so they scale with the page at any DPI — no fixed
    // pixel height that would degenerate into a sliver. The chart makers size their
    // fonts to the raster so labels stay legible once drawn onto the high-DPI page.
    if (chartChoice.contains("All", Qt::CaseInsensitive)) {
        QSize barSize  = chartImageSize(usableWidth, false); // Wide rectangle for bar charts
        QSize pieSize  = chartImageSize(usableWidth, true);  // Square for pie charts
        QSize lineSize = chartImageSize(usableWidth, false); // Wide rectangle for line charts

        drawFullscreenChart("Bar Chart",  makeBarChartImage(data, barSize, palette));
        drawFullscreenChart("Pie Chart",  makePieChartImage(data, pieSize, palette));
        drawFullscreenChart("Line Chart", makeLineChartImage(data, lineSize, palette, info.openHour, info.closeHour));
    } else if (chartChoice.contains("Pie", Qt::CaseInsensitive)) {
        QSize pieSize = chartImageSize(usableWidth, true);  // Square dimensions
        drawFullscreenChart("Pie Chart", makePieChartImage(data, pieSize, palette));
    } else {
        // For bar and line charts, use rectangular size
        QSize rectSize = chartImageSize(usableWidth, false);
        if (chartChoice.contains("Bar", Qt::CaseInsensitive)) {
            drawFullscreenChart("Bar Chart", makeBarChartImage(data, rectSize, palette));
        } else if (chartChoice.contains("Line", Qt::CaseInsensitive)) {
            drawFullscreenChart("Line Chart", makeLineChartImage(data, rectSize, palette, info.openHour, info.closeHour));
        }
    }

    // ===== WHEN? TIME ANALYTICS (spec 4b-iv-b §8.4) =====
    // Inserted BEFORE the terminal drawFooter so that footer foots the LAST
    // When? page. Placement note: drawFullscreenChart foots the PRIOR page at
    // ENTRY (never its own page at exit) and declares a local `y` shadowing the
    // outer one, exposing no seam for a caption — so the Data path's captions
    // ride in each chart TITLE, and the Empty/Error note (which has no
    // entry-foot of its own) must foot the current page itself before paging.
    switch (timeExport.state) {
    case TimeAnalyticsExportState::Disabled:
        break;   // legacy WITS.exe parity — draw nothing, advance no page
    case TimeAnalyticsExportState::Data: {
        const QSize whenSize = chartImageSize(usableWidth, false);   // screen-safe; upscaled by drawFullscreenChart
        drawFullscreenChart("Hourly Visits",
                            makeHourlyBarChartImage(timeExport, whenSize, palette));
        drawFullscreenChart("Visits by Day",
                            makeWeekdayBarChartImage(timeExport, whenSize, palette));
        break;
    }
    case TimeAnalyticsExportState::Empty:
    case TimeAnalyticsExportState::Error: {
        // The note can't share the last course-chart page (that page holds a
        // chart image and the outer y sits near the header). Foot the current
        // page FIRST, then open a fresh page and draw the note in normal y-flow.
        drawFooter(currentPage);
        device->newPage();
        currentPage++;
        y = margin;
        drawHeader(y);
        painter.setFont(QFont("Arial", 13, QFont::Bold));
        painter.setPen(Qt::black);
        painter.drawText(QRect(margin, y, usableWidth, vs(24)), Qt::AlignLeft,
                         whenSectionTitle());
        y += vs(30);
        painter.setFont(QFont("Arial", 11));
        const QString note = timeExportNoteText(timeExport.state);
        painter.drawText(QRect(margin, y, usableWidth, vs(22)), Qt::AlignLeft, note);
        y += vs(24);
        break;
    }
    }

    // Footer on the last page with current page number
    drawFooter(currentPage);

    // ===== DETAILED ROSTER ===== (gated: the per-student roster is opt-in — spec §9)
    // MUST open its own page unconditionally: drawFullscreenChart resets the outer
    // `y` back near the top of the last chart page, so newPageIfNeeded would be a
    // no-op here and the roster would overprint the chart image. The last content
    // page was already footed just above, so page forward directly.
    if (includeRoster) {
        device->newPage();
        currentPage++;
        y = margin;
        drawHeader(y);

        painter.setFont(QFont("Arial", 12, QFont::Bold));
        painter.setPen(Qt::black);
        painter.drawText(QRect(margin, y, usableWidth, vs(22)), Qt::AlignLeft, "Detailed Roster");
        y += vs(30);

        // --- Define column widths (8 columns total) ---
        int col1 = margin;                                   // School ID
        int col2 = margin + (usableWidth * 0.12);            // Name
        int col3 = margin + (usableWidth * 0.32);            // Gender
        int col4 = margin + (usableWidth * 0.42);            // Status
        int col5 = margin + (usableWidth * 0.55);            // Course
        int col6 = margin + (usableWidth * 0.70);            // Department
        int col7 = margin + (usableWidth * 0.85);            // Year Level
        int col8 = margin + (usableWidth * 0.95);            // Visits

        // Row rhythm: pin the row font, then floor the per-row advance at the glyph
        // box (height + a little leading) so variable-length data never overlaps even
        // after the DPI scaling above — this is the guard against the original bug.
        painter.setFont(QFont("Arial", 10));
        const QFontMetrics fm = painter.fontMetrics();
        const int rowPitch = qMax(vs(20), fm.height() + vs(4));

        // --- Draw header row ---
        painter.fillRect(QRect(margin, y - vs(15), usableWidth, vs(20)), palette.headerBg);
        painter.setPen(palette.headerText);

        painter.drawText(col1, y, "School ID");
        painter.drawText(col2, y, "Name");
        painter.drawText(col3, y, "Gender");
        painter.drawText(col4, y, "Status");
        painter.drawText(col5, y, "Course");
        painter.drawText(col6, y, "Department");
        painter.drawText(col7, y, "Year Level");
        painter.drawText(col8, y, "Visits");

        y += vs(25);
        painter.setPen(Qt::black);
        painter.drawLine(margin, y, pageWidth - margin, y);
        y += vs(20);

        int rowIndex = 0;
        for (auto v : data) {
            QJsonObject row = v.toObject();

            // Fill band tiles exactly at rowPitch so bands abut without gaps/overlap.
            QRect rowRect(margin, y - fm.ascent(), usableWidth, rowPitch);
            painter.fillRect(rowRect, (rowIndex % 2 == 0) ? palette.rowEvenBg : palette.rowOddBg);

            painter.setPen(palette.rowText);

            QString schoolId   = fm.elidedText(safeText(row["school_id"].toString()), Qt::ElideRight, col2 - col1 - vs(5));
            QString name       = fm.elidedText(safeText(row["name"].toString()), Qt::ElideRight, col3 - col2 - vs(5));
            QString gender     = fm.elidedText(safeText(row["gender"].toString()), Qt::ElideRight, col4 - col3 - vs(5));
            QString status     = fm.elidedText(safeText(row["status"].toString()), Qt::ElideRight, col5 - col4 - vs(5));
            QString course     = fm.elidedText(safeText(row["course"].toString()), Qt::ElideRight, col6 - col5 - vs(5));
            QString department = fm.elidedText(safeText(row["department"].toString()), Qt::ElideRight, col7 - col6 - vs(5));
            QString yearLevel  = fm.elidedText(safeText(row["year_level"].toString()), Qt::ElideRight, col8 - col7 - vs(5));

            painter.drawText(col1, y, schoolId);
            painter.drawText(col2, y, name);
            painter.drawText(col3, y, gender);
            painter.drawText(col4, y, status);
            painter.drawText(col5, y, course);
            painter.drawText(col6, y, department);
            painter.drawText(col7, y, yearLevel);
            painter.drawText(col8, y, QString::number(row["visits"].toInt()));

            y += rowPitch;
            rowIndex++;

            // newPageIfNeeded already restores the Arial 10 row font after
            // drawHeader leaves it at Arial 9, so continuation-page rows draw
            // at the same font that fm measures.
            newPageIfNeeded(200);
        }

        // Foot the roster's LAST page — the prepared-by section opens a fresh page next.
        drawFooter(currentPage);
    } // if (includeRoster)

    // ===== PREPARED BY =====
    device->newPage();
    currentPage++;

    y = margin + vs(100);
    drawHeader(y);

    painter.setFont(QFont("Arial", 11, QFont::Bold));
    painter.setPen(Qt::black);
    painter.drawText(QRect(margin, y, pageWidth - 2*margin, vs(25)),
                     Qt::AlignCenter, QString("Prepared by: %1").arg(safeText(librarian)));
    y += vs(25);

    painter.setFont(QFont("Arial", 10));
    painter.drawText(QRect(margin, y, pageWidth - 2*margin, vs(20)),
                     Qt::AlignCenter, safeText(position));
    y += vs(40);

    int sigWidth = vs(240);
    int sigX = (pageWidth - sigWidth) / 2;
    painter.drawLine(sigX, y, sigX + sigWidth, y);
    painter.drawText(QRect(sigX, y + vs(5), sigWidth, vs(20)), Qt::AlignCenter, "(Signature)");
    drawFooter(currentPage);

    return true;
}

// Ports the DOCUMENT-BUILDING BODY ONLY of adminWindow::exportReportToExcel
// (legacy adminwindow.cpp:2356-2474). The rows.isEmpty() QMessageBox guard,
// QFileDialog::getSaveFileName, and the local QXlsx::Document construction are
// omitted — the caller owns the QXlsx::Document and passes it by reference.
// The saveAs call and success/failure QMessageBox are also omitted — the caller
// does that; this function just builds the document and returns true.
bool ReportRenderer::writeReportToXlsx(QXlsx::Document &xlsx,
                                       const QJsonArray &rows,
                                       const QJsonObject &filters,
                                       const ReportHeaderInfo &info,
                                       const ReportAnalytics &analytics,
                                       bool includeRoster,
                                       const ReportTimeExport &timeExport)
{
    const int colCount = 8;

    QXlsx::Format titleFmt;
    titleFmt.setFontBold(true);
    titleFmt.setFontSize(16);
    titleFmt.setHorizontalAlignment(QXlsx::Format::AlignHCenter);

    QXlsx::Format subTitleFmt;
    subTitleFmt.setFontSize(11);
    subTitleFmt.setHorizontalAlignment(QXlsx::Format::AlignHCenter);

    QXlsx::Format sectionFmt;
    sectionFmt.setFontBold(true);
    sectionFmt.setFontSize(12);

    QXlsx::Format hdrFmt;
    hdrFmt.setFontBold(true);
    hdrFmt.setPatternBackgroundColor(QColor("#D6EAF8"));
    hdrFmt.setHorizontalAlignment(QXlsx::Format::AlignHCenter);

    // ===== SHEET 1: SUMMARY (renamed from the default sheet) =====
    // QXlsx::Document::sheetNames() reads the workbook's sheet-name list directly
    // and does NOT trigger the lazy "Sheet1" creation that only happens inside
    // Workbook::activeSheet() (see xlsxworkbook.cpp). On a brand-new Document,
    // sheetNames() is therefore still empty here; currentSheet() forces that
    // lazy creation so sheetNames().first() below is never called on an empty list.
    xlsx.currentSheet();
    xlsx.renameSheet(xlsx.sheetNames().first(), QStringLiteral("Summary"));
    xlsx.selectSheet(QStringLiteral("Summary"));

    int row = 1;
    xlsx.mergeCells(QXlsx::CellRange(row, 1, row, colCount), titleFmt);
    xlsx.write(row++, 1, info.schoolName, titleFmt);
    xlsx.mergeCells(QXlsx::CellRange(row, 1, row, colCount), subTitleFmt);
    xlsx.write(row++, 1, info.address, subTitleFmt);
    xlsx.mergeCells(QXlsx::CellRange(row, 1, row, colCount), subTitleFmt);
    xlsx.write(row++, 1, QString("Library Report - %1 to %2")
                             .arg(filters["start"].toString(), filters["end"].toString()), subTitleFmt);
    row += 1;

    xlsx.write(row++, 1, QString("Department: %1 | Course: %2 | School Year: %3")
                             .arg(filters["department"].toString(),
                                  filters["course"].toString(),
                                  filters["schoolYear"].toString()));
    row += 1;

    // --- KPI block (label | value pairs) ---
    xlsx.write(row++, 1, QStringLiteral("Summary"), sectionFmt);
    xlsx.write(row, 1, QStringLiteral("Total Visits"));
    xlsx.write(row++, 2, analytics.kpis.totalVisits);
    xlsx.write(row, 1, QStringLiteral("Unique Visitors"));
    xlsx.write(row++, 2, analytics.kpis.uniqueVisitors);
    xlsx.write(row, 1, QStringLiteral("Avg. Visits / Visitor"));
    xlsx.write(row++, 2, QString::number(analytics.kpis.avgVisitsPerVisitor, 'f', 1));
    xlsx.write(row, 1, QStringLiteral("Top Department"));
    xlsx.write(row, 2, analytics.kpis.hasData ? sanitizeXlsxText(analytics.kpis.topDepartment)
                                               : QStringLiteral("—"));
    xlsx.write(row++, 3, analytics.kpis.topDepartmentVisits);
    row += 1;

    // --- Ranking tables ---
    auto writeRanking = [&](const QString &heading, const QStringList &headers,
                            const QList<RankingEntry> &entries, bool withSublabel, bool withPercent) {
        xlsx.write(row++, 1, heading, sectionFmt);
        for (int c = 0; c < headers.size(); ++c)
            xlsx.write(row, c + 1, headers[c], hdrFmt);
        row++;
        for (const RankingEntry &e : entries) {
            int c = 1;
            xlsx.write(row, c++, e.rank);
            xlsx.write(row, c++, sanitizeXlsxText(e.label));
            if (withSublabel) xlsx.write(row, c++, sanitizeXlsxText(e.sublabel));
            xlsx.write(row, c++, e.visits);
            if (withPercent) xlsx.write(row, c++, QString::number(e.percentOfTotal, 'f', 1) + "%");
            row++;
        }
        row += 1;
    };
    writeRanking(QStringLiteral("Top 10 Students"),
                 { "Rank", "Name", "Course", "Visits" }, analytics.topStudents, true, false);
    writeRanking(QStringLiteral("Top 10 Courses"),
                 { "Rank", "Course", "Visits", "% of Total" }, analytics.topCourses, false, true);
    writeRanking(QStringLiteral("Top 10 Departments"),
                 { "Rank", "Department", "Visits", "% of Total" }, analytics.topDepartments, false, true);

    // ===== WHEN? TIME ANALYTICS (spec 4b-iv-b §10) =====
    // Side-by-side tables on the Summary sheet, below the rankings. Every
    // label/caption/header cell runs through sanitizeXlsxText for a single
    // uniform escaping path (count cells are integers, no sanitize needed).
    switch (timeExport.state) {
    case TimeAnalyticsExportState::Disabled:
        break;   // legacy WITS.exe parity — write nothing
    case TimeAnalyticsExportState::Error:
    case TimeAnalyticsExportState::Empty: {
        xlsx.write(row++, 1, sanitizeXlsxText(whenSectionTitle()), sectionFmt);
        const QString note = timeExportNoteText(timeExport.state);
        xlsx.write(row++, 1, sanitizeXlsxText(note));
        row += 1;
        break;
    }
    case TimeAnalyticsExportState::Data: {
        xlsx.write(row++, 1, sanitizeXlsxText(whenSectionTitle()), sectionFmt);

        // Peak-label row: hourly caption in col 1, weekday caption a few cols over.
        xlsx.write(row, 1, sanitizeXlsxText(peakHourCaption(timeExport)));
        xlsx.write(row, 4, sanitizeXlsxText(peakDayCaption(timeExport)));
        row++;

        // Two tables SIDE-BY-SIDE sharing one header row: hourly (cols 1-2, 24
        // rows) and weekday (cols 4-5, 7 rows). The single-cursor writeRanking
        // lambda can't drive two columns, so address cells directly by baseRow.
        const int baseRow = row;
        xlsx.write(baseRow, 1, sanitizeXlsxText(QStringLiteral("Hour")),  hdrFmt);
        xlsx.write(baseRow, 2, sanitizeXlsxText(QStringLiteral("Count")), hdrFmt);
        xlsx.write(baseRow, 4, sanitizeXlsxText(QStringLiteral("Day")),   hdrFmt);
        xlsx.write(baseRow, 5, sanitizeXlsxText(QStringLiteral("Count")), hdrFmt);
        for (int i = 0; i < qMin(timeExport.hourLabels.size(), timeExport.hourCounts.size()); ++i) {
            xlsx.write(baseRow + 1 + i, 1, sanitizeXlsxText(timeExport.hourLabels.at(i)));
            xlsx.write(baseRow + 1 + i, 2, timeExport.hourCounts.at(i));
        }
        for (int i = 0; i < qMin(timeExport.weekdayLabels.size(), timeExport.weekdayCounts.size()); ++i) {
            xlsx.write(baseRow + 1 + i, 4, sanitizeXlsxText(timeExport.weekdayLabels.at(i)));
            xlsx.write(baseRow + 1 + i, 5, timeExport.weekdayCounts.at(i));
        }
        // Advance the cursor past the TALLER (24-row hourly) table so the
        // system-generated footer that follows lands below the whole block.
        row = baseRow + 1 + timeExport.hourCounts.size();
        row += 1;
        break;
    }
    }

    xlsx.write(row++, 1,
               "This is a system-generated report. LOAMS.2 (Library Occupancy and Attendance Monitoring System), WITS 2016.");
    row += 1;
    xlsx.write(row++, 1, QString("Prepared by: %1").arg(info.librarian));
    xlsx.write(row++, 1, info.position);

    // ===== SHEET 2: DETAILED ROSTER (only when requested) =====
    if (includeRoster) {
        xlsx.addSheet(QStringLiteral("Detailed Roster"));   // becomes current
        int rr = 1;
        const QStringList headers = {"School ID", "Name", "Gender", "Course",
                                     "Year Level", "Department", "Status", "Visits"};
        for (int c = 0; c < headers.size(); ++c)
            xlsx.write(rr, c + 1, headers[c], hdrFmt);
        rr++;
        QXlsx::Format evenFmt, oddFmt;
        evenFmt.setPatternBackgroundColor(QColor("#F9F9F9"));
        oddFmt.setPatternBackgroundColor(QColor("#FFFFFF"));
        for (const auto &val : rows) {
            const QJsonObject obj = val.toObject();
            const QStringList rowData = {
                sanitizeXlsxText(obj["school_id"].toString()), sanitizeXlsxText(obj["name"].toString()),
                sanitizeXlsxText(obj["gender"].toString()), sanitizeXlsxText(obj["course"].toString()),
                sanitizeXlsxText(obj["year_level"].toString()), sanitizeXlsxText(obj["department"].toString()),
                sanitizeXlsxText(obj["status"].toString()), QString::number(obj["visits"].toInt())
            };
            for (int c = 0; c < rowData.size(); ++c)
                xlsx.write(rr, c + 1, rowData[c], (rr % 2 == 0) ? evenFmt : oddFmt);
            rr++;
        }
        for (int c = 0; c < headers.size(); ++c)
            xlsx.setColumnWidth(c + 1, headers[c].length() + 5);
        xlsx.selectSheet(QStringLiteral("Summary"));   // leave Summary active
    }

    return true;
}
