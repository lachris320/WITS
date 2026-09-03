#include <QtTest>
#include <QDate>
#include <QJsonObject>
#include <QJsonDocument>
#include <QPageLayout>
#include <QSignalSpy>
#include <QUrl>
#include <QTemporaryDir>
#include <QFileInfo>
#include "xlsxdocument.h"
#include "ReportingViewModel.h"

class TestReportingViewModel : public QObject
{
    Q_OBJECT
private slots:
    void buildFiltersDaySendsStringTypeAndRange();
    void buildFiltersMonthSendsRange();
    void buildFiltersSemesterSendsComponentsNotRange();
    void buildFiltersCustomSendsRange();
    void buildFiltersPassesDeptAndCourse();
    void aggregateSumsAndRanksByCourse();
    void aggregateEmptyIsEmpty();
    void deriveTilesComputesTotals();
    void deriveTilesEmptyIsZeroAndDash();
    void canGenerateWithoutDepartmentWhenDurationValid();
    void canGenerateDayRequiresValidDate();
    void canGenerateMonthRequiresMonthAndYear();
    void canGenerateSemesterRequiresSemesterAndYear();
    void canGenerateCustomRequiresOrderedRange();
    void settersEmitAndUpdateCanGenerate();
    void onDepartmentsLoadedPopulates();
    void onYearsLoadedPopulates();
    void onCoursesLoadedPopulates();
    void onCoursesLoadedFiltersServerAllEntry();
    void setDepartmentClearsCourse();
    void onReportDataReadyPopulatesPreview();
    void onReportDataReadyEmptyIsSuccessNotError();
    void analytics_populatedFromResult();
    void onReportErrorSetsErrorClearsLoading();
    void generateWhileLoadingIsNoop();
    void generateReportWithoutDepartmentShowsValidationMessage();
    void generateReportWithIncompleteDurationShowsValidationMessage();
    void filtersCompleteTracksDurationOnly();
    void validationMessageClearsWhenFiltersComplete();
    void realFetchErrorNotAutoClearedByFilterChange();
    void buildFiltersAllowsEmptyDepartment();
    void normalizeExportRowsCoercesVisitsToNumber();
    void semesterWindowMatchesServerRanges();
    void buildExportFiltersDayHasRangeAndLabels();
    void buildExportFiltersSemesterUsesServerWindow();
    void buildExportFiltersMonthSchoolYear();
    void canExportTruthTable();
    void paletteAndChartTypeSettersEmit();
    void orientation_defaultsToPortrait();
    void setOrientation_validValueEmitsAndUpdates();
    void setOrientation_blankRejectedNoSignal();
    void setOrientation_garbageRejectedNoSignal();
    void setOrientation_sameValueNoSignal();
    void orientations_containsPortraitAndLandscape();
    void pageOrientation_mapsPortraitAndLandscape();
    void pageOrientation_mapsBlankAndGarbageToPortrait();
    void applyResultStoresNormalizedExportRows();
    void failedRefetchDisablesExportAndClearsRows();
    void exportPdfWritesFile();
    void exportExcelWritesReadableCell();
    void exportPdfEmptyRowsShowsNoDataError();
    void exportPdfInvalidUrlShowsError();
    void exportWhileExportingIsNoop();
    void printReportEmptyRowsShowsNoDataError();
    void includeRosterInExport_defaultsFalse();
    void setIncludeRosterInExport_togglesAndSignals();
    void timeSection_propertyDefaults();
    void generate_operationFinalizesOnlyWhenBothSettle();
    void generate_operationFinalizesRegardlessOfSettleOrder();
    void generate_rowsLoadingClearsAtRowsSettleIndependently();
    void outcome_rowsSuccessTimeError_reportRendersTimeErrorLocalized();
    void outcome_rowsErrorTimeSuccess_primaryErrorFires();
    void canExport_unaffectedByTimeOutcome();
    void resetAtGenerate_clearsStaleTimeState();
    void timeModels_windowedEveryHourLabeled();
    void captions_formattedForKnownPeaks();
    void hasTimeData_falseOnAllZeroShowsEmptyState();
    void buildTimeExport_dataState_populatesLabelsCountsPeaks();
    void buildTimeExport_emptyState_listsEmpty();
    void buildTimeExport_errorState_winsOverData();
    void buildTimeExport_defensiveWrongLength_degradesToEmpty();
    void windowedCaption_followsInWindowPeakNotOverall();
    void allOutOfHours_hourCaptionEmptyWeekdayStillShown();

private:
    static QList<int> denseHours() {          // valid 24-array, peak at 14 (2 PM)
        QList<int> v; v.reserve(24);
        for (int i = 0; i < 24; ++i) v.append(0);
        v[9] = 3; v[14] = 12;
        return v;
    }
    static QList<int> denseWeek() {            // Sun-first; Monday busiest (idx1=40)
        return QList<int>{2, 40, 8, 30, 8, 5, 1};
    }
    static QList<int> zeros(int n) {
        QList<int> v; v.reserve(n);
        for (int i = 0; i < n; ++i) v.append(0);
        return v;
    }
};

void TestReportingViewModel::buildFiltersDaySendsStringTypeAndRange()
{
    const QJsonObject f = ReportingViewModel::buildFilters(
        "CE", "", 0, QDate(2026, 8, 14), 0, 0, "", 0, QDate(), QDate());
    QCOMPARE(f.value("durationType").toString(), QStringLiteral("day"));
    QCOMPARE(f.value("start").toString(), QStringLiteral("2026-08-14"));
    QCOMPARE(f.value("end").toString(), QStringLiteral("2026-08-14"));
}

void TestReportingViewModel::buildFiltersMonthSendsRange()
{
    const QJsonObject f = ReportingViewModel::buildFilters(
        "CE", "", 1, QDate(), 2, 2026, "", 0, QDate(), QDate());
    QCOMPARE(f.value("durationType").toString(), QStringLiteral("month"));
    QCOMPARE(f.value("start").toString(), QStringLiteral("2026-02-01"));
    QCOMPARE(f.value("end").toString(), QStringLiteral("2026-02-28"));
}

