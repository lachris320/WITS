#include <QtTest>
#include <QSignalSpy>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QDir>
#include <QImage>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include "SettingsViewModel.h"
#include "AdminSession.h"
#include "HttpForm.h"

class TestSettingsViewModel : public QObject
{
    Q_OBJECT
    QTemporaryDir m_tmp;
private slots:
    void initTestCase()
    {
        QVERIFY(m_tmp.isValid());
        QStandardPaths::setTestModeEnabled(true);
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_tmp.path());
    }
    void init()
    {
        // Seed a known baseline in the redirected QSettings before each test.
        QSettings s(QStringLiteral("MyCompany"), QStringLiteral("MyApp"));
        s.clear();
        s.setValue(QStringLiteral("school/name"), QStringLiteral("Acme Library"));
        s.setValue(QStringLiteral("school/address"), QStringLiteral("123 Main St"));
        s.setValue(QStringLiteral("admin/name"), QStringLiteral("J. Rizal"));
        s.setValue(QStringLiteral("admin/position"), QStringLiteral("Head Librarian"));
        s.setValue(QStringLiteral("library/openHour"), 8);
        s.setValue(QStringLiteral("library/closeHour"), 17);
        s.setValue(QStringLiteral("features/guestLogin"), true);
        s.sync();
    }

    void loadPopulatesPropertiesFromSettings();
    void loadStartsClean();
    void editingASettingMarksDirtyAndSignals();
    void resettingToSavedValueClearsDirty();
    void saveePersistsAllFieldsAndEmitsSaved();
    void saveMirrorsGuestFlagOntoKioskKey();
    void saveClearsDirty();
    void importLogoUpdatesPathAndEmitsLogoChanged();
    void importLogoAcceptsAFileUrl();
    void importBadPathLeavesLogoUnchanged();
    void importMissingFileSurfacesTheControllersReason();
    void importNonImageSurfacesTheControllersReason();
    void importLogoIsNotPersistedUntilSave();
    void reimportingADifferentImageChangesTheLogoUrl();
    void loadKeepsUnsavedEdits();
    void defaultManifestUrlIsALocalFileNamedForTheDepartment();
    void adminInfoSuccessEmitsSaved();
    void adminInfoErrorEmitsFailedWithMessage();
    void adminInfoAuthFailureEmitsAuthFailed();
    void keyChangeSuccessRefreshesSession();
    void keyChangeWrongOldKeyEmitsFailedAndKeepsSession();
    void departmentsParseFillsList();
    void departmentsErrorLeavesListEmpty();
    void departmentsErrorSurfacesStatusMessage();
    void departmentsAuthFailureEmitsAuthFailed();
    void departmentsNonJsonBodySurfacesStatus();
    void departmentsErrorKeepsThePreviouslyLoadedList();
    void serializeCsvQuotesSpecialCells();
    void resetVisitsEmptyDepartmentIsGuarded();
    void resetVisitsSuccessEmitsReset();
    void resetVisitsAuthFailureEmitsAuthFailed();
    void writeResetManifestContainsMetadataNotBackup();
    void http401AuthBodyReachesDecodeSeamNotNetworkError();
    void http200SuccessBodyStillReachesDecodeSeam();
    void transportFailureStillMeansNetworkError();
    void httpErrorWithNonJsonBodyFailsLoudlyNotSilently();
};

void TestSettingsViewModel::loadPopulatesPropertiesFromSettings()
{
    SettingsViewModel vm;
    vm.load();
    QCOMPARE(vm.schoolName(), QStringLiteral("Acme Library"));
    QCOMPARE(vm.schoolAddress(), QStringLiteral("123 Main St"));
    QCOMPARE(vm.adminName(), QStringLiteral("J. Rizal"));
    QCOMPARE(vm.adminPosition(), QStringLiteral("Head Librarian"));
    QCOMPARE(vm.openHour(), 8);
    QCOMPARE(vm.closeHour(), 17);
    QCOMPARE(vm.guestEnabled(), true);
}

