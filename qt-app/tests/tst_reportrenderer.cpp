#include <QtTest>
#include <QFont>
#include <QFontMetrics>
#include <QJsonArray>
#include <QJsonObject>
#include <QColor>
#include <QPainter>
#include <QPdfWriter>
#include <QPixmap>
#include <QTemporaryDir>
#include <QFileInfo>

#include "reportanalytics.h"
#include "reportrenderer.h"
#include "xlsxdocument.h"

class TstReportRenderer : public QObject
{
    Q_OBJECT

private slots:
    void aggregateVisitsByCourse_sumsPerCourse();
    void aggregateVisitsByCourseHour_countsInWindow();
    void aggregateVisitsByCourseHour_excludesOutOfWindowAndInvalid();
    void makeBarChartImage_nonNullAtSize();
    void makePieChartImage_nonNullAtSize();
    void makeLineChartImage_nonNullAtSize();
    void chartImageSize_scalesWithWidthNotFixedHeight();
    void scaledPx_scalesFrom96DpiBaseline();
    void rowAdvanceClearsFontHeightAtDefaultPdfDpi();
    void paintReport_writesPdf();
    void writeReportToXlsx_populatesCells();
    void writeReportToXlsx_rosterRowsPresentOnlyWhenIncluded();
    void writeReportToXlsx_summarySheetHasKpisAndRankings();
    void writeReportToXlsx_rosterOnSeparateSheetWhenIncluded();
    void paintReport_writesPdfWithAndWithoutRoster();
    void paintReport_writesAnalyticsPdfAtHighDpi();
    void writeReportToXlsx_sanitizesFormulaLeadingNames();
    void writeReportToXlsx_timeBlock_dataStatePresent();
    void writeReportToXlsx_timeBlock_disabledStateAbsent();
    void writeReportToXlsx_timeBlock_emptyAndErrorNotesDiffer();
    void makeHourlyBarChartImage_nonBlankAtScreenSafeSize();
    void makeWeekdayBarChartImage_nonBlankAtScreenSafeSize();
    void circularLogoPixmap_squareCircularAndUndistorted();

private:
    static QJsonArray sampleVisits() {
        return QJsonArray{
            QJsonObject{{"course", "BSIT"}, {"visits", 3}, {"login_time", "08:15:00"}},
            QJsonObject{{"course", "BSIT"}, {"visits", 2}, {"login_time", "08:45:00"}},
            QJsonObject{{"course", "BSCS"}, {"visits", 5}, {"login_time", "10:00:00"}},
        };
    }

    // Richer row shape needed by paintReport / writeReportToXlsx: student-level
    // rows carrying school_id/name/gender/status/course/department/year_level/visits.
    static QJsonArray sampleRows() {
        return QJsonArray{
            QJsonObject{
                {"school_id", "2023-00001"}, {"name", "Test Student One"},
                {"gender", "Male"}, {"status", "Regular"},
                {"course", "BSIT"}, {"department", "College of Computing Studies"},
                {"year_level", "1st Year"}, {"visits", 3}
            },
            QJsonObject{
                {"school_id", "2023-00002"}, {"name", "Test Student Two"},
                {"gender", "Female"}, {"status", "Regular"},
                {"course", "BSCS"}, {"department", "College of Computing Studies"},
                {"year_level", "1st Year"}, {"visits", 5}
            },
        };
    }

    static ReportAnalytics sampleAnalytics() {
        return ReportAnalytics::compute(sampleRows());   // visits already numeric
    }

    static bool xlsxContainsAcrossSheets(QXlsx::Document &xlsx, const QString &needle) {
        const QStringList names = xlsx.sheetNames();
        for (const QString &n : names) {
            xlsx.selectSheet(n);
            for (int r = 1; r <= 40; ++r)
                for (int c = 1; c <= 8; ++c)
                    if (xlsx.read(r, c).toString() == needle) return true;
        }
        return false;
    }

    static QJsonObject sampleFilters() {
        return QJsonObject{
            {"start", "2023-01-01"}, {"end", "2023-01-31"},
            {"department", "College of Computing Studies"},
            {"course", "All"}, {"schoolYear", "2023-2024"}
        };
    }

