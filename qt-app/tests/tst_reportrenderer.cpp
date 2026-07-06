#include <QtTest>
#include <QJsonArray>
#include <QJsonObject>
#include <QPdfWriter>
#include <QTemporaryDir>
#include <QFileInfo>

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
    void paintReport_writesPdf();
    void writeReportToXlsx_populatesCells();

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

void TstReportRenderer::paintReport_writesPdf() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("report.pdf");

    {
        QPdfWriter pdf(path);
        pdf.setResolution(300);
        const bool ok = ReportRenderer::paintReport(&pdf, 300, sampleRows(), sampleFilters(),
                                                     samplePalette(), sampleHeaderInfo());
        QVERIFY(ok);
    } // QPdfWriter flushes/finalizes the file on destruction.

    QVERIFY(QFileInfo(path).size() > 0);
}

void TstReportRenderer::writeReportToXlsx_populatesCells() {
    QXlsx::Document xlsx;
    const bool ok = ReportRenderer::writeReportToXlsx(xlsx, sampleRows(), sampleFilters(),
                                                       sampleHeaderInfo());
    QVERIFY(ok);

    // Title cell (row 1, col 1) holds the school name.
    QCOMPARE(xlsx.read(1, 1).toString(), sampleHeaderInfo().schoolName);

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

QTEST_MAIN(TstReportRenderer)
#include "tst_reportrenderer.moc"