void TestReportingViewModel::buildFiltersSemesterSendsComponentsNotRange()
{
    const QJsonObject f = ReportingViewModel::buildFilters(
        "CE", "", 2, QDate(), 0, 0, "First Semester", 2026, QDate(), QDate());
    QCOMPARE(f.value("durationType").toString(), QStringLiteral("semester"));
    QCOMPARE(f.value("year").toInt(), 2026);
    QCOMPARE(f.value("semester").toString(), QStringLiteral("First Semester"));
    // Semester ranging is server-side: no client start/end sent.
    QVERIFY(!f.contains("start"));
    QVERIFY(!f.contains("end"));
}

void TestReportingViewModel::buildFiltersCustomSendsRange()
{
    const QJsonObject f = ReportingViewModel::buildFilters(
        "CE", "", 3, QDate(), 0, 0, "", 0, QDate(2026, 1, 1), QDate(2026, 3, 31));
    QCOMPARE(f.value("durationType").toString(), QStringLiteral("custom"));
    QCOMPARE(f.value("start").toString(), QStringLiteral("2026-01-01"));
    QCOMPARE(f.value("end").toString(), QStringLiteral("2026-03-31"));
}

void TestReportingViewModel::buildFiltersPassesDeptAndCourse()
{
    const QJsonObject f = ReportingViewModel::buildFilters(
        "CE", "BSCE", 0, QDate(2026, 8, 14), 0, 0, "", 0, QDate(), QDate());
    QCOMPARE(f.value("department").toString(), QStringLiteral("CE"));
    QCOMPARE(f.value("course").toString(), QStringLiteral("BSCE"));
}

void TestReportingViewModel::aggregateSumsAndRanksByCourse()
{
    const QJsonArray data = QJsonDocument::fromJson(R"([
        {"course":"BSIT","visits":"3"},
        {"course":"BSCE","visits":"10"},
        {"course":"BSIT","visits":"4"},
        {"course":"BSCE","visits":"5"}
    ])").array();
    const QList<BarsModel::Bar> bars = ReportingViewModel::aggregateVisitsByCourse(data);
    QCOMPARE(bars.size(), 2);
    // Ranked descending by total: BSCE=15 then BSIT=7.
    QCOMPARE(bars.at(0).label, QStringLiteral("BSCE"));
    QCOMPARE(bars.at(0).value, 15.0);
    QCOMPARE(bars.at(1).label, QStringLiteral("BSIT"));
    QCOMPARE(bars.at(1).value, 7.0);
}

void TestReportingViewModel::aggregateEmptyIsEmpty()
{
    QCOMPARE(ReportingViewModel::aggregateVisitsByCourse(QJsonArray()).size(), 0);
}

void TestReportingViewModel::deriveTilesComputesTotals()
{
    const QJsonArray data = QJsonDocument::fromJson(R"([
        {"course":"BSIT","visits":"3"},
        {"course":"BSCE","visits":"10"},
        {"course":"BSCE","visits":"5"}
    ])").array();
    const ReportingViewModel::Tiles t = ReportingViewModel::deriveTiles(data);
    QCOMPARE(t.totalVisits, 18);
    QCOMPARE(t.studentsShown, 3);
    QCOMPARE(t.topCourse, QStringLiteral("BSCE"));   // 15 > 3
}

void TestReportingViewModel::deriveTilesEmptyIsZeroAndDash()
{
    const ReportingViewModel::Tiles t = ReportingViewModel::deriveTiles(QJsonArray());
    QCOMPARE(t.totalVisits, 0);
    QCOMPARE(t.studentsShown, 0);
    QCOMPARE(t.topCourse, QStringLiteral("—"));
}

void TestReportingViewModel::canGenerateWithoutDepartmentWhenDurationValid()
{
    ReportingViewModel vm;
    vm.setDurationType(0);
    QVERIFY(!vm.canGenerate());     // no duration value yet
    vm.setDay("2026-08-14");
    QVERIFY(vm.canGenerate());      // department optional -> generatable
}

void TestReportingViewModel::canGenerateDayRequiresValidDate()
{
    ReportingViewModel vm;
    vm.setDepartment("CE");              // NOTE: in this task setDepartment only stores + fires network; see Task 5
    vm.setDurationType(0);
    QVERIFY(!vm.canGenerate());          // no day yet
    vm.setDay("2026-08-14");
    QVERIFY(vm.canGenerate());
    vm.setDay("not-a-date");
    QVERIFY(!vm.canGenerate());
}

void TestReportingViewModel::canGenerateMonthRequiresMonthAndYear()
{
    ReportingViewModel vm;
    vm.setDepartment("CE");
    vm.setDurationType(1);
    vm.setMonth(2);
    QVERIFY(!vm.canGenerate());          // no year
    vm.setMonthYear(2026);
    QVERIFY(vm.canGenerate());
    vm.setMonth(0);
    QVERIFY(!vm.canGenerate());          // month out of 1..12
}

void TestReportingViewModel::canGenerateSemesterRequiresSemesterAndYear()
{
    ReportingViewModel vm;
    vm.setDepartment("CE");
    vm.setDurationType(2);
    vm.setSemester("First Semester");
    QVERIFY(!vm.canGenerate());          // no year
    vm.setSemYear(2026);
    QVERIFY(vm.canGenerate());
}

void TestReportingViewModel::canGenerateCustomRequiresOrderedRange()
{
    ReportingViewModel vm;
    vm.setDepartment("CE");
    vm.setDurationType(3);
    vm.setCustomStart("2026-03-31");
    vm.setCustomEnd("2026-01-01");
    QVERIFY(!vm.canGenerate());          // start > end
    vm.setCustomEnd("2026-06-30");
    QVERIFY(vm.canGenerate());
}

void TestReportingViewModel::settersEmitAndUpdateCanGenerate()
{
    ReportingViewModel vm;
    QSignalSpy canGenSpy(&vm, &ReportingViewModel::canGenerateChanged);
    QSignalSpy durSpy(&vm, &ReportingViewModel::durationTypeChanged);
    vm.setDurationType(3);
    QVERIFY(durSpy.count() >= 1);
    vm.setDepartment("CE");
    vm.setCustomStart("2026-01-01");
    vm.setCustomEnd("2026-02-01");
    QVERIFY(vm.canGenerate());
    QVERIFY(canGenSpy.count() >= 1);
}

