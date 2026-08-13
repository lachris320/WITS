#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>
#include "ImportViewModel.h"
#include "importdata.h"
#include "AdminSession.h"

class TestImportViewModel : public QObject
{
    Q_OBJECT
private slots:
    void idleToDuplicatesToUploadToResult();
    void allDuplicatesBlocksContinue();
    void validationFailureStaysIdle();
    void uploadFailedAuthSetsAuthFailure();

private:
    // Writes a minimal CSV and returns its file:// URL.
    static QUrl writeCsv(QTemporaryDir &dir, const QString &name, const QString &text)
    {
        const QString path = dir.filePath(name);
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write(text.toUtf8());
        f.close();
        return QUrl::fromLocalFile(path);
    }
};

void TestImportViewModel::idleToDuplicatesToUploadToResult()
{
    AdminSession::instance().setKey("s3cr3t-key");
    QTemporaryDir dir;
    ImportViewModel vm;
    QCOMPARE(vm.phase(), ImportViewModel::Phase::Idle);

    vm.setDataFile(writeCsv(dir, "in.csv",
        "School ID,Full Name\n21-1-0001,Juan Dela Cruz\n21-1-0002,Maria Clara\n"));
    QCOMPARE(vm.parsedCount(), 2);

    vm.startImport();
    QCOMPARE(vm.phase(), ImportViewModel::Phase::CheckingDuplicates);

    // Simulate the controller answering "no duplicates" -> upload begins.
    vm.onDuplicatesResolved({});
    vm.onUploadStarted();
    QCOMPARE(vm.phase(), ImportViewModel::Phase::Uploading);
    vm.onUploadProgress(100);
    QCOMPARE(vm.phase(), ImportViewModel::Phase::Processing);

    UploadResult r; r.ok = true; r.successCount = 2; r.skippedCount = 0; r.errorCount = 0;
    vm.onUploadFinished(r);
    QCOMPARE(vm.phase(), ImportViewModel::Phase::Done);
    QVERIFY(vm.resultText().contains("2"));
    QVERIFY(vm.resultText().contains("imported", Qt::CaseInsensitive));
}

void TestImportViewModel::allDuplicatesBlocksContinue()
{
    AdminSession::instance().setKey("s3cr3t-key");
    QTemporaryDir dir;
    ImportViewModel vm;
    vm.setDataFile(writeCsv(dir, "in.csv",
        "School ID,Full Name\n21-1-0001,Juan\n21-1-0002,Maria\n"));
    vm.startImport();

    vm.onDuplicatesResolved({"21-1-0001", "21-1-0002"});   // ALL duplicates
    QCOMPARE(vm.phase(), ImportViewModel::Phase::AwaitingDuplicates);
    QVERIFY(vm.allDuplicates());

    // continueAfterDuplicates must be a no-op when everything is a duplicate.
    vm.continueAfterDuplicates();
    QVERIFY(vm.phase() != ImportViewModel::Phase::Uploading);
}

void TestImportViewModel::validationFailureStaysIdle()
{
    AdminSession::instance().setKey("s3cr3t-key");
    QTemporaryDir dir;
    ImportViewModel vm;
    vm.setDataFile(writeCsv(dir, "bad.csv",
        "Full Name,Course\nJuan,BSIT\n"));   // no School ID column
    vm.startImport();

    QCOMPARE(vm.phase(), ImportViewModel::Phase::Idle);
    QVERIFY(vm.errorText().contains("School ID"));
}

void TestImportViewModel::uploadFailedAuthSetsAuthFailure()
{
    AdminSession::instance().setKey("bad-key");
    ImportViewModel vm;
    vm.onUploadFailed(QStringLiteral(
        "Admin authentication required — re-enter via admin login."));
    QCOMPARE(vm.phase(), ImportViewModel::Phase::Failed);
    QVERIFY(vm.authFailure());
    QVERIFY(!vm.errorText().isEmpty());
}

QTEST_MAIN(TestImportViewModel)
#include "tst_importviewmodel.moc"