    static ReportHeaderInfo sampleHeaderInfo() {
        ReportHeaderInfo info;
        info.schoolName = "Test University";
        info.address    = "Test Address";
        info.librarian  = "Test Librarian";
        info.position   = "Head Librarian";
        info.openHour   = 7;
        info.closeHour  = 21;
        return info;
    }

    static ReportPalette samplePalette() {
        return ReportPalette{
            QColor("#34495E"), Qt::white, QColor("#F4F6F7"), Qt::white, Qt::black,
            { QColor("#1f77b4"), QColor("#ff7f0e") }};
    }

    // A Data-state carrier with VM-formatted labels seeded directly (this core
    // test does not link the ViewModel). Peak hour 14 -> "2–3 PM"; Monday busiest.
    static ReportTimeExport sampleTimeExportData() {
        ReportTimeExport t;
        t.state = TimeAnalyticsExportState::Data;
        static const char *const hourTicks[24] = {
            "12A","1A","2A","3A","4A","5A","6A","7A","8A","9A","10A","11A",
            "12P","1P","2P","3P","4P","5P","6P","7P","8P","9P","10P","11P" };
        for (int h = 0; h < 24; ++h) {
            t.hourLabels << QString::fromLatin1(hourTicks[h]);
            t.hourCounts << (h == 14 ? 12 : (h == 9 ? 3 : 0));
        }
        static const char *const days[7] = { "Mon","Tue","Wed","Thu","Fri","Sat","Sun" };
        const int dayCounts[7] = { 40, 8, 30, 8, 5, 1, 2 };
        for (int d = 0; d < 7; ++d) {
            t.weekdayLabels << QString::fromLatin1(days[d]);
            t.weekdayCounts << dayCounts[d];
        }
        t.busiestHourLabel = QStringLiteral("2–3 PM");
        t.busiestDayLabel  = QStringLiteral("Monday");
        return t;
    }

    static bool imageHasNonWhitePixel(const QImage &img) {
        const QImage rgb = img.convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < rgb.height(); ++y)
            for (int x = 0; x < rgb.width(); ++x)
                if (rgb.pixelColor(x, y) != QColor(Qt::white)) return true;
        return false;
    }

    // Counts pixels approximately matching `target` (small per-channel tolerance
    // for antialiasing at bar edges). Used to distinguish "chart with real bars"
    // from "chart with only title/axis text" — both satisfy imageHasNonWhitePixel,
    // but only the former paints a substantial number of bar-brush-colored pixels.
    static int countBarColorPixels(const QImage &img, const QColor &target) {
        const QImage rgb = img.convertToFormat(QImage::Format_ARGB32);
        int count = 0;
        for (int y = 0; y < rgb.height(); ++y) {
            for (int x = 0; x < rgb.width(); ++x) {
                const QColor px = rgb.pixelColor(x, y);
                const int diff = qAbs(px.red() - target.red())
                                + qAbs(px.green() - target.green())
                                + qAbs(px.blue() - target.blue());
                if (diff < 24) ++count;
            }
        }
        return count;
    }
};

void TstReportRenderer::aggregateVisitsByCourse_sumsPerCourse() {
    const QMap<QString, int> got = ReportRenderer::aggregateVisitsByCourse(sampleVisits());
    QCOMPARE(got.value("BSIT"), 5);
    QCOMPARE(got.value("BSCS"), 5);
    QCOMPARE(got.size(), 2);
}

void TstReportRenderer::aggregateVisitsByCourseHour_countsInWindow() {
    const auto got = ReportRenderer::aggregateVisitsByCourseHour(sampleVisits(), 7, 21);
    QCOMPARE(got.value("BSIT").value(8), 2);  // two BSIT rows at hour 8
    QCOMPARE(got.value("BSCS").value(10), 1);
}

void TstReportRenderer::aggregateVisitsByCourseHour_excludesOutOfWindowAndInvalid() {
    QJsonArray data{
        QJsonObject{{"course", "BSIT"}, {"login_time", "05:00:00"}}, // before open (7)
        QJsonObject{{"course", "BSIT"}, {"login_time", "23:30:00"}}, // after close (21)
        QJsonObject{{"course", "BSIT"}, {"login_time", "not-a-time"}}, // invalid
        QJsonObject{{"course", "BSIT"}, {"login_time", "09:00:00"}}, // in window
    };
    const auto got = ReportRenderer::aggregateVisitsByCourseHour(data, 7, 21);
    QCOMPARE(got.value("BSIT").size(), 1);
    QCOMPARE(got.value("BSIT").value(9), 1);
}