void TestReportingViewModel::onDepartmentsLoadedPopulates()
{
    ReportingViewModel vm;
    QSignalSpy spy(&vm, &ReportingViewModel::departmentsChanged);
    vm.onDepartmentsLoaded({ "CE", "IT" });
    QCOMPARE(vm.departments(), QStringList({ "CE", "IT" }));
    QVERIFY(spy.count() >= 1);
}

void TestReportingViewModel::onYearsLoadedPopulates()
{
    ReportingViewModel vm;
    QSignalSpy spy(&vm, &ReportingViewModel::yearsChanged);
    vm.onYearsLoaded({ "2026", "2025" });
    QCOMPARE(vm.years(), QStringList({ "2026", "2025" }));
    QVERIFY(spy.count() >= 1);
}

void TestReportingViewModel::onCoursesLoadedPopulates()
{
    ReportingViewModel vm;
    QSignalSpy spy(&vm, &ReportingViewModel::coursesChanged);
    vm.onCoursesLoaded({ "BSCE", "BSEE" });
    QCOMPARE(vm.courses(), QStringList({ "BSCE", "BSEE" }));
    QVERIFY(spy.count() >= 1);
}

void TestReportingViewModel::onCoursesLoadedFiltersServerAllEntry()
{
    // ReportController::loadCourses requests include_all=true, so the server
    // list already carries its own "All" entry; LCascadingSelect prepends a
    // second "All" of its own. onCoursesLoaded must strip the server's entry
    // (case-insensitive, trimmed) so only the cascade's "All" remains.
    ReportingViewModel vm;
    vm.onCoursesLoaded({ "All", "BSCE", "BSEE" });
    QCOMPARE(vm.courses(), QStringList({ "BSCE", "BSEE" }));

    ReportingViewModel vm2;
    vm2.onCoursesLoaded({ "all courses", "BSIT" });
    QCOMPARE(vm2.courses(), QStringList({ "BSIT" }));
}

void TestReportingViewModel::setDepartmentClearsCourse()
{
    ReportingViewModel vm;
    vm.onCoursesLoaded({ "BSCE" });
    vm.setCourse("BSCE");
    QCOMPARE(vm.course(), QStringLiteral("BSCE"));
    vm.setDepartment("IT");
    QCOMPARE(vm.course(), QString());     // dependent-clear
}

void TestReportingViewModel::onReportDataReadyPopulatesPreview()
{
    ReportingViewModel vm;
    QSignalSpy resultSpy(&vm, &ReportingViewModel::resultChanged);
    const QJsonArray data = QJsonDocument::fromJson(R"([
        {"name":"A","course":"BSCE","year_level":"1","visits":"10"},
        {"name":"B","course":"BSIT","year_level":"2","visits":"4"}
    ])").array();
    vm.onReportDataReady(data);
    QVERIFY(vm.hasResult());
    QCOMPARE(vm.rows()->count(), 2);
    QCOMPARE(vm.courseBars()->rowCount(), 2);
    QCOMPARE(vm.totalVisits(), 14);
    QCOMPARE(vm.studentsShown(), 2);
    QCOMPARE(vm.topCourse(), QStringLiteral("BSCE"));
    QVERIFY(!vm.loading());
    QVERIFY(vm.errorText().isEmpty());
    QVERIFY(resultSpy.count() >= 1);
}

void TestReportingViewModel::onReportDataReadyEmptyIsSuccessNotError()
{
    ReportingViewModel vm;
    vm.onReportDataReady(QJsonArray());
    QVERIFY(vm.hasResult());
    QCOMPARE(vm.rows()->count(), 0);
    QCOMPARE(vm.totalVisits(), 0);
    QCOMPARE(vm.topCourse(), QStringLiteral("—"));
    QVERIFY(vm.errorText().isEmpty());   // empty result is NOT an error
}

void TestReportingViewModel::analytics_populatedFromResult()
{
    ReportingViewModel vm;
    QJsonArray rows{
        QJsonObject{{"school_id","1"},{"name","Ana"},{"course","BSIT"},
                    {"department","CCS"},{"year_level","1"},{"visits",5}},
        QJsonObject{{"school_id","2"},{"name","Ben"},{"course","BSCS"},
                    {"department","CCS"},{"year_level","1"},{"visits",3}},
    };
    vm.onReportDataReady(rows);                       // same seam the controller signal uses

    QCOMPARE(vm.uniqueVisitors(), 2);
    QCOMPARE(vm.avgVisitsPerVisitor(), 4.0);
    QCOMPARE(vm.topDepartment(), QStringLiteral("CCS"));
    QCOMPARE(vm.topDepartmentVisits(), 8);
    QVERIFY(vm.topStudents() != nullptr);
    QCOMPARE(vm.topStudents()->count(), 2);
    QCOMPARE(vm.topCourses()->count(), 2);
    QCOMPARE(vm.topDepartments()->count(), 1);        // single dept CCS
}

void TestReportingViewModel::onReportErrorSetsErrorClearsLoading()
{
    ReportingViewModel vm;
    vm.onReportError("Department is required", false);
    QCOMPARE(vm.errorText(), QStringLiteral("Department is required"));
    QVERIFY(!vm.loading());
}

void TestReportingViewModel::generateWhileLoadingIsNoop()
{
    ReportingViewModel vm;
    vm.setDepartment("CE");
    vm.setDurationType(0);
    vm.setDay("2026-08-14");
    QVERIFY(vm.canGenerate());
    vm.generateReport();                 // fires; operation in flight
    QVERIFY(vm.loading());
    QVERIFY(!vm.canGenerate());          // gated while the operation is in flight
    vm.generateReport();                 // no-op while in flight
    QVERIFY(vm.loading());
    // BOTH children must settle before the operation finalizes.
    vm.onReportDataReady(QJsonArray());
    vm.onTimeAnalyticsReady(QList<int>{}, QList<int>{});
    QVERIFY(!vm.loading());
    QVERIFY(vm.canGenerate());
}

void TestReportingViewModel::generateReportWithoutDepartmentShowsValidationMessage()
{
    ReportingViewModel vm;
    vm.generateReport();            // no duration -> validation (department not required)
    QCOMPARE(vm.errorText(), QStringLiteral("Complete the selected duration before generating a report."));
    QVERIFY(!vm.loading());
}

