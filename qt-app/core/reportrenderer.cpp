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
                                 const ReportAnalytics &analytics, bool includeRoster)
{
    QPainter painter;
    if (!painter.begin(device)) {
        return false;
    }

    Q_UNUSED(analytics);   // consumed by the KPI/ranking layouts in Task 4

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

    // ===== TABLE ===== (gated: the per-student roster is opt-in — spec §9)
    if (includeRoster) {

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

        if (y > usableHeight - vs(200)) {
            drawFooter(currentPage);
            device->newPage();
            currentPage++;
            y = margin;
            drawHeader(y);
            // drawHeader leaves the font at Arial 9; restore the row font so
            // continuation-page rows draw at the same Arial 10 that fm measures.
            painter.setFont(QFont("Arial", 10));
        }
    }

    } // if (includeRoster)

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

    // Footer on the last page with current page number
    drawFooter(currentPage);

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
                                       const ReportAnalytics &analytics, bool includeRoster)
{
    Q_UNUSED(analytics);   // consumed by the Summary sheet in Task 3

    // ===== HEADER =====
    QString schoolName = info.schoolName;
    QString address    = info.address;
    QString librarian  = info.librarian;
    QString position   = info.position;

    QXlsx::Format titleFmt;
    titleFmt.setFontBold(true);
    titleFmt.setFontSize(16);
    titleFmt.setHorizontalAlignment(QXlsx::Format::AlignHCenter);

    QXlsx::Format subTitleFmt;
    subTitleFmt.setFontSize(11);
    subTitleFmt.setHorizontalAlignment(QXlsx::Format::AlignHCenter);

    int row = 1;
    int colCount = 8; // ID, Name, Gender, Course, Year Level, Department, Status, Visits

    xlsx.mergeCells(QXlsx::CellRange(row, 1, row, colCount), titleFmt);
    xlsx.write(row++, 1, schoolName, titleFmt);

    xlsx.mergeCells(QXlsx::CellRange(row, 1, row, colCount), subTitleFmt);
    xlsx.write(row++, 1, address, subTitleFmt);

    xlsx.mergeCells(QXlsx::CellRange(row, 1, row, colCount), subTitleFmt);
    xlsx.write(row++, 1, QString("Library Report - %1 to %2")
                             .arg(filters["start"].toString(), filters["end"].toString()), subTitleFmt);
    row += 1;

    // ===== FILTERS =====
    xlsx.write(row++, 1, QString("Department: %1 | Course: %2 | School Year: %3")
                             .arg(filters["department"].toString(),
                                  filters["course"].toString(),
                                  filters["schoolYear"].toString()));

    row += 1;

    // ===== TABLE HEADERS =====
    QStringList headers = {"School ID", "Name", "Gender", "Course",
                           "Year Level", "Department", "Status", "Visits"};

    QXlsx::Format hdrFmt;
    hdrFmt.setFontBold(true);
    hdrFmt.setPatternBackgroundColor(QColor("#D6EAF8"));
    hdrFmt.setHorizontalAlignment(QXlsx::Format::AlignHCenter);

    for (int c = 0; c < headers.size(); ++c) {
        xlsx.write(row, c + 1, headers[c], hdrFmt);
    }

    // Freeze the header row
    //xlsx.currentWorksheet()->freezePane(QXlsx::CellRange(row + 1, 1, row + 1, 1));
    row++;

    // ===== TABLE ROWS ===== (gated: the per-student roster is opt-in — spec §9)
    if (includeRoster) {
        QXlsx::Format evenFmt, oddFmt;
        evenFmt.setPatternBackgroundColor(QColor("#F9F9F9"));
        oddFmt.setPatternBackgroundColor(QColor("#FFFFFF"));

        for (const auto &val : rows) {
            QJsonObject obj = val.toObject();
            QStringList rowData = {
                obj["school_id"].toString(),
                obj["name"].toString(),
                obj["gender"].toString(),
                obj["course"].toString(),
                obj["year_level"].toString(),
                obj["department"].toString(),
                obj["status"].toString(),
                QString::number(obj["visits"].toInt())
            };

            for (int c = 0; c < rowData.size(); ++c) {
                xlsx.write(row, c + 1, rowData[c], (row % 2 == 0) ? evenFmt : oddFmt);
            }
            row++;
        }
    }

    // Auto-fit columns (simulate by setting width based on text length)
    for (int c = 0; c < headers.size(); ++c) {
        xlsx.setColumnWidth(c + 1, headers[c].length() + 5);
    }

    // ===== FOOTER =====
    row += 2;
    xlsx.mergeCells(QXlsx::CellRange(row, 1, row, colCount));
    xlsx.write(row++, 1,
               "This is a system-generated report. LOAMS.2 (Library Occupancy and Attendance Monitoring System), WITS 2016.");

    row += 2;
    xlsx.mergeCells(QXlsx::CellRange(row, 1, row, colCount));
    xlsx.write(row++, 1, QString("Prepared by: %1").arg(librarian));

    xlsx.mergeCells(QXlsx::CellRange(row, 1, row, colCount));
    xlsx.write(row++, 1, position);

    return true;
}
