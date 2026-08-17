#include <QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "ReportRowsModel.h"

class TestReportRowsModel : public QObject
{
    Q_OBJECT
private slots:
    void setRowsPopulatesRolesAndCount();
    void visitsParsesStringOrNumber();
    void setRowsEmptyClears();
};

static QJsonArray arr(const char *json)
{
    return QJsonDocument::fromJson(json).array();
}

void TestReportRowsModel::setRowsPopulatesRolesAndCount()
{
    ReportRowsModel m;
    QSignalSpy countSpy(&m, &ReportRowsModel::countChanged);
    m.setRows(arr(R"([
        {"name":"Maria Santos","course":"BSCE","year_level":"3","visits":"42"},
        {"name":"Jose Cruz","course":"BSIT","year_level":"1","visits":"7"}
    ])"));
    QCOMPARE(m.count(), 2);
    QVERIFY(countSpy.count() >= 1);
    const QModelIndex i0 = m.index(0, 0);
    QCOMPARE(m.data(i0, ReportRowsModel::NameRole).toString(), QStringLiteral("Maria Santos"));
    QCOMPARE(m.data(i0, ReportRowsModel::CourseRole).toString(), QStringLiteral("BSCE"));
    QCOMPARE(m.data(i0, ReportRowsModel::YearLevelRole).toString(), QStringLiteral("3"));
    QCOMPARE(m.data(i0, ReportRowsModel::VisitsRole).toInt(), 42);
    const QHash<int, QByteArray> roles = m.roleNames();
    QCOMPARE(roles.value(ReportRowsModel::NameRole), QByteArray("name"));
    QCOMPARE(roles.value(ReportRowsModel::VisitsRole), QByteArray("visits"));
}

void TestReportRowsModel::visitsParsesStringOrNumber()
{
    QCOMPARE(reportVisits(QJsonObject{ {"visits", "13"} }), 13); // mysqli string
    QCOMPARE(reportVisits(QJsonObject{ {"visits", 9} }), 9);      // JSON number
    QCOMPARE(reportVisits(QJsonObject{ {"name", "x"} }), 0);      // missing
}

void TestReportRowsModel::setRowsEmptyClears()
{
    ReportRowsModel m;
    m.setRows(arr(R"([{"name":"A","course":"C","year_level":"1","visits":"1"}])"));
    QCOMPARE(m.count(), 1);
    m.setRows(QJsonArray());
    QCOMPARE(m.count(), 0);
}

QTEST_APPLESS_MAIN(TestReportRowsModel)
#include "tst_reportrowsmodel.moc"