void TstReportRenderer::makeBarChartImage_nonNullAtSize() {
    const ReportPalette pal = ReportPalette{
        QColor("#34495E"), Qt::white, QColor("#F4F6F7"), Qt::white, Qt::black,
        { QColor("#1f77b4"), QColor("#ff7f0e") }};
    const QImage img = ReportRenderer::makeBarChartImage(sampleVisits(), QSize(400, 300), pal);
    QVERIFY(!img.isNull());
    QCOMPARE(img.size(), QSize(400, 300));
}

void TstReportRenderer::makePieChartImage_nonNullAtSize() {
    const ReportPalette pal = ReportPalette{
        QColor("#34495E"), Qt::white, QColor("#F4F6F7"), Qt::white, Qt::black,
        { QColor("#1f77b4"), QColor("#ff7f0e") }};
    const QImage img = ReportRenderer::makePieChartImage(sampleVisits(), QSize(300, 300), pal);
    QVERIFY(!img.isNull());
    QCOMPARE(img.size(), QSize(300, 300));
}

void TstReportRenderer::makeLineChartImage_nonNullAtSize() {
    const ReportPalette pal = ReportPalette{
        QColor("#34495E"), Qt::white, QColor("#F4F6F7"), Qt::white, Qt::black,
        { QColor("#1f77b4"), QColor("#ff7f0e") }};
    const QImage img = ReportRenderer::makeLineChartImage(sampleVisits(), QSize(400, 300), pal, 7, 21);
    QVERIFY(!img.isNull());
    QCOMPARE(img.size(), QSize(400, 300));
}

// The sliver bug: chart heights were a fixed 600px literal while usableWidth is
// device-scaled (~9008px at 1200 DPI), yielding a ~15:1 strip that KeepAspectRatio
// shrank to a half-inch band. chartImageSize keys the height off usableWidth so the
// aspect ratio stays sane (5:3 landscape / square) at any DPI — no fixed 600.
void TstReportRenderer::chartImageSize_scalesWithWidthNotFixedHeight() {
    // Screen-adaptive: a QChartView is a QWidget clamped to the physical screen,
    // so the render size fits the available screen (never exceeds the base) and is
    // upscaled to the page later. usableWidth is ignored. Assert invariants that
    // hold regardless of the machine the test runs on.
    const QSize bar = ReportRenderer::chartImageSize(9000, false);
    const QSize pie = ReportRenderer::chartImageSize(9000, true);
    QVERIFY(!bar.isEmpty() && !pie.isEmpty());
    QVERIFY(bar.width() <= 1600 && bar.height() <= 1000);   // never exceeds the screen-safe base
    QVERIFY(pie.width() == pie.height());                   // pie is square
    const double aspect = double(bar.width()) / bar.height();
    QVERIFY(aspect > 1.2 && aspect < 2.0);                  // sane landscape aspect, no sliver
    QCOMPARE(ReportRenderer::chartImageSize(500, false), bar); // ignores usableWidth
}

// The overlap fix scales every legacy ~96-DPI pixel literal by resolution/96.
// This pins the arithmetic at the DPIs that matter: 96 (baseline, identity),
// 1200 (QPdfWriter default, where the bug bit), and 72 (down-scale).
void TstReportRenderer::scaledPx_scalesFrom96DpiBaseline() {
    QCOMPARE(ReportRenderer::scaledPx(20, 96), 20);
    QCOMPARE(ReportRenderer::scaledPx(20, 1200), 250);
    QCOMPARE(ReportRenderer::scaledPx(25, 1200), 313);   // qRound(25*12.5)=313
    QCOMPARE(ReportRenderer::scaledPx(20, 72), 15);
}

