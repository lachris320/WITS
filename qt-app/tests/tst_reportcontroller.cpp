#include <QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>

#include "reportcontroller.h"

class TstReportController : public QObject
{
    Q_OBJECT

private slots:
    // ---- getPalette ----
    void getPalette_blue_hasHeaderAnd8Colors();
    void getPalette_green_hasHeaderAnd8Colors();
    void getPalette_red_hasHeaderAnd8Colors();
    void getPalette_unknown_returnsDefault10Colors();

    // ---- parsers ----
    void parseDepartments_success_returnsList();
    void parseDepartments_notSuccess_returnsEmpty();
    void parseYears_success_intsToStrings();
    void parseYears_notSuccess_returnsEmpty();
    void parseCourses_success_returnsList();
    void parseCourses_notSuccess_returnsEmpty();
    void parseReportData_success_fillsData();
    void parseReportData_notSuccess_setsMessage();
    void parseReportData_nonObject_invalid();
    void parsePreviewData_success_returnsArray();
    void parsePreviewData_error_returnsEmpty();

    // ---- parseTimeAnalytics (get_report_time_data.php) ----
    void parseTimeAnalytics_valid_fillsArrays();
    void parseTimeAnalytics_wrongLengthHour_fails();
    void parseTimeAnalytics_wrongLengthWeekday_fails();
    void parseTimeAnalytics_missingField_fails();
    void parseTimeAnalytics_nonNumeric_fails();
    void parseTimeAnalytics_stringEncodedCounts_ok();
    void parseTimeAnalytics_statusError_failsWithMessage();

    // ---- computeDateRange ----
    void computeDateRange_day_valid();
    void computeDateRange_day_invalid();
    void computeDateRange_month_firstToLastDay();
    void computeDateRange_semesterFirst_janToJun();
    void computeDateRange_semesterSecond_julToDec();
    void computeDateRange_customValid();
    void computeDateRange_customReversed_invalid();
    void computeDateRange_outOfRange_invalid();

private:
    static QByteArray obj(const QJsonObject &o) {
        return QJsonDocument(o).toJson(QJsonDocument::Compact);
    }
    static QJsonArray numArray(int n, int fill = 0) {
        QJsonArray a; for (int i = 0; i < n; ++i) a.append(fill); return a;
    }
    static QByteArray timeObj(const QJsonArray &byHour, const QJsonArray &byWeekday,
                              const QString &status = QStringLiteral("success")) {
        QJsonObject o{{"status", status}, {"byHour", byHour}, {"byWeekday", byWeekday}};
        return QJsonDocument(o).toJson(QJsonDocument::Compact);
    }
};

void TstReportController::getPalette_blue_hasHeaderAnd8Colors() {
    const ReportPalette p = ReportController::getPalette("Blue");
    QCOMPARE(p.headerBg, QColor("#2E86C1"));
    QCOMPARE(p.chartColors.size(), 8);
}
void TstReportController::getPalette_green_hasHeaderAnd8Colors() {
    const ReportPalette p = ReportController::getPalette("Green");
    QCOMPARE(p.headerBg, QColor("#27AE60"));
    QCOMPARE(p.chartColors.size(), 8);
}
void TstReportController::getPalette_red_hasHeaderAnd8Colors() {
    const ReportPalette p = ReportController::getPalette("Red");
    QCOMPARE(p.headerBg, QColor("#C0392B"));
    QCOMPARE(p.chartColors.size(), 8);
}
void TstReportController::getPalette_unknown_returnsDefault10Colors() {
    const ReportPalette p = ReportController::getPalette("Rainbow");
    QCOMPARE(p.headerBg, QColor("#34495E"));
    QCOMPARE(p.chartColors.size(), 10);
}