void TestSettingsViewModel::loadStartsClean()
{
    SettingsViewModel vm;
    vm.load();
    QVERIFY(!vm.dirty());
}

void TestSettingsViewModel::editingASettingMarksDirtyAndSignals()
{
    SettingsViewModel vm;
    vm.load();
    QSignalSpy dirtySpy(&vm, &SettingsViewModel::dirtyChanged);
    vm.setSchoolName(QStringLiteral("New Name"));
    QVERIFY(vm.dirty());
    QVERIFY(dirtySpy.count() >= 1);
}

void TestSettingsViewModel::resettingToSavedValueClearsDirty()
{
    SettingsViewModel vm;
    vm.load();
    vm.setSchoolName(QStringLiteral("New Name"));
    QVERIFY(vm.dirty());
    vm.setSchoolName(QStringLiteral("Acme Library"));   // back to the loaded value
    QVERIFY(!vm.dirty());
}

void TestSettingsViewModel::saveePersistsAllFieldsAndEmitsSaved()
{
    SettingsViewModel vm;
    vm.load();
    vm.setSchoolName(QStringLiteral("Renamed Library"));
    vm.setOpenHour(7);
    QSignalSpy saved(&vm, &SettingsViewModel::saved);
    vm.save();
    QCOMPARE(saved.count(), 1);
    QSettings s(QStringLiteral("MyCompany"), QStringLiteral("MyApp"));
    QCOMPARE(s.value(QStringLiteral("school/name")).toString(), QStringLiteral("Renamed Library"));
    QCOMPARE(s.value(QStringLiteral("library/openHour")).toInt(), 7);
}

void TestSettingsViewModel::saveMirrorsGuestFlagOntoKioskKey()
{
    SettingsViewModel vm;
    vm.load();
    vm.setGuestEnabled(false);
    vm.save();
    QSettings s(QStringLiteral("MyCompany"), QStringLiteral("MyApp"));
    // Both the controller's key AND the key the kiosk reads must be set.
    QCOMPARE(s.value(QStringLiteral("features/guestLogin")).toBool(), false);
    QCOMPARE(s.value(QStringLiteral("kiosk/guestEnabled")).toBool(), false);

    vm.setGuestEnabled(true);
    vm.save();
    QSettings s2(QStringLiteral("MyCompany"), QStringLiteral("MyApp"));
    QCOMPARE(s2.value(QStringLiteral("features/guestLogin")).toBool(), true);
    QCOMPARE(s2.value(QStringLiteral("kiosk/guestEnabled")).toBool(), true);
}

void TestSettingsViewModel::saveClearsDirty()
{
    SettingsViewModel vm;
    vm.load();
    vm.setSchoolName(QStringLiteral("X"));
    QVERIFY(vm.dirty());
    vm.save();
    QVERIFY(!vm.dirty());
}

void TestSettingsViewModel::importLogoUpdatesPathAndEmitsLogoChanged()
{
    // Write a genuine 2x2 PNG into the temp dir so importImageFile accepts it.
    const QString src = m_tmp.path() + QStringLiteral("/src_logo.png");
    QImage img(2, 2, QImage::Format_ARGB32);
    img.fill(Qt::blue);
    QVERIFY(img.save(src, "PNG"));

    SettingsViewModel vm;
    vm.load();
    QSignalSpy logo(&vm, &SettingsViewModel::logoChanged);
    vm.importLogo(QUrl::fromLocalFile(src));

    QVERIFY(logo.count() >= 1);
    QVERIFY(!vm.logoPath().isEmpty());
    QVERIFY(vm.hasLogo());
    QVERIFY(vm.logoUrl().isValid());
    QVERIFY(vm.dirty());   // an import is an unsaved change
}