void TestReportingViewModel::generateReportWithIncompleteDurationShowsValidationMessage()
{
    ReportingViewModel vm;
    vm.setDepartment("CE");
    vm.setDurationType(0);        // Day mode, no day set
    vm.generateReport();
    QCOMPARE(vm.errorText(), QStringLiteral("Complete the selected duration before generating a report."));
    QVERIFY(!vm.loading());
}

void TestReportingViewModel::filtersCompleteTracksDurationOnly()
{
    ReportingViewModel vm;
    QVERIFY(!vm.filtersComplete());     // no duration
    vm.setDurationType(0);
    QVERIFY(!vm.filtersComplete());     // no day yet
    vm.setDay("2026-08-14");
    QVERIFY(vm.filtersComplete());      // valid duration, no department needed
}

void TestReportingViewModel::validationMessageClearsWhenFiltersComplete()
{
    ReportingViewModel vm;
    vm.generateReport();                 // no duration -> validation message
    QCOMPARE(vm.errorText(), QStringLiteral("Complete the selected duration before generating a report."));
    vm.setDurationType(0);
    QVERIFY(!vm.errorText().isEmpty());  // still incomplete (no day) -> stays
    vm.setDay("2026-08-14");             // now complete -> auto-clears
    QVERIFY(vm.errorText().isEmpty());
}

void TestReportingViewModel::realFetchErrorNotAutoClearedByFilterChange()
{
    ReportingViewModel vm;
    vm.setDepartment("CE");
    vm.setDurationType(0);
    vm.setDay("2026-08-14");
    QVERIFY(vm.filtersComplete());
    vm.generateReport();                 // fetches (loading true)
    vm.onReportError("Server error 500", false);   // a REAL fetch error
    QCOMPARE(vm.errorText(), QStringLiteral("Server error 500"));
    // Changing a filter while still complete must NOT clear a real fetch error.
    vm.setDay("2026-08-15");
    QCOMPARE(vm.errorText(), QStringLiteral("Server error 500"));
}

void TestReportingViewModel::buildFiltersAllowsEmptyDepartment()
{
    const QJsonObject f = ReportingViewModel::buildFilters(
        "", "", 0, QDate(2026, 8, 14), 0, 0, "", 0, QDate(), QDate());
    QCOMPARE(f.value("department").toString(), QString());   // empty = all departments
    QVERIFY(f.contains("department"));
}

void TestReportingViewModel::normalizeExportRowsCoercesVisitsToNumber()
{
    const QJsonArray in = QJsonDocument::fromJson(R"([
        {"name":"Ana","course":"BSIT","visits":"5","year_level":"3"},
        {"name":"Ben","course":"BSCE","visits":8}
    ])").array();
    const QJsonArray out = ReportingViewModel::normalizeExportRows(in);
    QCOMPARE(out.size(), 2);
    QVERIFY(out.at(0).toObject().value("visits").isDouble());
    QCOMPARE(out.at(0).toObject().value("visits").toInt(), 5);
    QCOMPARE(out.at(1).toObject().value("visits").toInt(), 8);
    QCOMPARE(out.at(0).toObject().value("course").toString(), QStringLiteral("BSIT"));
    QCOMPARE(ReportingViewModel::normalizeExportRows(QJsonArray()).size(), 0);
}

void TestReportingViewModel::semesterWindowMatchesServerRanges()
{
    DateRange first = ReportingViewModel::semesterWindow("First Semester", 2026);
    QVERIFY(first.valid);
    QCOMPARE(first.start, QStringLiteral("2026-06-01"));
    QCOMPARE(first.end,   QStringLiteral("2026-10-31"));

    DateRange second = ReportingViewModel::semesterWindow("Second Semester", 2026);
    QCOMPARE(second.start, QStringLiteral("2026-11-01"));
    QCOMPARE(second.end,   QStringLiteral("2027-03-31"));   // crosses the year

    DateRange summer = ReportingViewModel::semesterWindow("Summer", 2026);
    QCOMPARE(summer.start, QStringLiteral("2026-04-01"));
    QCOMPARE(summer.end,   QStringLiteral("2026-05-31"));

    QVERIFY(!ReportingViewModel::semesterWindow("", 2026).valid);
    QVERIFY(!ReportingViewModel::semesterWindow("First Semester", 0).valid);
}

void TestReportingViewModel::buildExportFiltersDayHasRangeAndLabels()
{
    const QJsonObject f = ReportingViewModel::buildExportFilters(
        "", "", 0, QDate(2026, 8, 14), 0, 0, "", 0, QDate(), QDate(), "Bar");
    QCOMPARE(f.value("department").toString(), QStringLiteral("All Departments"));
    QCOMPARE(f.value("course").toString(),     QStringLiteral("All Courses"));
    QCOMPARE(f.value("start").toString(),      QStringLiteral("2026-08-14"));
    QCOMPARE(f.value("end").toString(),        QStringLiteral("2026-08-14"));
    QCOMPARE(f.value("schoolYear").toString(), QStringLiteral("2026"));
    QCOMPARE(f.value("chartType").toString(),  QStringLiteral("Bar"));
}

void TestReportingViewModel::buildExportFiltersSemesterUsesServerWindow()
{
    const QJsonObject f = ReportingViewModel::buildExportFilters(
        "CE", "BSCE", 2, QDate(), 0, 0, "Second Semester", 2026, QDate(), QDate(), "Pie");
    QCOMPARE(f.value("department").toString(), QStringLiteral("CE"));
    QCOMPARE(f.value("course").toString(),     QStringLiteral("BSCE"));
    QCOMPARE(f.value("start").toString(),      QStringLiteral("2026-11-01"));
    QCOMPARE(f.value("end").toString(),        QStringLiteral("2027-03-31"));
    QCOMPARE(f.value("schoolYear").toString(), QStringLiteral("2026"));
    QCOMPARE(f.value("chartType").toString(),  QStringLiteral("Pie"));
}

void TestReportingViewModel::buildExportFiltersMonthSchoolYear()
{
    const QJsonObject f = ReportingViewModel::buildExportFilters(
        "IT", "", 1, QDate(), 2, 2025, "", 0, QDate(), QDate(), "Bar");
    QCOMPARE(f.value("start").toString(),      QStringLiteral("2025-02-01"));
    QCOMPARE(f.value("end").toString(),        QStringLiteral("2025-02-28"));
    QCOMPARE(f.value("schoolYear").toString(), QStringLiteral("2025"));
}

