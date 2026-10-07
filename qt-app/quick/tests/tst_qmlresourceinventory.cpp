#include <QtTest>
#include <QBuffer>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QImage>
#include <QNetworkProxy>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QStandardPaths>
#include <memory>

#include "RecordingPolicyNam.h"
#include "SchoolInfoUtil.h"
#include "SearchResultsModel.h"
#include "SettingsViewModel.h"
#include "apiconfig.h"
#include "appsettings.h"
#include "loginparser.h"
#include "offlinehttp.h"
#include "studentdata.h"
#include "tinyhttpserver.h"
#include "transport/policyenforcingnam.h"

// S1a resource inventory (S1 spec §3 "Resource inventory": every resource
// actually loaded becomes a test). Each row of the plan's inventory table is
// loaded exactly as production does — URL built by the production C++ code,
// rendered by the production QML component — on an engine whose managers come
// from a (recording) PolicyNamFactory installed via installOn(), against the
// in-process loopback TinyHttpServer (127.0.0.1, ephemeral port; never XAMPP).
// Only R2, R3, R4 and R6 prove the seam (the request appears in the request log
// of a factory-made PolicyEnforcingNam). R1, R5, R7, R8 and R10 are regression /
// scope guards: qrc compile integrity, sentinel/empty logic, local file: assets
// (for a future S1e scheme policy) and the poster source tripwire.
namespace {
constexpr int kImageReady = 1;   // QQuickImageBase::Ready (QML Image.Ready)
constexpr int kImageError = 3;   // QQuickImageBase::Error (QML Image.Error)

QByteArray pngBytes()
{
    QImage img(4, 4, QImage::Format_ARGB32);
    img.fill(Qt::blue);
    QByteArray bytes;
    QBuffer buf(&bytes);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    return bytes;
}

QString qmlString(const QString &s)
{
    QString escaped = s;
    escaped.replace(QLatin1String("\\"), QLatin1String("\\\\"));
    escaped.replace(QLatin1String("\""), QLatin1String("\\\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

QString writePng(const QString &fileName)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);   // test-mode AppDataLocation (AppSettings::isolateForTesting)
    const QString path = dir + QLatin1Char('/') + fileName;
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(pngBytes());
    return path;
}
} // namespace

class TestQmlResourceInventory : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanup();
    void bundledModuleQmlLoadsFromQrc();                      // R1 (module lookup)
    void everyBundledQmlFileCompilesFromQrc();                // R1 (all 43 files)
    void turnstilePhotoLoadsThroughFactory();                 // R2
    void loginPhotoUrlLoadsThroughFactory();                  // R3
    void searchAvatarLoadsThroughFactory();                   // R4
    void defaultSentinelAndEmptyPhotoNeverRequested();        // R5
    void missingRemotePhotoFallsBackToInitials();             // R6
    void importedLogoFileUrlLoads();                          // R7
    void settingsLogoPreviewLoads();                          // R8
    void posterIsNotDisplayedByAnyQml();                      // R10

private:
    std::unique_ptr<QQuickView> makeView();
    static QQuickItem *create(QQuickView &view, const QString &qml);
    void expectRemoteAvatarLoads(const QString &photoUrl, const QByteArray &path);

    TinyHttpServer m_server;
    RequestLog m_log;                               // every request through a factory-made manager
    RecordingPolicyNamFactory m_factory{&m_log};    // outlives every view/engine created below
};

void TestQmlResourceInventory::initTestCase()
{
    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
    QVERIFY(m_server.listen());
    TinyHttpServer::Response png;
    png.headers = {{"Content-Type", "image/png"}};
    png.body = pngBytes();
    m_server.route("/loams_api/uploads/students/s1.png", png);
    m_server.route("/loams_api/uploads/students/s2.png", png);
    m_server.route("/loams_api/uploads/students/s3.png", png);
    // Routed on purpose: R5 proves the sentinel is never REQUESTED, not that it 404s.
    m_server.route("/loams_api/uploads/default.jpg", png);
}