// Ties the helper to the metric that caused the overlap: at the QPdfWriter
// default 1200 DPI the legacy raw 20px row advance is far below the row font's
// glyph box, so rows print on top of each other. The scaled advance clears it.
void TstReportRenderer::rowAdvanceClearsFontHeightAtDefaultPdfDpi() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    {
        QPdfWriter pdf(dir.filePath("metrics.pdf"));
        pdf.setResolution(1200);
        QPainter p(&pdf);
        p.setFont(QFont("Arial", 10));            // the table row font
        const QFontMetrics fm = p.fontMetrics();
        QVERIFY(20 < fm.height());                                   // documents the legacy overlap: raw 20px < glyph box
        QVERIFY(ReportRenderer::scaledPx(20, 1200) >= fm.height());  // the fix clears the glyph box
        p.end();                                  // end painter before QPdfWriter is destroyed
    }
}

void TstReportRenderer::paintReport_writesPdf() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("report.pdf");

    {
        QPdfWriter pdf(path);
        pdf.setResolution(300);
        const bool ok = ReportRenderer::paintReport(&pdf, 300, sampleRows(), sampleFilters(),
                                                     samplePalette(), sampleHeaderInfo(),
                                                     sampleAnalytics(), true, ReportTimeExport{});
        QVERIFY(ok);
    } // QPdfWriter flushes/finalizes the file on destruction.

    QVERIFY(QFileInfo(path).size() > 0);
}

void TstReportRenderer::writeReportToXlsx_populatesCells() {
    QXlsx::Document xlsx;
    const bool ok = ReportRenderer::writeReportToXlsx(xlsx, sampleRows(), sampleFilters(),
                                                       sampleHeaderInfo(), sampleAnalytics(), true,
                                                       ReportTimeExport{});
    QVERIFY(ok);

    // Title cell (row 1, col 1) holds the school name.
    QCOMPARE(xlsx.read(1, 1).toString(), sampleHeaderInfo().schoolName);   // Summary title
    QVERIFY(xlsx.selectSheet("Detailed Roster"));

    // Smoke-check that real data rows landed: scan a small range below the
    // table header for one of our synthetic school IDs / names.
    bool foundSchoolId = false;
    bool foundName = false;
    for (int row = 1; row <= 15; ++row) {
        for (int col = 1; col <= 8; ++col) {
            const QString cell = xlsx.read(row, col).toString();
            if (cell == "2023-00001") foundSchoolId = true;
            if (cell == "Test Student One") foundName = true;
        }
    }
    QVERIFY(foundSchoolId);
    QVERIFY(foundName);
}

void TstReportRenderer::writeReportToXlsx_rosterRowsPresentOnlyWhenIncluded() {
    {   // includeRoster = false -> per-student roster rows absent everywhere
        QXlsx::Document xlsx;
        QVERIFY(ReportRenderer::writeReportToXlsx(
            xlsx, sampleRows(), sampleFilters(), sampleHeaderInfo(), sampleAnalytics(), false,
            ReportTimeExport{}));
        QVERIFY(!xlsxContainsAcrossSheets(xlsx, "2023-00001"));
    }
    {   // includeRoster = true -> the roster rows are present (single sheet now; a
        //  "Detailed Roster" sheet after Task 3 — the cross-sheet scan covers both)
        QXlsx::Document xlsx;
        QVERIFY(ReportRenderer::writeReportToXlsx(
            xlsx, sampleRows(), sampleFilters(), sampleHeaderInfo(), sampleAnalytics(), true,
            ReportTimeExport{}));
        QVERIFY(xlsxContainsAcrossSheets(xlsx, "2023-00001"));
    }
}

void TstReportRenderer::writeReportToXlsx_summarySheetHasKpisAndRankings() {
    QXlsx::Document xlsx;
    QVERIFY(ReportRenderer::writeReportToXlsx(
        xlsx, sampleRows(), sampleFilters(), sampleHeaderInfo(), sampleAnalytics(), false,
        ReportTimeExport{}));

    QVERIFY(xlsx.sheetNames().contains("Summary"));
    QVERIFY(xlsx.selectSheet("Summary"));

    // The Summary sheet carries KPI labels and at least one ranking heading.
    // sampleRows(): BSIT=3 + BSCS=5 -> total 8 visits, 2 unique students.
    bool foundTotalLabel = false, foundTotalValue = false, foundRankingHeading = false;
    for (int r = 1; r <= 60; ++r) {
        for (int c = 1; c <= 8; ++c) {
            const QString cell = xlsx.read(r, c).toString();
            if (cell.contains("Total Visits", Qt::CaseInsensitive)) foundTotalLabel = true;
            if (cell == "8") foundTotalValue = true;
            if (cell.contains("Top 10 Students", Qt::CaseInsensitive)) foundRankingHeading = true;
        }
    }
    QVERIFY(foundTotalLabel);
    QVERIFY(foundTotalValue);
    QVERIFY(foundRankingHeading);
}