void TestSettingsViewModel::importLogoAcceptsAFileUrl()
{
    // FileDialog hands QML a "file:///C:/..." QUrl. The VM must convert it
    // itself — the controller bottoms out in QFile::exists()/QFile::copy(),
    // which fail on a "file://" string.
    const QString src = m_tmp.path() + QStringLiteral("/url_logo.png");
    QImage img(2, 2, QImage::Format_ARGB32);
    img.fill(Qt::red);
    QVERIFY(img.save(src, "PNG"));

    const QUrl url = QUrl::fromLocalFile(src);
    QVERIFY(url.toString().startsWith(QStringLiteral("file:///")));

    SettingsViewModel vm;
    vm.load();
    vm.importLogo(url);
    QVERIFY(vm.hasLogo());
    QVERIFY(!vm.logoPath().startsWith(QStringLiteral("file:")));
}

void TestSettingsViewModel::importBadPathLeavesLogoUnchanged()
{
    SettingsViewModel vm;
    vm.load();
    const QString before = vm.logoPath();
    vm.importLogo(QUrl::fromLocalFile(m_tmp.path() + QStringLiteral("/does_not_exist.png")));
    QCOMPARE(vm.logoPath(), before);   // unchanged
}

void TestSettingsViewModel::importMissingFileSurfacesTheControllersReason()
{
    // SettingsController::importImageFile emits importError with a PRECISE
    // reason. The VM must relay that reason instead of flattening every
    // failure into one generic string.
    SettingsViewModel vm;
    vm.load();
    vm.importLogo(QUrl::fromLocalFile(m_tmp.path() + QStringLiteral("/nope.png")));
    QVERIFY2(vm.statusMessage().contains(QStringLiteral("File not found")),
             qPrintable(vm.statusMessage()));
}

void TestSettingsViewModel::importNonImageSurfacesTheControllersReason()
{
    // A file that exists but is not decodable as an image takes the second
    // importError branch — a different reason, which must also survive.
    const QString bogus = m_tmp.path() + QStringLiteral("/not_an_image.png");
    QFile f(bogus);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("this is plain text, not a PNG");
    f.close();

    SettingsViewModel vm;
    vm.load();
    vm.importLogo(QUrl::fromLocalFile(bogus));
    QVERIFY2(vm.statusMessage().contains(QStringLiteral("Not a valid image")),
             qPrintable(vm.statusMessage()));
}

void TestSettingsViewModel::importLogoIsNotPersistedUntilSave()
{
    // An import is an UNSAVED edit (it marks the form dirty), so it must not
    // already be written to QSettings — otherwise "dirty" is a lie and there
    // is no way to abandon the change by navigating away.
    const QString src = m_tmp.path() + QStringLiteral("/gated_logo.png");
    QImage img(2, 2, QImage::Format_ARGB32);
    img.fill(Qt::green);
    QVERIFY(img.save(src, "PNG"));

    SettingsViewModel vm;
    vm.load();
    vm.importLogo(QUrl::fromLocalFile(src));
    QVERIFY(vm.dirty());
    QVERIFY(!vm.logoPath().isEmpty());

    {
        QSettings s(QStringLiteral("MyCompany"), QStringLiteral("MyApp"));
        QVERIFY2(s.value(QStringLiteral("school/logoPath")).toString().isEmpty(),
                 "importLogo() must not persist behind the Save gate");
    }

    vm.save();
    QVERIFY(!vm.dirty());
    QSettings s2(QStringLiteral("MyCompany"), QStringLiteral("MyApp"));
    QCOMPARE(s2.value(QStringLiteral("school/logoPath")).toString(), vm.logoPath());
}