void TestQmlResourceInventory::cleanup()
{
    ApiConfig::resetBaseUrl();
}

std::unique_ptr<QQuickView> TestQmlResourceInventory::makeView()
{
    auto view = std::make_unique<QQuickView>();
    if (!PolicyNamFactory::installOn(*view->engine(), m_factory))   // before any manager exists
        qFatal("inventory: transport seam could not be installed on a fresh engine");
    view->engine()->addImportPath(QStringLiteral("qrc:/qt/qml"));
    view->resize(200, 200);
    view->show();
    return view;
}

QQuickItem *TestQmlResourceInventory::create(QQuickView &view, const QString &qml)
{
    QQmlComponent component(view.engine());
    component.setData(qml.toUtf8(), QUrl());
    if (!component.isReady()) {
        qWarning() << component.errors();
        return nullptr;
    }
    auto *item = qobject_cast<QQuickItem *>(component.create());
    if (!item)
        return nullptr;
    item->setParent(view.contentItem());       // freed with the view
    item->setParentItem(view.contentItem());
    return item;
}

void TestQmlResourceInventory::expectRemoteAvatarLoads(const QString &photoUrl,
                                                       const QByteArray &path)
{
    auto view = makeView();
    // The engine's own manager is a seam manager...
    QVERIFY(qobject_cast<PolicyEnforcingNam *>(view->engine()->networkAccessManager()));
    QQuickItem *avatar = create(*view, QStringLiteral(
        "import QtQuick\nimport LOAMS\nLAvatar { size: 40; initials: \"TA\"; source: %1 }")
        .arg(qmlString(photoUrl)));
    QVERIFY(avatar);
    QTRY_COMPARE_WITH_TIMEOUT(avatar->property("imageStatus").toInt(), kImageReady, 5000);
    QVERIFY(!avatar->property("showInitials").toBool());
    QVERIFY(m_server.countFor(path) >= 1);
    // ...and the photo request itself passed through a factory-made
    // PolicyEnforcingNam (whichever thread issued it).
    QVERIFY2(m_log.contains(QUrl(photoUrl)),
             qPrintable(QStringLiteral("not fetched through the seam: ") + photoUrl));
}

void TestQmlResourceInventory::bundledModuleQmlLoadsFromQrc()
{
    auto view = makeView();
    QQmlComponent component(view->engine());
    component.loadFromModule("LOAMS", "LAvatar");
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QCOMPARE(component.url().scheme(), QStringLiteral("qrc"));
}

void TestQmlResourceInventory::everyBundledQmlFileCompilesFromQrc()
{
    // R1 per file: every .qml in the source tree (quick/qml — the same 43 files
    // listed in qt_add_qml_module QML_FILES) is loaded from the module's qrc
    // copy and compiled by a seam-installed engine. Compile/load only:
    // instantiating whole screens (e.g. AdminScreen with autoLoad) could start
    // requests; live instantiation stays covered by tst_appshell (AppShell) and
    // the tst_qml_* suites. A source file missing from the module fails here.
    QVERIFY2(QFile::exists(QStringLiteral(":/qt/qml/LOAMS/qml/AppShell.qml")),
             "module resources are not under :/qt/qml/LOAMS/ — check qt_add_qml_module's resource prefix");
    auto view = makeView();
    const QDir srcRoot(QStringLiteral(SRC_QML_DIR));
    QDirIterator it(srcRoot.path(), {QStringLiteral("*.qml")}, QDir::Files,
                    QDirIterator::Subdirectories);
    QStringList failures;
    int compiled = 0;
    while (it.hasNext()) {
        const QString rel = srcRoot.relativeFilePath(it.next());   // e.g. components/LAvatar.qml
        QQmlComponent component(view->engine(),
                                QUrl(QStringLiteral("qrc:/qt/qml/LOAMS/qml/") + rel));
        if (component.isReady())
            ++compiled;
        else
            failures << rel + QStringLiteral(": ") + component.errorString();
    }
    QVERIFY2(failures.isEmpty(), qPrintable(failures.join(QLatin1Char('\n'))));
    QCOMPARE(compiled, 43);   // update together with QML_FILES in quick/CMakeLists.txt
}