void TestReportingViewModel::canExportTruthTable()
{
    ReportingViewModel vm;
    QVERIFY(!vm.canExport());                       // no result yet
    const QJsonArray data = QJsonDocument::fromJson(
        R"([{"name":"Ana","course":"BSIT","visits":"5"}])").array();
    vm.onReportDataReady(data);                     // hasResult + rows>0, not loading/exporting
    QVERIFY(vm.canExport());
    vm.onReportDataReady(QJsonArray());             // hasResult true but zero rows
    QVERIFY(!vm.canExport());
}

void TestReportingViewModel::paletteAndChartTypeSettersEmit()
{
    ReportingViewModel vm;
    QCOMPARE(vm.palette(), QStringLiteral("Default"));
    QCOMPARE(vm.chartType(), QStringLiteral("Bar"));
    QSignalSpy pSpy(&vm, &ReportingViewModel::paletteChanged);
    QSignalSpy cSpy(&vm, &ReportingViewModel::chartTypeChanged);
    vm.setPalette("Blue");
    vm.setChartType("Pie");
    QCOMPARE(vm.palette(), QStringLiteral("Blue"));
    QCOMPARE(vm.chartType(), QStringLiteral("Pie"));
    QCOMPARE(pSpy.count(), 1);
    QCOMPARE(cSpy.count(), 1);
    QCOMPARE(vm.palettes().size(), 4);
    QCOMPARE(vm.chartTypes().size(), 2);
}

void TestReportingViewModel::applyResultStoresNormalizedExportRows()
{
    ReportingViewModel vm;
    const QJsonArray data = QJsonDocument::fromJson(
        R"([{"name":"Ana","course":"BSIT","visits":"5"}])").array();
    vm.onReportDataReady(data);
    QVERIFY(vm.canExport());   // rows stored + hasResult
}

void TestReportingViewModel::failedRefetchDisablesExportAndClearsRows()
{
    ReportingViewModel vm;
    vm.onReportDataReady(QJsonDocument::fromJson(
        R"([{"name":"Ana","course":"BSIT","visits":"5"}])").array());
    QVERIFY(vm.canExport());                 // a clean result

    // Change filters, then fire a new fetch that fails.
    vm.setDurationType(0);
    vm.setDay(QStringLiteral("2026-08-14")); // filtersComplete
    vm.generateReport();                     // setLoading(true): clears m_exportRows, loading
    QVERIFY(!vm.canExport());                // gated while loading
    vm.onReportError(QStringLiteral("Server error"), false);   // loading=false, errorText set
    QVERIFY(!vm.canExport());                // errorText non-empty AND rows cleared
    // (Task 6 adds the complementary assertion that exportPdf then hits the
    // "No data" guard — exportPdf does not exist yet in Task 5.)
}

void TestReportingViewModel::exportPdfWritesFile()
{
    ReportingViewModel vm;
    vm.onReportDataReady(QJsonDocument::fromJson(
        R"([{"school_id":"1","name":"Ana","gender":"F","status":"Regular",
             "course":"BSIT","department":"IT","year_level":"3","visits":"5"}])").array());
    QTemporaryDir dir;
    const QString path = dir.filePath("report.pdf");
    vm.exportPdf(QUrl::fromLocalFile(path));
    QTRY_VERIFY(!vm.exporting());                 // queued render completes
    QVERIFY(QFileInfo::exists(path));
    QFile f(path); QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.read(4), QByteArray("%PDF"));
    QVERIFY(!vm.exportStatus().isEmpty());
    QVERIFY(vm.exportError().isEmpty());
}

void TestReportingViewModel::exportExcelWritesReadableCell()
{
    ReportingViewModel vm;
    vm.setIncludeRosterInExport(true);   // roster is opt-in (spec §9); this test reads a roster cell
    vm.onReportDataReady(QJsonDocument::fromJson(
        R"([{"school_id":"1","name":"Ana","gender":"F","status":"Regular",
             "course":"BSIT","department":"IT","year_level":"3","visits":"5"}])").array());
    QTemporaryDir dir;
    const QString path = dir.filePath("report.xlsx");
    vm.exportExcel(QUrl::fromLocalFile(path));
    QTRY_VERIFY(!vm.exporting());
    QVERIFY(QFileInfo::exists(path));
    QXlsx::Document doc(path);
    QVERIFY(doc.load());
    // The student name lands somewhere in the sheet's table body.
    bool foundName = false;
    for (int r = 1; r <= 40 && !foundName; ++r)
        for (int c = 1; c <= 8; ++c)
            if (doc.read(r, c).toString() == QStringLiteral("Ana")) { foundName = true; break; }
    QVERIFY(foundName);
}

void TestReportingViewModel::exportPdfEmptyRowsShowsNoDataError()
{
    ReportingViewModel vm;   // never generated -> m_exportRows empty
    QTemporaryDir dir;
    vm.exportPdf(QUrl::fromLocalFile(dir.filePath("x.pdf")));
    QVERIFY(vm.exportError().contains(QStringLiteral("No data")));
    QVERIFY(!vm.exporting());
}

void TestReportingViewModel::exportPdfInvalidUrlShowsError()
{
    ReportingViewModel vm;
    vm.onReportDataReady(QJsonDocument::fromJson(
        R"([{"name":"Ana","course":"BSIT","visits":"5"}])").array());
    vm.exportPdf(QUrl("https://example.com/x.pdf"));   // non-file URL -> empty local path
    QVERIFY(!vm.exportError().isEmpty());
    QVERIFY(!vm.exporting());
}

void TestReportingViewModel::exportWhileExportingIsNoop()
{
    ReportingViewModel vm;
    vm.onReportDataReady(QJsonDocument::fromJson(
        R"([{"name":"Ana","course":"BSIT","visits":"5"}])").array());
    QTemporaryDir dir;
    vm.exportPdf(QUrl::fromLocalFile(dir.filePath("a.pdf")));   // sets exporting=true (queued)
    QVERIFY(vm.exporting());
    const QString before = vm.exportError();
    vm.exportExcel(QUrl::fromLocalFile(dir.filePath("b.xlsx")));   // must no-op while exporting
    QCOMPARE(vm.exportError(), before);
    QTRY_VERIFY(!vm.exporting());                               // first export drains
}