void TstReportController::parseDepartments_success_returnsList() {
    QJsonObject o{{"status", "success"},
                  {"departments", QJsonArray{"College of Computing Studies",
                                             "College of Engineering"}}};
    const QStringList got = ReportController::parseDepartments(obj(o));
    QCOMPARE(got, (QStringList{"College of Computing Studies", "College of Engineering"}));
}
void TstReportController::parseDepartments_notSuccess_returnsEmpty() {
    QJsonObject o{{"status", "error"}, {"message", "nope"}};
    QVERIFY(ReportController::parseDepartments(obj(o)).isEmpty());
}
void TstReportController::parseYears_success_intsToStrings() {
    QJsonObject o{{"status", "success"}, {"years", QJsonArray{2023, 2024}}};
    QCOMPARE(ReportController::parseYears(obj(o)), (QStringList{"2023", "2024"}));
}
void TstReportController::parseYears_notSuccess_returnsEmpty() {
    QJsonObject o{{"status", "error"}};
    QVERIFY(ReportController::parseYears(obj(o)).isEmpty());
}
void TstReportController::parseCourses_success_returnsList() {
    QJsonObject o{{"status", "success"}, {"courses", QJsonArray{"All", "BSIT", "BSCS"}}};
    QCOMPARE(ReportController::parseCourses(obj(o)), (QStringList{"All", "BSIT", "BSCS"}));
}
void TstReportController::parseCourses_notSuccess_returnsEmpty() {
    QJsonObject o{{"status", "error"}};
    QVERIFY(ReportController::parseCourses(obj(o)).isEmpty());
}
void TstReportController::parseReportData_success_fillsData() {
    QJsonObject o{{"status", "success"},
                  {"data", QJsonArray{QJsonObject{{"course", "BSIT"}, {"visits", 3}}}}};
    QJsonArray data; QString msg;
    QCOMPARE(ReportController::parseReportData(obj(o), data, msg),
             ReportDataOutcome::Success);
    QCOMPARE(data.size(), 1);
    QVERIFY(msg.isEmpty());
}
void TstReportController::parseReportData_notSuccess_setsMessage() {
    QJsonObject o{{"status", "error"}, {"message", "bad filters"}};
    QJsonArray data; QString msg;
    QCOMPARE(ReportController::parseReportData(obj(o), data, msg),
             ReportDataOutcome::NotSuccess);
    QCOMPARE(msg, QString("bad filters"));
}
void TstReportController::parseReportData_nonObject_invalid() {
    QJsonArray data; QString msg;
    QCOMPARE(ReportController::parseReportData(QByteArray("not json"), data, msg),
             ReportDataOutcome::InvalidResponse);
}
void TstReportController::parsePreviewData_success_returnsArray() {
    QJsonObject o{{"status", "success"},
                  {"data", QJsonArray{QJsonObject{{"course", "BSIT"}}}}};
    QCOMPARE(ReportController::parsePreviewData(obj(o)).size(), 1);
}
void TstReportController::parsePreviewData_error_returnsEmpty() {
    QJsonObject o{{"status", "error"}};
    QVERIFY(ReportController::parsePreviewData(obj(o)).isEmpty());
}

void TstReportController::parseTimeAnalytics_valid_fillsArrays() {
    QJsonArray hours = numArray(24); hours[9] = 5; hours[14] = 12;
    QJsonArray week  = numArray(7);  week[1] = 40;      // Monday (Sun-first idx 1)
    QList<int> outH, outW; QString err;
    QVERIFY(ReportController::parseTimeAnalytics(timeObj(hours, week), outH, outW, err));
    QCOMPARE(outH.size(), 24);
    QCOMPARE(outW.size(), 7);
    QCOMPARE(outH.at(14), 12);
    QCOMPARE(outW.at(1), 40);
    QVERIFY(err.isEmpty());
}

void TstReportController::parseTimeAnalytics_wrongLengthHour_fails() {
    QList<int> outH, outW; QString err;
    QVERIFY(!ReportController::parseTimeAnalytics(
        timeObj(numArray(23), numArray(7)), outH, outW, err));
    QVERIFY(!err.isEmpty());
    QVERIFY(outH.isEmpty());     // nothing malformed handed downstream
    QVERIFY(outW.isEmpty());
}

void TstReportController::parseTimeAnalytics_wrongLengthWeekday_fails() {
    QList<int> outH, outW; QString err;
    QVERIFY(!ReportController::parseTimeAnalytics(
        timeObj(numArray(24), numArray(8)), outH, outW, err));
    QVERIFY(!err.isEmpty());
}

void TstReportController::parseTimeAnalytics_missingField_fails() {
    QJsonObject o{{"status", "success"}, {"byHour", numArray(24)}};   // no byWeekday
    QList<int> outH, outW; QString err;
    QVERIFY(!ReportController::parseTimeAnalytics(
        QJsonDocument(o).toJson(QJsonDocument::Compact), outH, outW, err));
}