void TestSettingsViewModel::reimportingADifferentImageChangesTheLogoUrl()
{
    // GUI-smoke regression: SettingsController names the destination after the
    // SOURCE basename ("logo_" + fileName), so two DIFFERENT images that happen
    // to share a filename land on the SAME destination path. logoPath is then
    // unchanged, so logoUrl is byte-identical, so QML's `source:` binding
    // re-evaluates to a value it already holds — Image never re-reads the file
    // and keeps painting the previously decoded (now stale) pixmap. The admin
    // sees the palette re-theme (which reads the file from disk) but the old
    // logo. logoUrl must therefore change whenever a new import lands, even
    // when the destination path does not.
    const QString dirA = m_tmp.path() + QStringLiteral("/a");
    const QString dirB = m_tmp.path() + QStringLiteral("/b");
    QVERIFY(QDir().mkpath(dirA));
    QVERIFY(QDir().mkpath(dirB));

    QImage blue(2, 2, QImage::Format_ARGB32);
    blue.fill(Qt::blue);
    QImage red(4, 4, QImage::Format_ARGB32);
    red.fill(Qt::red);
    QVERIFY(blue.save(dirA + QStringLiteral("/same_name.png"), "PNG"));
    QVERIFY(red.save(dirB + QStringLiteral("/same_name.png"), "PNG"));

    SettingsViewModel vm;
    vm.load();

    vm.importLogo(QUrl::fromLocalFile(dirA + QStringLiteral("/same_name.png")));
    const QString firstPath = vm.logoPath();
    const QUrl firstUrl = vm.logoUrl();
    QVERIFY(!firstPath.isEmpty());
    QVERIFY(firstUrl.isValid());

    QSignalSpy logo(&vm, &SettingsViewModel::logoChanged);
    vm.importLogo(QUrl::fromLocalFile(dirB + QStringLiteral("/same_name.png")));

    QCOMPARE(logo.count(), 1);
    // The collision itself is expected — the destination is basename-derived.
    QCOMPARE(vm.logoPath(), firstPath);
    // ...but the URL handed to QML must not be, or the preview goes stale.
    QVERIFY2(vm.logoUrl() != firstUrl,
             "logoUrl must change per import so Image re-reads the file");
    // The busting token must not corrupt the path QML/Image resolves to.
    QCOMPARE(vm.logoUrl().toLocalFile(), firstPath);
}

void TestSettingsViewModel::loadKeepsUnsavedEdits()
{
    // AdminScreen's Loader destroys/recreates SettingsScreen on every
    // navigation, so load() re-runs. It must not silently discard edits.
    SettingsViewModel vm;
    vm.load();
    vm.setSchoolName(QStringLiteral("Unsaved Draft"));
    QVERIFY(vm.dirty());

    vm.load();                                                       // re-entry
    QCOMPARE(vm.schoolName(), QStringLiteral("Unsaved Draft"));      // survives
    QVERIFY(vm.dirty());                                             // still dirty

    // A clean form still re-reads the baseline.
    vm.setSchoolName(QStringLiteral("Acme Library"));
    QVERIFY(!vm.dirty());
    vm.load();
    QCOMPARE(vm.schoolName(), QStringLiteral("Acme Library"));
}

void TestSettingsViewModel::defaultManifestUrlIsALocalFileNamedForTheDepartment()
{
    SettingsViewModel vm;
    const QUrl url = vm.defaultManifestUrl(QStringLiteral("CE"));
    QVERIFY(url.isLocalFile());
    const QString base = QFileInfo(url.toLocalFile()).fileName();
    QVERIFY(base.startsWith(QStringLiteral("Reset_Manifest_")));
    QVERIFY(base.contains(QStringLiteral("CE")));
    QVERIFY(base.endsWith(QStringLiteral(".csv")));
    // Never a "backup"/"export" — the manifest is operation metadata only.
    QVERIFY(!base.contains(QStringLiteral("Backup"), Qt::CaseInsensitive));
    QVERIFY(!base.contains(QStringLiteral("Export"), Qt::CaseInsensitive));
}

void TestSettingsViewModel::adminInfoSuccessEmitsSaved()
{
    SettingsViewModel vm;
    QSignalSpy ok(&vm, &SettingsViewModel::adminInfoSaved);
    vm.applyAdminInfoResponse(R"({"status":"success","message":"Admin info updated successfully."})");
    QCOMPARE(ok.count(), 1);
}

void TestSettingsViewModel::adminInfoErrorEmitsFailedWithMessage()
{
    SettingsViewModel vm;
    QSignalSpy bad(&vm, &SettingsViewModel::adminInfoFailed);
    vm.applyAdminInfoResponse(R"({"status":"error","message":"Failed to update admin info."})");
    QCOMPARE(bad.count(), 1);
    QCOMPARE(bad.at(0).at(0).toString(), QStringLiteral("Failed to update admin info."));
}

