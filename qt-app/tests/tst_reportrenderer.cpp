#include <QtTest>
#include <QJsonArray>
#include <QJsonObject>

#include "reportrenderer.h"

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

private:
    static QJsonArray sampleVisits() {
        return QJsonArray{
            QJsonObject{{"course", "BSIT"}, {"visits", 3}, {"login_time", "08:15:00"}},
            QJsonObject{{"course", "BSIT"}, {"visits", 2}, {"login_time", "08:45:00"}},
            QJsonObject{{"course", "BSCS"}, {"visits", 5}, {"login_time", "10:00:00"}},
        };
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

QTEST_MAIN(TstReportRenderer)
#include "tst_reportrenderer.moc"