void TestQmlResourceInventory::turnstilePhotoLoadsThroughFactory()
{
    const QUrl base = m_server.url(QStringLiteral("/loams_api/"));
    const QByteArray payload =
        "{\"status\":\"success\",\"latest_id\":4,\"entry\":{\"id\":4,\"card\":\"C\","
        "\"created_at\":\"2026-09-29 08:30:00\",\"reader\":0,"
        "\"student\":{\"name\":\"Test Student A\",\"photo_path\":\"uploads/students/s1.png\"}}}";
    const LoginParser::EntryEventResult r = LoginParser::parseEntryEvent(payload, base);
    QVERIFY(r.valid);
    QVERIFY(r.hasStudent);
    const QString photo = r.student.value(QStringLiteral("photo_url")).toString();
    QCOMPARE(photo, m_server.url(QStringLiteral("/loams_api/uploads/students/s1.png")).toString());
    expectRemoteAvatarLoads(photo, "/loams_api/uploads/students/s1.png");
}

void TestQmlResourceInventory::loginPhotoUrlLoadsThroughFactory()
{
    const QString serverPhoto =
        m_server.url(QStringLiteral("/loams_api/uploads/students/s2.png")).toString();
    const QByteArray body = QStringLiteral(
        "{\"status\":\"success\",\"student\":{\"name\":\"Test Student B\",\"photo_url\":\"%1\"}}")
        .arg(serverPhoto).toUtf8();
    const LoginParser::RfidResult r = LoginParser::parseRfidResponse(body);
    QVERIFY(r.ok);
    const QString photo = r.student.value(QStringLiteral("photo_url")).toString();
    QCOMPARE(photo, serverPhoto);   // KioskViewModel::applyStudentLogin binds this verbatim
    expectRemoteAvatarLoads(photo, "/loams_api/uploads/students/s2.png");
}

void TestQmlResourceInventory::searchAvatarLoadsThroughFactory()
{
    QVERIFY(ApiConfig::setBaseUrl(m_server.url(QStringLiteral("/loams_api/")).toString()));
    SearchResultsModel model;
    StudentRecord rec;
    rec.schoolId = QStringLiteral("TEST-0001");
    rec.name = QStringLiteral("Test Student C");
    rec.photo = QStringLiteral("uploads/students/s3.png");
    model.setRecords({rec});
    const QString photo = model.data(model.index(0), SearchResultsModel::PhotoRole).toString();
    QCOMPARE(photo, m_server.url(QStringLiteral("/loams_api/uploads/students/s3.png")).toString());
    expectRemoteAvatarLoads(photo, "/loams_api/uploads/students/s3.png");
}

void TestQmlResourceInventory::defaultSentinelAndEmptyPhotoNeverRequested()
{
    auto view = makeView();
    const QString sentinel = m_server.url(QStringLiteral("/loams_api/uploads/default.jpg")).toString();
    QQuickItem *withSentinel = create(*view, QStringLiteral(
        "import QtQuick\nimport LOAMS\nLAvatar { initials: \"TD\"; source: %1 }").arg(qmlString(sentinel)));
    QQuickItem *empty = create(*view, QStringLiteral(
        "import QtQuick\nimport LOAMS\nLAvatar { initials: \"TE\"; source: \"\" }"));
    QVERIFY(withSentinel);
    QVERIFY(empty);
    QVERIFY(withSentinel->property("showInitials").toBool());
    QVERIFY(empty->property("showInitials").toBool());
    QTest::qWait(300);   // negative assertion: give a (wrong) load time to reach the server
    QCOMPARE(m_server.countFor("/loams_api/uploads/default.jpg"), 0);
}