void TstReportRenderer::writeReportToXlsx_rosterOnSeparateSheetWhenIncluded() {
    QXlsx::Document xlsx;
    QVERIFY(ReportRenderer::writeReportToXlsx(
        xlsx, sampleRows(), sampleFilters(), sampleHeaderInfo(), sampleAnalytics(), true,
        ReportTimeExport{}));

    QVERIFY(xlsx.sheetNames().contains("Detailed Roster"));
    QVERIFY(xlsx.selectSheet("Detailed Roster"));
    bool foundSchoolId = false;
    for (int r = 1; r <= 20; ++r)
        for (int c = 1; c <= 8; ++c)
            if (xlsx.read(r, c).toString() == "2023-00001") foundSchoolId = true;
    QVERIFY(foundSchoolId);
}

void TstReportRenderer::paintReport_writesPdfWithAndWithoutRoster() {
    for (bool includeRoster : { false, true }) {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("report.pdf");
        {
            QPdfWriter pdf(path);
            pdf.setResolution(300);
            QVERIFY(ReportRenderer::paintReport(
                &pdf, 300, sampleRows(), sampleFilters(), samplePalette(),
                sampleHeaderInfo(), sampleAnalytics(), includeRoster, ReportTimeExport{}));
        }
        QVERIFY(QFileInfo(path).size() > 0);
    }
}

void TstReportRenderer::paintReport_writesAnalyticsPdfAtHighDpi() {
    // Exercise the chart + roster combination explicitly — sampleFilters() sets
    // no "chartType", so a chart is only drawn when we add one. The chart+roster
    // path is exactly where the "roster overprints the last chart page" bug lives:
    // the roster MUST open its own page unconditionally (Task 4, item 6).
    QJsonObject filtersWithChart = sampleFilters();
    filtersWithChart["chartType"] = "Bar";
    for (int resolution : { 300, 1200 }) {
        for (bool includeRoster : { false, true }) {
            QTemporaryDir dir;
            QVERIFY(dir.isValid());
            const QString path = dir.filePath("analytics.pdf");
            {
                QPdfWriter pdf(path);
                pdf.setResolution(resolution);
                QVERIFY(ReportRenderer::paintReport(
                    &pdf, resolution, sampleRows(), filtersWithChart, samplePalette(),
                    sampleHeaderInfo(), sampleAnalytics(), includeRoster, ReportTimeExport{}));
            }
            QVERIFY(QFileInfo(path).size() > 0);
        }
    }
}

// Regression test for a claude-review finding: the vendored QXlsx
// Worksheet::write() treats any string starting with '=' as a live FORMULA,
// so a network-derived student name like =HYPERLINK("http://evil","x") would
// be written (and evaluated by Excel on open) as a formula rather than text
// (Excel formula/CSV injection). writeReportToXlsx must prefix such values
// with a literal apostrophe so they render as inert text.
void TstReportRenderer::writeReportToXlsx_sanitizesFormulaLeadingNames() {
    const QString payload = QStringLiteral("=HYPERLINK(\"http://evil\",\"x\")");
    QJsonArray rows{
        QJsonObject{
            {"school_id", "2023-00009"}, {"name", payload},
            {"gender", "Male"}, {"status", "Regular"},
            {"course", "BSIT"}, {"department", "College of Computing Studies"},
            {"year_level", "1st Year"}, {"visits", 1}
        },
        QJsonObject{
            {"school_id", "2023-00010"}, {"name", "Test Student One"},
            {"gender", "Female"}, {"status", "Regular"},
            {"course", "BSCS"}, {"department", "College of Computing Studies"},
            {"year_level", "1st Year"}, {"visits", 1}
        },
    };
    ReportAnalytics analytics = ReportAnalytics::compute(rows);

    QXlsx::Document xlsx;
    QVERIFY(ReportRenderer::writeReportToXlsx(
        xlsx, rows, sampleFilters(), sampleHeaderInfo(), analytics, true, ReportTimeExport{}));

    QVERIFY(xlsx.selectSheet("Detailed Roster"));

    bool foundSanitizedPayload = false;
    bool foundUnchangedNormalName = false;
    for (int r = 1; r <= 20; ++r) {
        for (int c = 1; c <= 8; ++c) {
            const QString cell = xlsx.read(r, c).toString();
            if (cell == QLatin1Char('\'') + payload) {
                QVERIFY(cell.startsWith(QLatin1Char('\'')));
                foundSanitizedPayload = true;
            }
            // The raw, unescaped payload must never appear as a stored cell value.
            QVERIFY(cell != payload);
            if (cell == "Test Student One") foundUnchangedNormalName = true;
        }
    }
    QVERIFY(foundSanitizedPayload);
    QVERIFY(foundUnchangedNormalName);
}

