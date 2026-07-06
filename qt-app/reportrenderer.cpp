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
#include <QMarginsF>
#include <QPagedPaintDevice>
#include <QPainter>
#include <QPixmap>
#include <QRect>
#include <QScopeGuard>
#include <QTime>

#include "xlsxformat.h"     // QXlsx::Format  (match adminwindow.cpp's include)
#include "xlsxcellrange.h"  // QXlsx::CellRange (mergeCells)

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

// Shared render tail of the three chart makers: paint a configured QChart into
// an ARGB32 QImage of the requested size. A local QChartView owns `chart` and
// deletes it when this returns — same ownership as the inline versions it replaces.
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

    //color set
    QBarSeries *series = new QBarSeries();
    QStringList categories;
    QVector<QColor> colors = { palette.rowEvenBg, palette.rowOddBg, palette.headerBg };

    int colorIndex = 0;
    for (auto it = courseCounts.begin(); it != courseCounts.end(); ++it) {
        QBarSet *set = new QBarSet(it.key());
        *set << it.value();
        set->setBrush(palette.chartColors[colorIndex % palette.chartColors.size()]);
        series->append(set);
        categories << it.key();
        colorIndex++;
    }

    // Create chart
    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("Library Visits by Course");
    chart->setTitleFont(QFont("Arial", 16, QFont::Bold));

    QBarCategoryAxis *axisX = new QBarCategoryAxis();
    axisX->append(categories);
    axisX->setLabelsFont(QFont("Arial", 12));
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("Number of Visits");
    axisY->setLabelsFont(QFont("Arial", 12));
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    chart->legend()->setVisible(true);
    chart->legend()->setFont(QFont("Arial", 12));

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

    // Create pie series
    QPieSeries *series = new QPieSeries();
    QVector<QColor> colors = { palette.rowEvenBg, palette.rowOddBg, palette.headerBg };
    int colorIndex = 0;
    for (auto it = courseCounts.begin(); it != courseCounts.end(); ++it) {
        QPieSlice *slice = series->append(it.key(), it.value());
        slice->setLabel(QString("%1: %2").arg(it.key()).arg(it.value()));
        slice->setLabelVisible(true);
        slice->setBrush(palette.chartColors[colorIndex % palette.chartColors.size()]);
        slice->setLabelFont(QFont("Arial", 14, QFont::Bold)); // ✅ bigger font
        colorIndex++;
    }

    // Create chart
    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("Library Visits by Course");
    chart->setTitleFont(QFont("Arial", 16, QFont::Bold));
    chart->legend()->setVisible(true);
    chart->legend()->setFont(QFont("Arial", 12));
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

    int globalMax = 0;
    for (auto it = courseTimeCounts.begin(); it != courseTimeCounts.end(); ++it)
        for (int count : it.value())
            if (count > globalMax)
                globalMax = count;

    QChart *chart = new QChart();
    chart->setTitle("Library Peak Hours by Course");
    chart->setTitleFont(QFont("Arial", 16, QFont::Bold));

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
    axisX->setLabelsFont(QFont("Arial", 10));
    chart->addAxis(axisX, Qt::AlignBottom);

    // Y axis = number of students (auto-scale)
    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("Number of Students");
    axisY->setLabelsFont(QFont("Arial", 10));
    axisY->setRange(0, globalMax + 1);
    axisY->setTickCount(globalMax + 2);
    chart->addAxis(axisY, Qt::AlignLeft);

    // Attach axes
    for (QAbstractSeries *s : chart->series()) {
        s->attachAxis(axisX);
        s->attachAxis(axisY);
    }

    chart->legend()->setVisible(true);
    chart->legend()->setFont(QFont("Arial", 12));

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
                                 const ReportHeaderInfo &info)
{
    QPainter painter;
    if (!painter.begin(device)) {
        return false;
    }

    auto finalize = qScopeGuard([&]() {
        if (painter.isActive()) {
            painter.end();
            qDebug() << "Report paint finalized successfully.";
        }
    });

    auto safeText = [](const QString &s) -> QString {
        QString clean = s;
        clean.replace(QChar(0xFFFD), "?");
        return clean;
    };

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
        painter.drawText(QRect(margin, pageHeight - margin - 20,
                               usableWidth, 20),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         "This is a system generated report. LOAMS.2 (Library Occupancy and Attendance Monitoring System), WITS 2016.");

        // Right side: page number
        QString footerText = QString("Page %1").arg(pageNum);
        painter.drawText(QRect(margin, pageHeight - margin - 20,
                               usableWidth, 20),
                         Qt::AlignRight | Qt::AlignVCenter, footerText);
    };


    qDebug() << "Paint Width:" << pageWidth << "Height:" << pageHeight;
    qDebug() << "Calculated margin:" << margin;
    qDebug() << "Paint Resolution:" << resolution;

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

        int textLeft = margin + logoSize + 15;
        int textWidth = usableWidth - logoSize - 15;

        painter.setFont(QFont("Times New Roman", 16, QFont::Bold));
        painter.drawText(QRect(textLeft, y, textWidth, 30),
                         Qt::AlignLeft | Qt::AlignVCenter, safeText(schoolName));

        painter.setFont(QFont("Times New Roman", 11));
        painter.drawText(QRect(textLeft, y + 25, textWidth, 30),
                         Qt::AlignLeft | Qt::AlignVCenter, safeText(address));

        QString dateStr = QDate::currentDate().toString("dddd, MMMM d, yyyy");
        QString timeStr = QTime::currentTime().toString("hh:mm:ss AP");
        painter.setFont(QFont("Arial", 9));
        painter.drawText(QRect(margin, y, usableWidth, 20), Qt::AlignRight, dateStr);
        painter.drawText(QRect(margin, y + 15, usableWidth, 20), Qt::AlignRight, timeStr);

        // Line under header
        y += logoSize + 20;
        painter.setPen(Qt::black);
        painter.drawLine(margin, y, pageWidth - margin, y);
        y += 30;  // spacing after header
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
    painter.drawText(QRect(margin, y, usableWidth, 30), Qt::AlignLeft, filtersLine);
    y += 40;

    // ===== TABLE =====

    // --- Define column widths (8 columns total) ---
    int col1 = margin;                                   // School ID
    int col2 = margin + (usableWidth * 0.12);            // Name
    int col3 = margin + (usableWidth * 0.32);            // Gender
    int col4 = margin + (usableWidth * 0.42);            // Status
    int col5 = margin + (usableWidth * 0.55);            // Course
    int col6 = margin + (usableWidth * 0.70);            // Department
    int col7 = margin + (usableWidth * 0.85);            // Year Level
    int col8 = margin + (usableWidth * 0.95);            // Visits

    // --- Draw header row ---
    painter.fillRect(QRect(margin, y - 15, usableWidth, 20), palette.headerBg);
    painter.setPen(palette.headerText);

    painter.drawText(col1, y, "School ID");
    painter.drawText(col2, y, "Name");
    painter.drawText(col3, y, "Gender");
    painter.drawText(col4, y, "Status");
    painter.drawText(col5, y, "Course");
    painter.drawText(col6, y, "Department");
    painter.drawText(col7, y, "Year Level");
    painter.drawText(col8, y, "Visits");

    y += 25;
    painter.setPen(Qt::black);
    painter.drawLine(margin, y, pageWidth - margin, y);
    y += 20;

    int rowIndex = 0;
    for (auto v : data) {
        QJsonObject row = v.toObject();

        QRect rowRect(margin, y - 15, usableWidth, 20);
        painter.fillRect(rowRect, (rowIndex % 2 == 0) ? palette.rowEvenBg : palette.rowOddBg);

        painter.setPen(palette.rowText);
        QFontMetrics fm = painter.fontMetrics();

        QString schoolId   = fm.elidedText(safeText(row["school_id"].toString()), Qt::ElideRight, col2 - col1 - 5);
        QString name       = fm.elidedText(safeText(row["name"].toString()), Qt::ElideRight, col3 - col2 - 5);
        QString gender     = fm.elidedText(safeText(row["gender"].toString()), Qt::ElideRight, col4 - col3 - 5);
        QString status     = fm.elidedText(safeText(row["status"].toString()), Qt::ElideRight, col5 - col4 - 5);
        QString course     = fm.elidedText(safeText(row["course"].toString()), Qt::ElideRight, col6 - col5 - 5);
        QString department = fm.elidedText(safeText(row["department"].toString()), Qt::ElideRight, col7 - col6 - 5);
        QString yearLevel  = fm.elidedText(safeText(row["year_level"].toString()), Qt::ElideRight, col8 - col7 - 5);

        painter.drawText(col1, y, schoolId);
        painter.drawText(col2, y, name);
        painter.drawText(col3, y, gender);
        painter.drawText(col4, y, status);
        painter.drawText(col5, y, course);
        painter.drawText(col6, y, department);
        painter.drawText(col7, y, yearLevel);
        painter.drawText(col8, y, QString::number(row["visits"].toInt()));

        y += 20;
        rowIndex++;

        if (y > usableHeight - 200) {
            drawFooter(currentPage);
            device->newPage();
            currentPage++;
            y = margin;
            drawHeader(y);
        }
    }

    // ===== CHARTS: each chart placed on its own page and scaled to fill almost whole page =====
    auto drawFullscreenChart = [&](const QString &label, const QImage &img) {
        if (img.isNull()) {
            qDebug() << label << "is null, skipping.";
            return;
        }

        drawFooter(currentPage);
        device->newPage();
        currentPage++;
        y = margin;        // reset Y for the new page
        drawHeader(y);
        qDebug() << "New page created for chart (" << label << "), page:" << currentPage;

        // Compute area for chart (leave space for footer)
        const int bottomReserve = 60;
        QRect targetArea(margin, y, usableWidth, pageHeight - y - margin - bottomReserve);

        // Scale preserving aspect ratio
        QSize scaledSize = img.size().scaled(targetArea.size(), Qt::KeepAspectRatio);

        // Center in target area
        int x = targetArea.left() + (targetArea.width() - scaledSize.width()) / 2;
        int y = targetArea.top() + (targetArea.height() - scaledSize.height()) / 2;
        QRect drawRect(x, y, scaledSize.width(), scaledSize.height());

        // Draw the image at the calculated rect
        painter.drawImage(drawRect, img);

        qDebug() << "Chart" << label << "drawn at rect:" << drawRect
                 << "from image size:" << img.size();
    };

    QString chartChoice = filters["chartType"].toString();

    // request chart images sized to the full usable area (so the chart is rendered at that resolution)
    // Before calling chart generation functions, calculate appropriate sizes

    if (chartChoice.contains("All", Qt::CaseInsensitive)) {
        QSize barSize(usableWidth, 600);  // Wide rectangle for bar charts
        QSize pieSize(700, 700);          // Square for pie charts
        QSize lineSize(usableWidth, 600); // Wide rectangle for line charts

        drawFullscreenChart("Bar Chart",  makeBarChartImage(data, barSize, palette));
        drawFullscreenChart("Pie Chart",  makePieChartImage(data, pieSize, palette));
        drawFullscreenChart("Line Chart", makeLineChartImage(data, lineSize, palette, info.openHour, info.closeHour));
    } else if (chartChoice.contains("Pie", Qt::CaseInsensitive)) {
        QSize pieSize(700, 700);  // Square dimensions
        drawFullscreenChart("Pie Chart", makePieChartImage(data, pieSize, palette));
    } else {
        // For bar and line charts, use rectangular size
        QSize rectSize(usableWidth, 600);
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

    y = margin + 100;
    drawHeader(y);

    painter.setFont(QFont("Arial", 11, QFont::Bold));
    painter.setPen(Qt::black);
    painter.drawText(QRect(margin, y, pageWidth - 2*margin, 25),
                     Qt::AlignCenter, QString("Prepared by: %1").arg(safeText(librarian)));
    y += 25;

    painter.setFont(QFont("Arial", 10));
    painter.drawText(QRect(margin, y, pageWidth - 2*margin, 20),
                     Qt::AlignCenter, safeText(position));
    y += 40;

    int sigWidth = 240;
    int sigX = (pageWidth - sigWidth) / 2;
    painter.drawLine(sigX, y, sigX + sigWidth, y);
    painter.drawText(QRect(sigX, y + 5, sigWidth, 20), Qt::AlignCenter, "(Signature)");
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
                                       const ReportHeaderInfo &info)
{
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

    // ===== TABLE ROWS =====
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