void TestReportingViewModel::printReportEmptyRowsShowsNoDataError()
{
    ReportingViewModel vm;   // no rows -> must not open a dialog
    vm.printReport();
    QVERIFY(vm.exportError().contains(QStringLiteral("No data")));
    QVERIFY(!vm.exporting());
}

void TestReportingViewModel::includeRosterInExport_defaultsFalse() {
    ReportingViewModel vm;
    QCOMPARE(vm.includeRosterInExport(), false);   // spec §9: default OFF
}

void TestReportingViewModel::setIncludeRosterInExport_togglesAndSignals() {
    ReportingViewModel vm;
    QSignalSpy spy(&vm, &ReportingViewModel::includeRosterInExportChanged);
    vm.setIncludeRosterInExport(true);
    QCOMPARE(vm.includeRosterInExport(), true);
    QCOMPARE(spy.count(), 1);
    vm.setIncludeRosterInExport(true);   // no-op on unchanged value
    QCOMPARE(spy.count(), 1);
    vm.setIncludeRosterInExport(false);
    QCOMPARE(vm.includeRosterInExport(), false);
    QCOMPARE(spy.count(), 2);
}

void TestReportingViewModel::timeSection_propertyDefaults() {
    ReportingViewModel vm;
    QVERIFY(vm.timeError().isEmpty());
    QVERIFY(!vm.timeLoading());
    QVERIFY(!vm.hasTimeData());
    QVERIFY(vm.busiestHourLabel().isEmpty());
    QVERIFY(vm.busiestDayLabel().isEmpty());
    QVERIFY(vm.hourlyBars() != nullptr);
    QVERIFY(vm.weekdayBars() != nullptr);
    // Before any Generate, canGenerate depends on filters only (no operation pending).
    vm.setDurationType(0);
    vm.setDay("2026-08-14");
    QVERIFY(vm.canGenerate());
}

void TestReportingViewModel::generate_operationFinalizesOnlyWhenBothSettle() {
    ReportingViewModel vm;
    vm.setDurationType(0);
    vm.setDay("2026-08-14");
    QVERIFY(vm.canGenerate());
    vm.generateReport();
    QVERIFY(!vm.canGenerate());                       // operation in flight
    vm.onReportDataReady(QJsonArray());               // only rows settle
    QVERIFY(!vm.canGenerate());                       // time still pending -> not finalized
    vm.onTimeAnalyticsReady(denseHours(), denseWeek()); // time settles
    QVERIFY(vm.canGenerate());                        // both settled -> finalized
}

void TestReportingViewModel::generate_operationFinalizesRegardlessOfSettleOrder() {
    ReportingViewModel vm;
    vm.setDurationType(0);
    vm.setDay("2026-08-14");
    vm.generateReport();
    vm.onTimeAnalyticsError("x");                     // time settles FIRST
    QVERIFY(!vm.canGenerate());                       // rows still pending
    vm.onReportDataReady(QJsonArray());               // rows settle
    QVERIFY(vm.canGenerate());
}

void TestReportingViewModel::generate_rowsLoadingClearsAtRowsSettleIndependently() {
    ReportingViewModel vm;
    vm.setDurationType(0);
    vm.setDay("2026-08-14");
    vm.generateReport();
    QVERIFY(vm.loading());        // rows loading -> preview dim
    QVERIFY(vm.timeLoading());    // section spinner
    vm.onReportDataReady(QJsonArray());
    QVERIFY(!vm.loading());       // preview un-dims at rows settle...
    QVERIFY(vm.timeLoading());    // ...even though time is still running
    vm.onTimeAnalyticsReady(denseHours(), denseWeek());
    QVERIFY(!vm.timeLoading());
}

void TestReportingViewModel::outcome_rowsSuccessTimeError_reportRendersTimeErrorLocalized() {
    ReportingViewModel vm;
    vm.setDurationType(0);
    vm.setDay("2026-08-14");
    vm.generateReport();
    vm.onReportDataReady(QJsonDocument::fromJson(
        R"([{"school_id":"1","name":"Ana","course":"BSIT","department":"CCS","visits":5}])").array());
    vm.onTimeAnalyticsError("network down");
    QVERIFY(vm.hasResult());                 // primary report still rendered
    QVERIFY(vm.errorText().isEmpty());       // NOT the fatal rows-error path
    QVERIFY(vm.canExport());                 // export unaffected by the time failure
    QCOMPARE(vm.timeError(), QStringLiteral("network down"));  // localized
    QVERIFY(!vm.hasTimeData());
}

void TestReportingViewModel::outcome_rowsErrorTimeSuccess_primaryErrorFires() {
    ReportingViewModel vm;
    vm.setDurationType(0);
    vm.setDay("2026-08-14");
    vm.generateReport();
    vm.onReportError("Server 500", false);              // primary fatal path
    vm.onTimeAnalyticsReady(denseHours(), denseWeek()); // time succeeds anyway
    QCOMPARE(vm.errorText(), QStringLiteral("Server 500"));  // primary error fires
    QVERIFY(!vm.canExport());                           // rows error blocks export
    QVERIFY(vm.timeError().isEmpty());                  // time path had no error
    // (The "When?" section is gated on hasResult in QML — Task 5 — so it stays hidden here.)
}

void TestReportingViewModel::canExport_unaffectedByTimeOutcome() {
    ReportingViewModel vm;
    vm.setDurationType(0);
    vm.setDay("2026-08-14");
    vm.generateReport();
    vm.onReportDataReady(QJsonDocument::fromJson(
        R"([{"school_id":"1","name":"Ana","course":"BSIT","department":"CCS","visits":5}])").array());
    QVERIFY(vm.canExport());
    vm.onTimeAnalyticsError("boom");         // time error must not change canExport
    QVERIFY(vm.canExport());
}