void TestQmlResourceInventory::missingRemotePhotoFallsBackToInitials()
{
    auto view = makeView();
    QVERIFY(qobject_cast<PolicyEnforcingNam *>(view->engine()->networkAccessManager()));
    const QString missing = m_server.url(QStringLiteral("/loams_api/uploads/students/missing.png")).toString();
    QQuickItem *avatar = create(*view, QStringLiteral(
        "import QtQuick\nimport LOAMS\nLAvatar { initials: \"TM\"; source: %1 }").arg(qmlString(missing)));
    QVERIFY(avatar);
    QTRY_COMPARE_WITH_TIMEOUT(avatar->property("imageStatus").toInt(), kImageError, 5000);
    QVERIFY(avatar->property("showInitials").toBool());
    QVERIFY(m_server.countFor("/loams_api/uploads/students/missing.png") >= 1);
    // The request (which ended in a 404) was logged by a factory-made manager.
    QVERIFY2(m_log.contains(QUrl(missing)),
             qPrintable(QStringLiteral("not fetched through the seam: ") + missing));
}

void TestQmlResourceInventory::importedLogoFileUrlLoads()
{
    const QString path = writePng(QStringLiteral("logo_inventory.png"));
    bool hasLogo = false;
    const QUrl logo = SchoolInfoUtil::resolveLogoUrl(path, &hasLogo);
    QVERIFY(hasLogo);
    QVERIFY(logo.isLocalFile());

    const qsizetype requestsBefore = m_server.requests().size();
    auto view = makeView();
    QQuickItem *badge = create(*view, QStringLiteral(
        "import QtQuick\nimport LOAMS\nLLogoCircle { size: 52; hasLogo: true; logoUrl: %1 }")
        .arg(qmlString(logo.toString())));
    QVERIFY(badge);
    QObject *image = badge->findChild<QObject *>(QStringLiteral("logoImage"));
    QVERIFY(image);
    QTRY_COMPARE_WITH_TIMEOUT(image->property("status").toInt(), kImageReady, 5000);
    QCOMPARE(m_server.requests().size(), requestsBefore);   // local asset: no network
}

void TestQmlResourceInventory::settingsLogoPreviewLoads()
{
    const QString path = writePng(QStringLiteral("logo_settings_inventory.png"));
    {
        AppSettings s;
        s.setValue(QStringLiteral("school/logoPath"), path);
        s.sync();
    }
    OfflineHttp vmHttp;                          // seam owner in a test: never a live manager
    SettingsViewModel vm(nullptr, &vmHttp.http);
    vm.load();
    const QUrl url = vm.logoUrl();
    QVERIFY(url.isLocalFile());

    auto view = makeView();
    QQuickItem *preview = create(*view, QStringLiteral(
        "import QtQuick\nImage { width: 48; height: 48; fillMode: Image.PreserveAspectFit; source: %1 }")
        .arg(qmlString(url.toString())));
    QVERIFY(preview);
    QTRY_COMPARE_WITH_TIMEOUT(preview->property("status").toInt(), kImageReady, 5000);

    AppSettings s;
    s.remove(QStringLiteral("school/logoPath"));
}

void TestQmlResourceInventory::posterIsNotDisplayedByAnyQml()
{
    // Inventory row R10: imported posters are stored (school/posterPath) but no
    // 2.0 QML displays them. If a screen starts showing one, this fails so the
    // poster gets its own load test here.
    QDirIterator it(QStringLiteral(SRC_QML_DIR), {QStringLiteral("*.qml")},
                    QDir::Files, QDirIterator::Subdirectories);
    QStringList hits;
    int scanned = 0;
    while (it.hasNext()) {
        const QString file = it.next();
        QFile f(file);
        QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(file));
        ++scanned;
        if (QString::fromUtf8(f.readAll()).contains(QLatin1String("poster"), Qt::CaseInsensitive))
            hits << file;
    }
    QVERIFY2(scanned > 20, "scan found too few QML files — is SRC_QML_DIR wrong?");
    QVERIFY2(hits.isEmpty(), qPrintable(hits.join(QStringLiteral(", "))));
}

QTEST_MAIN(TestQmlResourceInventory)
#include "tst_qmlresourceinventory.moc"