void TstReportRenderer::writeReportToXlsx_timeBlock_dataStatePresent() {
    QXlsx::Document xlsx;
    QVERIFY(ReportRenderer::writeReportToXlsx(
        xlsx, sampleRows(), sampleFilters(), sampleHeaderInfo(),
        sampleAnalytics(), false, sampleTimeExportData()));
    QVERIFY(xlsx.selectSheet("Summary"));

    bool foundTitle = false, foundPeakHour = false, foundBusiestDay = false;
    bool foundHourHdr = false, foundCountHdr = false, foundDayHdr = false;
    bool foundHourCell = false, foundDayCell = false;
    for (int r = 1; r <= 80; ++r) {
        for (int c = 1; c <= 8; ++c) {
            const QString cell = xlsx.read(r, c).toString();
            if (cell == "When do students visit?") foundTitle = true;
            if (cell == "Peak Hour: 2–3 PM") foundPeakHour = true;
            if (cell == "Busiest Day: Monday") foundBusiestDay = true;
            if (cell == "Hour") foundHourHdr = true;
            if (cell == "Count") foundCountHdr = true;
            if (cell == "Day") foundDayHdr = true;
            if (cell == "2P") foundHourCell = true;    // hourLabels[14], hourly col
            if (cell == "Mon") foundDayCell = true;    // weekdayLabels[0], weekday col
        }
    }
    QVERIFY(foundTitle);
    QVERIFY(foundPeakHour);
    QVERIFY(foundBusiestDay);
    QVERIFY(foundHourHdr);
    QVERIFY(foundCountHdr);
    QVERIFY(foundDayHdr);
    QVERIFY(foundHourCell);
    QVERIFY(foundDayCell);
}

void TstReportRenderer::writeReportToXlsx_timeBlock_disabledStateAbsent() {
    QXlsx::Document xlsx;
    QVERIFY(ReportRenderer::writeReportToXlsx(
        xlsx, sampleRows(), sampleFilters(), sampleHeaderInfo(),
        sampleAnalytics(), false, ReportTimeExport{}));   // default = Disabled
    QVERIFY(xlsx.selectSheet("Summary"));
    for (int r = 1; r <= 80; ++r)
        for (int c = 1; c <= 8; ++c)
            QVERIFY(xlsx.read(r, c).toString() != "When do students visit?");
}

void TstReportRenderer::writeReportToXlsx_timeBlock_emptyAndErrorNotesDiffer() {
    ReportTimeExport empty;  empty.state = TimeAnalyticsExportState::Empty;
    ReportTimeExport err;    err.state   = TimeAnalyticsExportState::Error;

    auto findNote = [](QXlsx::Document &x, const QString &needle) {
        x.selectSheet("Summary");
        for (int r = 1; r <= 80; ++r)
            for (int c = 1; c <= 8; ++c)
                if (x.read(r, c).toString() == needle) return true;
        return false;
    };

    QXlsx::Document xe;
    QVERIFY(ReportRenderer::writeReportToXlsx(
        xe, sampleRows(), sampleFilters(), sampleHeaderInfo(), sampleAnalytics(), false, empty));
    QVERIFY(findNote(xe, "No visit activity in this range"));
    QVERIFY(!findNote(xe, "Visit-time data could not be loaded"));
    QVERIFY(!findNote(xe, "Hour"));   // no table headers in the Empty state

    QXlsx::Document xr;
    QVERIFY(ReportRenderer::writeReportToXlsx(
        xr, sampleRows(), sampleFilters(), sampleHeaderInfo(), sampleAnalytics(), false, err));
    QVERIFY(findNote(xr, "Visit-time data could not be loaded"));
    QVERIFY(!findNote(xr, "No visit activity in this range"));
}