void TestSettingsViewModel::adminInfoAuthFailureEmitsAuthFailed()
{
    SettingsViewModel vm;
    QSignalSpy auth(&vm, &SettingsViewModel::authFailed);
    vm.applyAdminInfoResponse(R"({"status":"error","message":"Invalid admin key"})");
    QCOMPARE(auth.count(), 1);
}

void TestSettingsViewModel::keyChangeSuccessRefreshesSession()
{
    AdminSession::instance().setKey(QStringLiteral("OLDKEY"));
    SettingsViewModel vm;
    QSignalSpy ok(&vm, &SettingsViewModel::keyChanged);
    vm.applyKeyChangeResponse(R"({"status":"success","message":"Password updated successfully."})",
                              QStringLiteral("NEWKEY"));
    QCOMPARE(ok.count(), 1);
    QCOMPARE(AdminSession::instance().key(), QStringLiteral("NEWKEY"));
    AdminSession::instance().clear();   // isolate: don't leak into sibling tests
}

void TestSettingsViewModel::keyChangeWrongOldKeyEmitsFailedAndKeepsSession()
{
    AdminSession::instance().setKey(QStringLiteral("OLDKEY"));
    SettingsViewModel vm;
    QSignalSpy bad(&vm, &SettingsViewModel::keyChangeFailed);
    vm.applyKeyChangeResponse(R"({"status":"error","message":"Old password is incorrect."})",
                              QStringLiteral("NEWKEY"));
    QCOMPARE(bad.count(), 1);
    QCOMPARE(AdminSession::instance().key(), QStringLiteral("OLDKEY"));   // unchanged
    AdminSession::instance().clear();   // isolate: don't leak into sibling tests
}

void TestSettingsViewModel::departmentsParseFillsList()
{
    SettingsViewModel vm;
    QSignalSpy spy(&vm, &SettingsViewModel::departmentsChanged);
    vm.applyDepartmentsResponse(R"({"status":"success","departments":["CE","IT","BA"]})");
    QCOMPARE(spy.count(), 1);
    QCOMPARE(vm.departments(), (QStringList{"CE","IT","BA"}));
}

void TestSettingsViewModel::departmentsErrorLeavesListEmpty()
{
    SettingsViewModel vm;
    vm.applyDepartmentsResponse(R"({"status":"error","message":"No departments found"})");
    QVERIFY(vm.departments().isEmpty());
}

void TestSettingsViewModel::departmentsErrorSurfacesStatusMessage()
{
    // get_departments.php signals failure IN-BAND (HTTP 200 +
    // {"status":"error"}), so an error body must not be degraded to an empty
    // list in silence — that is indistinguishable from "no departments".
    SettingsViewModel vm;
    QSignalSpy changed(&vm, &SettingsViewModel::departmentsChanged);
    vm.applyDepartmentsResponse(R"({"status":"error","message":"No departments found"})");
    QCOMPARE(changed.count(), 0);
    QCOMPARE(vm.statusMessage(), QStringLiteral("No departments found"));
}

void TestSettingsViewModel::departmentsAuthFailureEmitsAuthFailed()
{
    // A 401 from requireAdminAuth carries a body, so it reaches this decode
    // seam. Settings is the one screen where auth state matters most — the
    // admin must be told the session is dead, not shown an empty picker.
    const QByteArray missingKey = R"({"status":"error","message":"Admin authentication required"})";
    const QByteArray badKey     = R"({"status":"error","message":"Invalid admin key"})";

    for (const QByteArray &body : {missingKey, badKey}) {
        SettingsViewModel vm;
        QSignalSpy auth(&vm, &SettingsViewModel::authFailed);
        QSignalSpy changed(&vm, &SettingsViewModel::departmentsChanged);
        vm.applyDepartmentsResponse(body);
        QCOMPARE(auth.count(), 1);
        QCOMPARE(changed.count(), 0);
        QVERIFY(vm.departments().isEmpty());
    }
}