void TestReportingViewModel::resetAtGenerate_clearsStaleTimeState() {
    ReportingViewModel vm;
    vm.setDurationType(0);
    vm.setDay("2026-08-14");
    vm.generateReport();
    vm.onReportDataReady(QJsonArray());
    vm.onTimeAnalyticsError("stale error");
    QCOMPARE(vm.timeError(), QStringLiteral("stale error"));
    // A second Generate must clear ALL When-section state before the new fetch fires.
    vm.setDay("2026-08-15");
    vm.generateReport();
    QVERIFY(vm.timeError().isEmpty());   // cleared at Generate start (staleness guard)
    QVERIFY(!vm.hasTimeData());
    QVERIFY(vm.timeLoading());           // section spinning again
}

void TestReportingViewModel::timeModels_windowedEveryHourLabeled() {
    ReportingViewModel vm;
    vm.setDurationType(0);
    vm.setDay("2026-08-14");
    vm.generateReport();
    vm.onReportDataReady(QJsonArray());
    vm.onTimeAnalyticsReady(denseHours(), denseWeek());

    QCOMPARE(vm.hourlyBars()->rowCount(), 15);   // [7,21] inclusive = 15 bars
    QCOMPARE(vm.weekdayBars()->rowCount(), 7);

    // EVERY hour in the window is labeled now (the h%3 blanking is gone).
    QCOMPARE(vm.hourlyBars()->data(vm.hourlyBars()->index(0, 0),
             BarsModel::LabelRole).toString(), QStringLiteral("7A"));   // openHour
    QCOMPARE(vm.hourlyBars()->data(vm.hourlyBars()->index(7, 0),
             BarsModel::LabelRole).toString(), QStringLiteral("2P"));   // hour 14
    QCOMPARE(vm.hourlyBars()->data(vm.hourlyBars()->index(14, 0),
             BarsModel::LabelRole).toString(), QStringLiteral("9P"));   // closeHour 21
    QVERIFY(!vm.hourlyBars()->data(vm.hourlyBars()->index(1, 0),
             BarsModel::LabelRole).toString().isEmpty());               // no blanks

    // Weekday bars are Monday-first; value at Mon = denseWeek() Sun-first idx1 = 40.
    QCOMPARE(vm.weekdayBars()->data(vm.weekdayBars()->index(0, 0),
             BarsModel::LabelRole).toString(), QStringLiteral("Mon"));
    QCOMPARE(vm.weekdayBars()->data(vm.weekdayBars()->index(6, 0),
             BarsModel::LabelRole).toString(), QStringLiteral("Sun"));
    QCOMPARE(vm.weekdayBars()->data(vm.weekdayBars()->index(0, 0),
             BarsModel::ValueRole).toInt(), 40);
}

void TestReportingViewModel::captions_formattedForKnownPeaks() {
    ReportingViewModel vm;
    vm.setDurationType(0);
    vm.setDay("2026-08-14");
    vm.generateReport();
    vm.onReportDataReady(QJsonArray());
    vm.onTimeAnalyticsReady(denseHours(), denseWeek());   // peak hour 14, peak day Monday
    QVERIFY(vm.hasTimeData());
    QCOMPARE(vm.busiestHourLabel(), QStringLiteral("2–3 PM"));
    QCOMPARE(vm.busiestDayLabel(), QStringLiteral("Monday"));
}

void TestReportingViewModel::hasTimeData_falseOnAllZeroShowsEmptyState() {
    ReportingViewModel vm;
    vm.setDurationType(0);
    vm.setDay("2026-08-14");
    vm.generateReport();
    vm.onReportDataReady(QJsonArray());
    vm.onTimeAnalyticsReady(zeros(24), zeros(7));
    QVERIFY(!vm.hasTimeData());
    QVERIFY(vm.busiestHourLabel().isEmpty());   // captions unused in the empty state
    QVERIFY(vm.busiestDayLabel().isEmpty());
    QCOMPARE(vm.hourlyBars()->rowCount(), 15);  // windowed bars (all zero)
    QCOMPARE(vm.weekdayBars()->rowCount(), 7);
}

void TestReportingViewModel::buildTimeExport_dataState_populatesLabelsCountsPeaks() {
    ReportingViewModel vm;
    vm.onTimeAnalyticsReady(denseHours(), denseWeek());   // peak hour 14, peak day Monday
    const ReportTimeExport te = vm.buildTimeExport();
    QCOMPARE(te.state, TimeAnalyticsExportState::Data);

    // 15 windowed hour labels ([7,21]), byte-identical to hourTick, EVERY label set.
    QCOMPARE(te.hourLabels.size(), 15);
    QCOMPARE(te.hourLabels.at(0), QStringLiteral("7A"));    // openHour 7
    QCOMPARE(te.hourLabels.at(7), QStringLiteral("2P"));    // hour 14
    QCOMPARE(te.hourLabels.at(14), QStringLiteral("9P"));   // closeHour 21

    // Counts equal the [7,21] SLICE of the RAW 24-wide hourly (NOT the whole array).
    const TimeAnalytics ta = TimeAnalytics::compute(denseHours(), denseWeek(), 7, 21);
    QCOMPARE(te.hourCounts, ta.hourly.mid(7, 15));   // hours 7..21 inclusive
    QCOMPARE(te.hourCounts.at(7), 12);   // hour 14 -> window index 7
    QCOMPARE(te.hourCounts.at(2), 3);    // hour 9  -> window index 2

    // 7 weekday labels Mon→Sun; counts == weekdayMonFirst.
    QCOMPARE(te.weekdayLabels.size(), 7);
    QCOMPARE(te.weekdayLabels.at(0), QStringLiteral("Mon"));
    QCOMPARE(te.weekdayLabels.at(6), QStringLiteral("Sun"));
    QCOMPARE(te.weekdayCounts, ta.weekdayMonFirst);
    QCOMPARE(te.weekdayCounts.at(0), 40);

    // Peak VALUE strings == the on-screen captions (parity).
    QCOMPARE(te.busiestHourLabel, QStringLiteral("2–3 PM"));
    QCOMPARE(te.busiestDayLabel, QStringLiteral("Monday"));
}

