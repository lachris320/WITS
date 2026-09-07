#include <QtTest>
#include "SearchResultsModel.h"
#include "studentdata.h"
#include "apiconfig.h"

class TestSearchResultsModel : public QObject
{
    Q_OBJECT
private slots:
    void photoRoleJoinsBaseUrlWhenRelative()
    {
        SearchResultsModel m;
        StudentRecord r; r.name = "Maria Santos";
        r.photo = "uploads/students/2023-1.jpg";
        m.setRecords({ r });
        const QString got = m.data(m.index(0), SearchResultsModel::PhotoRole).toString();
        QCOMPARE(got, ApiConfig::endpoint("uploads/students/2023-1.jpg").toString());
        QVERIFY(got.startsWith("http"));
        QVERIFY(got.endsWith("uploads/students/2023-1.jpg"));
    }

    void photoRoleEmptyWhenNoPhoto()
    {
        SearchResultsModel m;
        StudentRecord r; r.name = "Jose Ramirez"; r.photo = "";
        m.setRecords({ r });
        QCOMPARE(m.data(m.index(0), SearchResultsModel::PhotoRole).toString(), QString());
    }

    void roleNamesExposePhoto()
    {
        SearchResultsModel m;
        QCOMPARE(m.roleNames().value(SearchResultsModel::PhotoRole), QByteArray("photo"));
    }
};

QTEST_APPLESS_MAIN(TestSearchResultsModel)
#include "tst_searchresultsmodel.moc"