void TestSettingsViewModel::departmentsNonJsonBodySurfacesStatus()
{
    // An HTML/PHP-fatal body is still the server answering; it must fail
    // visibly rather than leave a blank picker with no explanation.
    SettingsViewModel vm;
    QSignalSpy auth(&vm, &SettingsViewModel::authFailed);
    vm.applyDepartmentsResponse("<b>Fatal error</b>: something broke");
    QCOMPARE(auth.count(), 0);
    QVERIFY(!vm.statusMessage().isEmpty());
}

void TestSettingsViewModel::departmentsErrorKeepsThePreviouslyLoadedList()
{
    // A later failure must not silently wipe a picker that is already good.
    SettingsViewModel vm;
    vm.applyDepartmentsResponse(R"({"status":"success","departments":["CE","IT"]})");
    QCOMPARE(vm.departments(), (QStringList{"CE","IT"}));
    vm.applyDepartmentsResponse(R"({"status":"error","message":"Query failed"})");
    QCOMPARE(vm.departments(), (QStringList{"CE","IT"}));
    QCOMPARE(vm.statusMessage(), QStringLiteral("Query failed"));
}

void TestSettingsViewModel::serializeCsvQuotesSpecialCells()
{
    const QStringList headers{ "Name", "Note" };
    const QList<QStringList> rows{
        { "Ana Cruz",       "ok" },
        { "O'Hara, Liam",   "has \"quote\" and, comma" },
    };
    const QString csv = SettingsViewModel::serializeCsv(headers, rows);
    const QString expected =
        "Name,Note\r\n"
        "Ana Cruz,ok\r\n"
        "\"O'Hara, Liam\",\"has \"\"quote\"\" and, comma\"\r\n";
    QCOMPARE(csv, expected);
}

void TestSettingsViewModel::resetVisitsEmptyDepartmentIsGuarded()
{
    SettingsViewModel vm;
    QSignalSpy reset(&vm, &SettingsViewModel::visitsReset);
    QSignalSpy failed(&vm, &SettingsViewModel::resetFailed);
    vm.resetVisits(QString(), QStringLiteral("key"));   // empty dept -> no POST
    QCOMPARE(reset.count(), 0);
    QCOMPARE(failed.count(), 1);
    QVERIFY(!vm.busy());                                // never went busy
}

void TestSettingsViewModel::resetVisitsSuccessEmitsReset()
{
    SettingsViewModel vm;
    QSignalSpy reset(&vm, &SettingsViewModel::visitsReset);
    vm.applyResetVisitsResponse(R"({"status":"success","message":"Visit counts reset and visit history cleared for department: CE"})");
    QCOMPARE(reset.count(), 1);
}

void TestSettingsViewModel::resetVisitsAuthFailureEmitsAuthFailed()
{
    SettingsViewModel vm;
    QSignalSpy auth(&vm, &SettingsViewModel::authFailed);
    vm.applyResetVisitsResponse(R"({"status":"error","message":"Invalid admin key"})");
    QCOMPARE(auth.count(), 1);
}

void TestSettingsViewModel::writeResetManifestContainsMetadataNotBackup()
{
    SettingsViewModel vm;
    vm.load();
    vm.setProperty("adminName", QStringLiteral("Jane Librarian"));   // Operator
    const QString path = m_tmp.path() + QStringLiteral("/manifest.csv");
    QVERIFY(vm.writeResetManifest(QStringLiteral("CE"), QUrl::fromLocalFile(path)));

    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString csv = QString::fromUtf8(f.readAll());
    QVERIFY(csv.contains(QStringLiteral("Department")));
    QVERIFY(csv.contains(QStringLiteral("CE")));               // the reset department
    QVERIFY(csv.contains(QStringLiteral("Jane Librarian")));   // Operator
    QVERIFY(csv.contains(QStringLiteral("Reset requested")));
    QVERIFY(!csv.contains(QStringLiteral("Backup")));          // never labeled a backup
}