void TestReportingViewModel::buildTimeExport_emptyState_listsEmpty() {
    ReportingViewModel vm;
    vm.onTimeAnalyticsReady(zeros(24), zeros(7));   // all-zero, no error
    const ReportTimeExport te = vm.buildTimeExport();
    QCOMPARE(te.state, TimeAnalyticsExportState::Empty);
    QVERIFY(te.hourLabels.isEmpty());
    QVERIFY(te.hourCounts.isEmpty());
    QVERIFY(te.weekdayLabels.isEmpty());
    QVERIFY(te.weekdayCounts.isEmpty());
    QVERIFY(te.busiestHourLabel.isEmpty());
    QVERIFY(te.busiestDayLabel.isEmpty());
}

void TestReportingViewModel::buildTimeExport_errorState_winsOverData() {
    ReportingViewModel vm;
    vm.onTimeAnalyticsReady(denseHours(), denseWeek());   // hasData == true
    vm.onTimeAnalyticsError(QStringLiteral("network down"));   // error set AFTER data
    const ReportTimeExport te = vm.buildTimeExport();
    QCOMPARE(te.state, TimeAnalyticsExportState::Error);   // error wins over hasData
    QVERIFY(te.hourLabels.isEmpty());
    QVERIFY(te.weekdayLabels.isEmpty());
    QVERIFY(te.busiestHourLabel.isEmpty());
    QVERIFY(te.busiestDayLabel.isEmpty());
}

void TestReportingViewModel::buildTimeExport_defensiveWrongLength_degradesToEmpty() {
    ReportingViewModel vm;
    vm.onTimeAnalyticsReady(zeros(10), zeros(7));   // hourly wrong length -> compute bails
    const ReportTimeExport te = vm.buildTimeExport();
    QCOMPARE(te.state, TimeAnalyticsExportState::Empty);   // degrade, no OOB
    QVERIFY(te.hourLabels.isEmpty());
    QVERIFY(te.weekdayLabels.isEmpty());
}

void TestReportingViewModel::windowedCaption_followsInWindowPeakNotOverall() {
    // Overall 24h peak is at hour 6 (out of [7,21]); the in-window peak is hour 10.
    // The caption must name the in-window peak, and screen must match export.
    QList<int> byHour = zeros(24);
    byHour[6] = 30;    // taller, pre-open -> ignored by the window
    byHour[10] = 9;    // in-window peak -> "10-11 AM"
    ReportingViewModel vm;
    vm.onTimeAnalyticsReady(byHour, denseWeek());
    QCOMPARE(vm.busiestHourLabel(), QStringLiteral("10–11 AM"));
    QCOMPARE(vm.buildTimeExport().busiestHourLabel, QStringLiteral("10–11 AM"));
    QVERIFY(vm.hasTimeData());
}

void TestReportingViewModel::allOutOfHours_hourCaptionEmptyWeekdayStillShown() {
    // Every hourly visit is outside [7,21]; the windowed hour peak is 0 so the hour
    // caption is suppressed, but hasData/weekday data remain (decision 5).
    QList<int> byHour = zeros(24);
    byHour[2] = 6;     // pre-open staff login, out of window
    ReportingViewModel vm;
    vm.onTimeAnalyticsReady(byHour, denseWeek());
    QVERIFY(vm.busiestHourLabel().isEmpty());                       // screen caption gone
    QVERIFY(vm.buildTimeExport().busiestHourLabel.isEmpty());       // export caption gone
    QVERIFY(vm.hasTimeData());                                      // overall data present
    QCOMPARE(vm.busiestDayLabel(), QStringLiteral("Monday"));       // weekday unaffected
    QCOMPARE(vm.weekdayBars()->rowCount(), 7);
}

void TestReportingViewModel::orientation_defaultsToPortrait()
{
    ReportingViewModel vm;
    QCOMPARE(vm.orientation(), QStringLiteral("Portrait"));
}

void TestReportingViewModel::setOrientation_validValueEmitsAndUpdates()
{
    ReportingViewModel vm;
    QSignalSpy spy(&vm, &ReportingViewModel::orientationChanged);
    vm.setOrientation("Landscape");
    QCOMPARE(vm.orientation(), QStringLiteral("Landscape"));
    QCOMPARE(spy.count(), 1);
}

void TestReportingViewModel::setOrientation_blankRejectedNoSignal()
{
    ReportingViewModel vm;
    QSignalSpy spy(&vm, &ReportingViewModel::orientationChanged);
    vm.setOrientation("");
    QCOMPARE(vm.orientation(), QStringLiteral("Portrait"));   // unchanged
    QCOMPARE(spy.count(), 0);                                  // no signal at all (decision 3)
}

void TestReportingViewModel::setOrientation_garbageRejectedNoSignal()
{
    ReportingViewModel vm;
    QSignalSpy spy(&vm, &ReportingViewModel::orientationChanged);
    vm.setOrientation("Diagonal");
    QCOMPARE(vm.orientation(), QStringLiteral("Portrait"));   // unchanged
    QCOMPARE(spy.count(), 0);                                  // no signal at all (decision 3)
}

void TestReportingViewModel::setOrientation_sameValueNoSignal()
{
    ReportingViewModel vm;
    QSignalSpy spy(&vm, &ReportingViewModel::orientationChanged);
    vm.setOrientation("Portrait");   // same as the default -> v == m_orientation half of the guard
    QCOMPARE(vm.orientation(), QStringLiteral("Portrait"));   // unchanged
    QCOMPARE(spy.count(), 0);                                  // no assignment, no signal
}

void TestReportingViewModel::orientations_containsPortraitAndLandscape()
{
    ReportingViewModel vm;
    QCOMPARE(vm.orientations(), QStringList({ QStringLiteral("Portrait"), QStringLiteral("Landscape") }));
}

void TestReportingViewModel::pageOrientation_mapsPortraitAndLandscape()
{
    QCOMPARE(ReportingViewModel::pageOrientation("Portrait"), QPageLayout::Portrait);
    QCOMPARE(ReportingViewModel::pageOrientation("Landscape"), QPageLayout::Landscape);
}

void TestReportingViewModel::pageOrientation_mapsBlankAndGarbageToPortrait()
{
    QCOMPARE(ReportingViewModel::pageOrientation(""), QPageLayout::Portrait);
    QCOMPARE(ReportingViewModel::pageOrientation("garbage"), QPageLayout::Portrait);
}

QTEST_MAIN(TestReportingViewModel)
#include "tst_reportingviewmodel.moc"