void TstReportController::parseTimeAnalytics_nonNumeric_fails() {
    QJsonArray hours = numArray(24); hours[3] = "abc";               // non-numeric string
    QList<int> outH, outW; QString err;
    QVERIFY(!ReportController::parseTimeAnalytics(
        timeObj(hours, numArray(7)), outH, outW, err));
}

void TstReportController::parseTimeAnalytics_stringEncodedCounts_ok() {
    QJsonArray hours = numArray(24); hours[5] = "8";                 // numeric STRING
    QJsonArray week  = numArray(7);  week[2] = "15";
    QList<int> outH, outW; QString err;
    QVERIFY(ReportController::parseTimeAnalytics(timeObj(hours, week), outH, outW, err));
    QCOMPARE(outH.at(5), 8);
    QCOMPARE(outW.at(2), 15);
}

void TstReportController::parseTimeAnalytics_statusError_failsWithMessage() {
    QJsonObject o{{"status", "error"}, {"message", "boom"}};
    QList<int> outH, outW; QString err;
    QVERIFY(!ReportController::parseTimeAnalytics(
        QJsonDocument(o).toJson(QJsonDocument::Compact), outH, outW, err));
    QCOMPARE(err, QStringLiteral("boom"));
}

void TstReportController::computeDateRange_day_valid() {
    const DateRange r = ReportController::computeDateRange(
        0, QDate(2023, 5, 17), 0, 0, QString(), 0, QDate(), QDate());
    QVERIFY(r.valid);
    QCOMPARE(r.start, QString("2023-05-17"));
    QCOMPARE(r.end,   QString("2023-05-17"));
}
void TstReportController::computeDateRange_day_invalid() {
    const DateRange r = ReportController::computeDateRange(
        0, QDate(), 0, 0, QString(), 0, QDate(), QDate());
    QVERIFY(!r.valid);
}
void TstReportController::computeDateRange_month_firstToLastDay() {
    const DateRange r = ReportController::computeDateRange(
        1, QDate(), 2 /*Feb*/, 2024, QString(), 0, QDate(), QDate());
    QVERIFY(r.valid);
    QCOMPARE(r.start, QString("2024-02-01"));
    QCOMPARE(r.end,   QString("2024-02-29")); // 2024 is a leap year
}
void TstReportController::computeDateRange_semesterFirst_janToJun() {
    const DateRange r = ReportController::computeDateRange(
        2, QDate(), 0, 0, "1st Semester", 2023, QDate(), QDate());
    QVERIFY(r.valid);
    QCOMPARE(r.start, QString("2023-01-01"));
    QCOMPARE(r.end,   QString("2023-06-30"));
}
void TstReportController::computeDateRange_semesterSecond_julToDec() {
    const DateRange r = ReportController::computeDateRange(
        2, QDate(), 0, 0, "2nd Semester", 2023, QDate(), QDate());
    QVERIFY(r.valid);
    QCOMPARE(r.start, QString("2023-07-01"));
    QCOMPARE(r.end,   QString("2023-12-31"));
}
void TstReportController::computeDateRange_customValid() {
    const DateRange r = ReportController::computeDateRange(
        3, QDate(), 0, 0, QString(), 0, QDate(2023, 1, 10), QDate(2023, 3, 20));
    QVERIFY(r.valid);
    QCOMPARE(r.start, QString("2023-01-10"));
    QCOMPARE(r.end,   QString("2023-03-20"));
}
void TstReportController::computeDateRange_customReversed_invalid() {
    const DateRange r = ReportController::computeDateRange(
        3, QDate(), 0, 0, QString(), 0, QDate(2023, 3, 20), QDate(2023, 1, 10));
    QVERIFY(!r.valid);
}
void TstReportController::computeDateRange_outOfRange_invalid() {
    const DateRange r = ReportController::computeDateRange(
        9, QDate(2023, 5, 17), 0, 0, QString(), 0, QDate(), QDate());
    QVERIFY(!r.valid);
}

QTEST_GUILESS_MAIN(TstReportController)
#include "tst_reportcontroller.moc"
