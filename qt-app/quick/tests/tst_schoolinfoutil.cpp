#include <QtTest>
#include <QTemporaryFile>
#include <QUrl>
#include "SchoolInfoUtil.h"

// Unit test for the PURE, Qt-Gui-free SchoolInfoUtil::resolveLogoUrl() helper
// (quick/viewmodels/SchoolInfoUtil.h), extracted from KioskViewModel and
// SchoolInfoViewModel so the two school-identity surfaces share one
// implementation of the robustness-critical logo seam instead of two copies
// that could drift. A rotted path must NEVER resolve to a non-empty URL.
class TestSchoolInfoUtil : public QObject
{
    Q_OBJECT
private slots:
    void emptyPathYieldsNoLogo();
    void realFileYieldsLocalFileUrl();
    void nonExistentPathYieldsNoLogo();
};

void TestSchoolInfoUtil::emptyPathYieldsNoLogo()
{
    bool hasLogo = true;   // seed with the wrong value to prove it is written
    const QUrl url = SchoolInfoUtil::resolveLogoUrl(QString(), &hasLogo);
    QVERIFY(!hasLogo);
    QVERIFY(url.isEmpty());
}

void TestSchoolInfoUtil::realFileYieldsLocalFileUrl()
{
    QTemporaryFile f;
    QVERIFY(f.open());
    const QString path = f.fileName();

    bool hasLogo = false;
    const QUrl url = SchoolInfoUtil::resolveLogoUrl(path, &hasLogo);
    QVERIFY(hasLogo);
    QCOMPARE(url, QUrl::fromLocalFile(path));
    QVERIFY(url.isLocalFile());
}

void TestSchoolInfoUtil::nonExistentPathYieldsNoLogo()
{
    QString path;
    {
        // Let a QTemporaryFile create then destroy a unique name so we have a
        // path that is guaranteed not to exist on disk right now.
        QTemporaryFile f;
        QVERIFY(f.open());
        path = f.fileName();
    }
    QVERIFY(!QFile::exists(path));

    bool hasLogo = true;
    const QUrl url = SchoolInfoUtil::resolveLogoUrl(path, &hasLogo);
    QVERIFY(!hasLogo);
    QVERIFY(url.isEmpty());
}

QTEST_APPLESS_MAIN(TestSchoolInfoUtil)
#include "tst_schoolinfoutil.moc"
