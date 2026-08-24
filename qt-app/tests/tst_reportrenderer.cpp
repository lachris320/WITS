#include <QtTest>
#include <QFont>
#include <QFontMetrics>
#include <QJsonArray>
#include <QJsonObject>
#include <QPainter>
#include <QPdfWriter>
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
                                                     sampleAnalytics(), true);
        QVERIFY(ok);
    } // QPdfWriter flushes/finalizes the file on destruction.

    QVERIFY(QFileInfo(path).size() > 0);
}

void TstReportRenderer::writeReportToXlsx_populatesCells() {
    QXlsx::Document xlsx;
    const bool ok = ReportRenderer::writeReportToXlsx(xlsx, sampleRows(), sampleFilters(),
                                                       sampleHeaderInfo(), sampleAnalytics(), true);
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
            xlsx, sampleRows(), sampleFilters(), sampleHeaderInfo(), sampleAnalytics(), false));
        QVERIFY(!xlsxContainsAcrossSheets(xlsx, "2023-00001"));
    }
    {   // includeRoster = true -> the roster rows are present (single sheet now; a
        //  "Detailed Roster" sheet after Task 3 — the cross-sheet scan covers both)
        QXlsx::Document xlsx;
        QVERIFY(ReportRenderer::writeReportToXlsx(
            xlsx, sampleRows(), sampleFilters(), sampleHeaderInfo(), sampleAnalytics(), true));
        QVERIFY(xlsxContainsAcrossSheets(xlsx, "2023-00001"));
    }
}

void TstReportRenderer::writeReportToXlsx_summarySheetHasKpisAndRankings() {
    QXlsx::Document xlsx;
    QVERIFY(ReportRenderer::writeReportToXlsx(
        xlsx, sampleRows(), sampleFilters(), sampleHeaderInfo(), sampleAnalytics(), false));

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
        xlsx, sampleRows(), sampleFilters(), sampleHeaderInfo(), sampleAnalytics(), true));

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
                sampleHeaderInfo(), sampleAnalytics(), includeRoster));
        }
        QVERIFY(QFileInfo(path).size() > 0);
    }
}

QTEST_MAIN(TstReportRenderer)
#include "tst_reportrenderer.moc"