void TstReportRenderer::makeHourlyBarChartImage_nonBlankAtScreenSafeSize() {
    const QSize sz = ReportRenderer::chartImageSize(9000, false);   // screen-safe size
    const QImage img = ReportRenderer::makeHourlyBarChartImage(
        sampleTimeExportData(), sz, samplePalette());
    QVERIFY(!img.isNull());
    QCOMPARE(img.size(), sz);                    // MUST render at the screen-safe size
    QVERIFY(imageHasNonWhitePixel(img));         // real bars, not a blank raster

    // imageHasNonWhitePixel alone is satisfied by title/axis text even when the
    // bars themselves never draw (QBarCategoryAxis duplicate-empty-category bug).
    // Require a substantial count of actual bar-brush-colored pixels.
    const int barPixels = countBarColorPixels(img, samplePalette().chartColors.first());
    qDebug() << "hourly bar-color pixel count:" << barPixels;
    QVERIFY2(barPixels > 500, qPrintable(QString("expected >500 bar-color pixels, got %1").arg(barPixels)));
}

void TstReportRenderer::makeWeekdayBarChartImage_nonBlankAtScreenSafeSize() {
    const QSize sz = ReportRenderer::chartImageSize(9000, false);
    const QImage img = ReportRenderer::makeWeekdayBarChartImage(
        sampleTimeExportData(), sz, samplePalette());
    QVERIFY(!img.isNull());
    QCOMPARE(img.size(), sz);
    QVERIFY(imageHasNonWhitePixel(img));

    const int barPixels = countBarColorPixels(img, samplePalette().chartColors.first());
    qDebug() << "weekday bar-color pixel count:" << barPixels;
    QVERIFY2(barPixels > 500, qPrintable(QString("expected >500 bar-color pixels, got %1").arg(barPixels)));
}

// Regression: the report-header logo drew a KeepAspectRatio (non-square) pixmap
// stretched into a forced-square rect → vertical oblong. circularLogoPixmap must
// return a SQUARE, circle-clipped pixmap whose non-square source is expanded to
// fully cover the box (no letterbox gaps), matching the on-screen LLogoCircle.
void TstReportRenderer::circularLogoPixmap_squareCircularAndUndistorted()
{
    QPixmap src(200, 80);            // deliberately non-square (2.5:1)
    src.fill(Qt::red);               // fully opaque
    const QPixmap out = ReportRenderer::circularLogoPixmap(src, 64);

    // (A) Output is square — directly kills the oblong symptom.
    QCOMPARE(out.size(), QSize(64, 64));

    const QImage im = out.toImage().convertToFormat(QImage::Format_ARGB32);

    // (B) Circular clip: the four corners are transparent, the center opaque.
    QCOMPARE(qAlpha(im.pixel(0, 0)),   0);
    QCOMPARE(qAlpha(im.pixel(63, 0)),  0);
    QCOMPARE(qAlpha(im.pixel(0, 63)),  0);
    QCOMPARE(qAlpha(im.pixel(63, 63)), 0);
    QCOMPARE(qAlpha(im.pixel(32, 32)), 255);

    // (C) Expand-to-cover (KeepAspectRatioByExpanding), not letterbox: the
    // top- and bottom-center pixels just inside the circle are covered (opaque).
    // A KeepAspectRatio scale of a 2.5:1 source would leave these bands transparent.
    QCOMPARE(qAlpha(im.pixel(32, 4)),  255);
    QCOMPARE(qAlpha(im.pixel(32, 59)), 255);
}

QTEST_MAIN(TstReportRenderer)
#include "tst_reportrenderer.moc"