// --- 401-from-requireAdminAuth end-to-end (transport seam + decode seam) -----
// These compose the two network-free halves of the real path: the pure
// HttpForm::isServerAnswer classification that submit() applies, and the
// apply*Response decode seam it dispatches to. No live network is stood up.

void TestSettingsViewModel::http401AuthBodyReachesDecodeSeamNotNetworkError()
{
    // The exact bodies requireAdminAuth($conn) emits with http_response_code(401)
    // — see deliverables/loams_api/auth_helper.php. Qt reports the 401 as
    // QNetworkReply::ContentAccessDenied, i.e. reply->error() is set.
    const QByteArray missingKey = R"({"status":"error","message":"Admin authentication required"})";
    const QByteArray badKey     = R"({"status":"error","message":"Invalid admin key"})";

    for (const QByteArray &body : {missingKey, badKey}) {
        QVERIFY(HttpForm::isServerAnswer(/*replyHadError=*/true, 401, body));

        // reset_visits.php — the tier-2 irreversible path, where telling the
        // user "network error" for a mistyped key is worst.
        SettingsViewModel reset;
        QSignalSpy resetAuth(&reset, &SettingsViewModel::authFailed);
        QSignalSpy resetNet(&reset, &SettingsViewModel::networkError);
        reset.applyResetVisitsResponse(body);
        QCOMPARE(resetAuth.count(), 1);
        QCOMPARE(resetNet.count(), 0);

        // update_admin_info.php — same guard, same classification.
        SettingsViewModel info;
        QSignalSpy infoAuth(&info, &SettingsViewModel::authFailed);
        QSignalSpy infoNet(&info, &SettingsViewModel::networkError);
        info.applyAdminInfoResponse(body);
        QCOMPARE(infoAuth.count(), 1);
        QCOMPARE(infoNet.count(), 0);
    }
}

void TestSettingsViewModel::http200SuccessBodyStillReachesDecodeSeam()
{
    const QByteArray body = R"({"status":"success","message":"Visit counts reset and visit history cleared for department: CE"})";
    QVERIFY(HttpForm::isServerAnswer(/*replyHadError=*/false, 200, body));

    SettingsViewModel vm;
    QSignalSpy reset(&vm, &SettingsViewModel::visitsReset);
    QSignalSpy net(&vm, &SettingsViewModel::networkError);
    vm.applyResetVisitsResponse(body);
    QCOMPARE(reset.count(), 1);
    QCOMPARE(net.count(), 0);
}

void TestSettingsViewModel::transportFailureStillMeansNetworkError()
{
    // Backend unreachable: no status line, no body. The decode seam is never
    // reached — submit() takes the onNetworkError branch.
    QVERIFY(!HttpForm::isServerAnswer(/*replyHadError=*/true, 0, QByteArray()));
    QVERIFY(!HttpForm::isServerAnswer(/*replyHadError=*/true, 0, QByteArray("")));
}

void TestSettingsViewModel::httpErrorWithNonJsonBodyFailsLoudlyNotSilently()
{
    // An HTTP error carrying a non-JSON body (a PHP fatal, an HTML error page)
    // is still the server answering, so it reaches the decode seam — which
    // must surface a visible failure rather than doing nothing.
    const QByteArray html = "<b>Fatal error</b>: something broke";
    QVERIFY(HttpForm::isServerAnswer(/*replyHadError=*/true, 500, html));

    SettingsViewModel vm;
    QSignalSpy failed(&vm, &SettingsViewModel::resetFailed);
    QSignalSpy auth(&vm, &SettingsViewModel::authFailed);
    QSignalSpy reset(&vm, &SettingsViewModel::visitsReset);
    vm.applyResetVisitsResponse(html);
    QCOMPARE(reset.count(), 0);
    QCOMPARE(auth.count(), 0);
    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.at(0).at(0).toString(), QStringLiteral("Reset failed."));
    QVERIFY(!vm.busy());   // the busy spinner is always cleared
}

QTEST_MAIN(TestSettingsViewModel)
#include "tst_settingsviewmodel.moc"
