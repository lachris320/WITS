#include <QtTest>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QStringList>
#include <algorithm>

// S1a transport-seam source guard (S1 spec §3 "Injection boundary" and
// "SSL errors are never ignored ... a source grep check enforces this").
//
// 1. No QNetworkAccessManager is constructed (heap, make_unique/shared, stack
//    or by-value member) or subclassed in production code under qt-app/core
//    or qt-app/quick outside the seam: owners use HttpClient (C++) or
//    PolicyNamFactory (QML). Exempt: quick/tests/, qt-app/tests/ and
//    qt-app/testsupport/ (test doubles); the frozen legacy Widgets sources
//    (qt-app root) are not scanned.
// 2. Every core network class asserts a non-null injected manager (its raw
//    pointer constructor is a test seam; production passes HttpClient::manager()).
// 3. The SSL-error bypass call never appears anywhere in client source.
//
// Matching is on comment-stripped code (approximate: a "//" inside a string
// literal truncates that line, which can only hide code written after a URL).
//
// kPendingMigration lists owners not yet moved onto the seam. Each migration
// task deletes its entries; a listed file that no longer offends FAILS as
// stale, so the list can only shrink. It is empty once S1a is complete.
namespace {
const QStringList kPendingMigration = {
    QStringLiteral("quick/AccessControlHub.cpp"),                    // Task 8
    QStringLiteral("quick/viewmodels/AccessControlViewModel.cpp"),   // Task 9
    QStringLiteral("quick/viewmodels/DashboardViewModel.cpp"),       // Task 9
    QStringLiteral("quick/viewmodels/VisitLogsViewModel.cpp"),       // Task 9
    QStringLiteral("quick/viewmodels/GuestViewModel.cpp"),           // Task 10
    QStringLiteral("quick/viewmodels/KioskViewModel.cpp"),           // Task 10
    QStringLiteral("quick/viewmodels/DatabaseViewModel.cpp"),        // Task 11
    QStringLiteral("quick/viewmodels/SearchViewModel.cpp"),          // Task 11
    QStringLiteral("quick/viewmodels/ImportViewModel.cpp"),          // Task 12
    QStringLiteral("quick/viewmodels/ReportingViewModel.cpp"),       // Task 12
    QStringLiteral("quick/viewmodels/SettingsViewModel.cpp"),        // Task 12
};

const QRegularExpression kNamConstruction(QStringLiteral(
    "\\bnew\\s+QNetworkAccessManager\\b"
    "|\\bmake_(?:unique|shared)\\s*<\\s*QNetworkAccessManager\\s*>"
    "|\\bQNetworkAccessManager\\s+[A-Za-z_]\\w*\\s*[;({=]"));
const QRegularExpression kNamSubclass(QStringLiteral(":\\s*public\\s+QNetworkAccessManager\\b"));
const QString kSeamSubclassHeader = QStringLiteral("core/transport/policyenforcingnam.h");

// Core classes that hold an injected manager (Task 4b contract).
const QStringList kCoreNetworkClasses = {
    QStringLiteral("core/studentcontroller.cpp"),
    QStringLiteral("core/visitorcontroller.cpp"),
    QStringLiteral("core/reportcontroller.cpp"),
    QStringLiteral("core/importcontroller.cpp"),
    QStringLiteral("core/brandingcontroller.cpp"),
    QStringLiteral("core/accesscontrol/accessdecisionservice.cpp"),
    QStringLiteral("core/accesscontrol/turnstileprovider.cpp"),
};

// Test files that still construct a seam owner (network-owning ViewModel or
// AccessControlHub) WITHOUT an injected fake manager. Tasks 8-12 delete their
// entries; empty once S1a is complete. A listed file that no longer offends
// FAILS as stale.
const QStringList kPendingTestMigration = {
    QStringLiteral("quick/tests/tst_appshell.cpp"),               // Task 8 (needs the hub's HttpClient ctor)
    QStringLiteral("quick/tests/tst_accesscontrolhub.cpp"),       // Task 8
    QStringLiteral("quick/tests/tst_accesscontrolviewmodel.cpp"), // Task 9
    QStringLiteral("quick/tests/tst_dashboardviewmodel.cpp"),     // Task 9
    QStringLiteral("quick/tests/tst_visitlogsviewmodel.cpp"),     // Task 9
    QStringLiteral("quick/tests/tst_guestviewmodel.cpp"),         // Task 10
    QStringLiteral("quick/tests/tst_kioskviewmodel.cpp"),         // Task 10
    QStringLiteral("quick/tests/tst_databaseviewmodel.cpp"),      // Task 11
    QStringLiteral("quick/tests/tst_searchviewmodel.cpp"),        // Task 11
    QStringLiteral("quick/tests/tst_importviewmodel.cpp"),        // Task 12
    QStringLiteral("quick/tests/tst_reportingviewmodel.cpp"),     // Task 12
    QStringLiteral("quick/tests/tst_settingsviewmodel.cpp"),      // Task 12
};

const QString kOwnerTypes = QStringLiteral(
    "(?:(?:Dashboard|VisitLogs|AccessControl|Kiosk|Guest|Search|Database|Import|Reporting|Settings)"
    "ViewModel|AccessControlHub)");
} // namespace

class TestTransportSeamGuard : public QObject
{
    Q_OBJECT
private slots:
    void namConstructedOnlyInsideSeam();
    void coreNetworkClassesRequireInjectedManager();
    void noSslErrorBypassInClientSource();
    void ownersInTestsUseFakeManagers();
    void quickMainInstallsTransportBeforeLoad();

private:
    static QStringList sourceFiles(const QString &subdir, const QStringList &globs,
                                   const QStringList &skipPrefixes);
    static QString stripComments(const QString &src);
    static QString readSource(const QString &rel, bool *ok);
};

QStringList TestTransportSeamGuard::sourceFiles(const QString &subdir, const QStringList &globs,
                                                const QStringList &skipPrefixes)
{
    const QDir root(QStringLiteral(SRC_ROOT));
    QStringList out;
    QDirIterator it(subdir.isEmpty() ? root.path() : root.filePath(subdir), globs,
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString rel = root.relativeFilePath(it.next());
        const QStringList segments = rel.split(QLatin1Char('/'));
        const bool inBuildDir = std::any_of(segments.cbegin(), segments.cend() - 1,
            [](const QString &s) { return s.startsWith(QLatin1String("build")); });
        const bool skipped = std::any_of(skipPrefixes.cbegin(), skipPrefixes.cend(),
            [&rel](const QString &p) { return rel.startsWith(p); });
        if (!inBuildDir && !skipped)
            out << rel;
    }
    out.sort();
    return out;
}

QString TestTransportSeamGuard::stripComments(const QString &src)
{
    QString out = src;
    out.remove(QRegularExpression(QStringLiteral("/\\*.*?\\*/"),
                                  QRegularExpression::DotMatchesEverythingOption));
    out.remove(QRegularExpression(QStringLiteral("//[^\\n]*")));
    return out;
}

QString TestTransportSeamGuard::readSource(const QString &rel, bool *ok)
{
    QFile f(QDir(QStringLiteral(SRC_ROOT)).filePath(rel));
    *ok = f.open(QIODevice::ReadOnly);
    return *ok ? QString::fromUtf8(f.readAll()) : QString();
}

void TestTransportSeamGuard::namConstructedOnlyInsideSeam()
{
    const QStringList globs = {QStringLiteral("*.cpp"), QStringLiteral("*.h")};
    const QStringList files = sourceFiles(QStringLiteral("core"), globs, {})
        + sourceFiles(QStringLiteral("quick"), globs, {QStringLiteral("quick/tests/")});
    QVERIFY2(files.size() > 50, "scan found too few files — is SRC_ROOT wrong?");

    QStringList offenders;
    for (const QString &rel : files) {
        bool ok = false;
        const QString code = stripComments(readSource(rel, &ok));
        QVERIFY2(ok, qPrintable(QStringLiteral("cannot read ") + rel));
        const bool constructs = kNamConstruction.match(code).hasMatch();
        const bool subclasses = rel != kSeamSubclassHeader && kNamSubclass.match(code).hasMatch();
        if (constructs || subclasses)
            offenders << rel;
    }

    QStringList unexpected;
    QStringList stale;
    for (const QString &o : offenders) {
        if (!kPendingMigration.contains(o))
            unexpected << o;
    }
    for (const QString &p : kPendingMigration) {
        if (!offenders.contains(p))
            stale << p;
    }
    QVERIFY2(unexpected.isEmpty(), qPrintable(
        QStringLiteral("QNetworkAccessManager constructed/subclassed outside the transport "
                       "seam (use HttpClient / PolicyNamFactory): ") + unexpected.join(QStringLiteral(", "))));
    QVERIFY2(stale.isEmpty(), qPrintable(
        QStringLiteral("Stale kPendingMigration entries (already migrated — delete them): ")
        + stale.join(QStringLiteral(", "))));
}

void TestTransportSeamGuard::coreNetworkClassesRequireInjectedManager()
{
    // Every core class that stores a QNetworkAccessManager* must (a) be listed
    // here, so a new one cannot slip in unchecked, and (b) assert non-null in
    // its constructor (Task 4b contract).
    const QStringList files = sourceFiles(QStringLiteral("core"), {QStringLiteral("*.cpp")}, {});
    const QRegularExpression storesManager(QStringLiteral("\\bm_nam\\s*\\(\\s*nam\\s*\\)"));
    QStringList unlisted;
    QStringList missingAssert;
    for (const QString &rel : files) {
        bool ok = false;
        const QString code = stripComments(readSource(rel, &ok));
        QVERIFY2(ok, qPrintable(QStringLiteral("cannot read ") + rel));
        if (!storesManager.match(code).hasMatch())
            continue;
        if (!kCoreNetworkClasses.contains(rel))
            unlisted << rel;
        if (!code.contains(QLatin1String("Q_ASSERT_X(nam,")))
            missingAssert << rel;
    }
    for (const QString &rel : kCoreNetworkClasses) {
        bool ok = false;
        const QString code = stripComments(readSource(rel, &ok));
        QVERIFY2(ok, qPrintable(QStringLiteral("cannot read ") + rel));
        if (!code.contains(QLatin1String("Q_ASSERT_X(nam,")) && !missingAssert.contains(rel))
            missingAssert << rel;
    }
    QVERIFY2(unlisted.isEmpty(), qPrintable(
        QStringLiteral("core class stores an injected manager but is not in kCoreNetworkClasses: ")
        + unlisted.join(QStringLiteral(", "))));
    QVERIFY2(missingAssert.isEmpty(), qPrintable(
        QStringLiteral("core network class lacks Q_ASSERT_X(nam, ...) non-null contract: ")
        + missingAssert.join(QStringLiteral(", "))));
}

void TestTransportSeamGuard::noSslErrorBypassInClientSource()
{
    // Needle assembled at runtime so this file never matches itself.
    const QString needle = QStringLiteral("ignoreSsl") + QStringLiteral("Errors");
    const QStringList files = sourceFiles(QString(),
        {QStringLiteral("*.cpp"), QStringLiteral("*.h"), QStringLiteral("*.qml"), QStringLiteral("*.js")},
        {QStringLiteral("libs/")});
    QVERIFY2(files.size() > 50, "scan found too few files — is SRC_ROOT wrong?");

    QStringList hits;
    for (const QString &rel : files) {
        bool ok = false;
        const QString raw = readSource(rel, &ok);           // raw: comments count too
        QVERIFY2(ok, qPrintable(QStringLiteral("cannot read ") + rel));
        if (raw.contains(needle))
            hits << rel;
    }
    QVERIFY2(hits.isEmpty(), qPrintable(
        QStringLiteral("SSL errors must never be ignored (spec §3): ") + hits.join(QStringLiteral(", "))));
}

void TestTransportSeamGuard::ownersInTestsUseFakeManagers()
{
    // A seam owner constructed in a test with no HttpClient argument builds a
    // real manager aimed at the configured backend (default
    // http://localhost/loams_api/) and could mutate a running XAMPP. Every test
    // must pass an HttpClient over CapturingNam / SequencedNam / OfflineHttp.
    // Exempt: functions named defaultConstruction* / defaultHub*, which only
    // assert seam ownership and issue no request.
    const QStringList files = sourceFiles(QStringLiteral("quick/tests"), {QStringLiteral("tst_*.cpp")}, {});
    QVERIFY2(files.size() > 20, "scan found too few test files — is SRC_ROOT wrong?");
    const QRegularExpression header(QStringLiteral("^void\\s+\\w+::(\\w+)\\s*\\("),
                                    QRegularExpression::MultilineOption);
    const QRegularExpression stackDefault(
        QStringLiteral("^[ \\t]*(?:static\\s+)?") + kOwnerTypes
            + QStringLiteral("\\s+\\w+\\s*(?:;|\\{\\s*\\}|\\([^,()]*\\))"),
        QRegularExpression::MultilineOption);
    const QRegularExpression heapDefault(
        QStringLiteral("\\b(?:new\\s+|make_unique\\s*<\\s*)") + kOwnerTypes
            + QStringLiteral("\\s*>?\\s*\\([^,()]*\\)"));

    QStringList offenders;
    for (const QString &rel : files) {
        bool ok = false;
        const QString code = stripComments(readSource(rel, &ok));
        QVERIFY2(ok, qPrintable(QStringLiteral("cannot read ") + rel));
        QList<QPair<qsizetype, QString>> functions;   // (start offset, name), in order
        for (auto it = header.globalMatch(code); it.hasNext();) {
            const QRegularExpressionMatch m = it.next();
            functions.append({m.capturedStart(), m.captured(1)});
        }
        auto enclosing = [&functions](qsizetype pos) {
            QString name;
            for (const auto &f : functions) {
                if (f.first > pos)
                    break;
                name = f.second;
            }
            return name;
        };
        bool offends = false;
        for (const QRegularExpression *re : {&stackDefault, &heapDefault}) {
            for (auto it = re->globalMatch(code); it.hasNext();) {
                const QString fn = enclosing(it.next().capturedStart());
                if (!fn.startsWith(QLatin1String("defaultConstruction"))
                    && !fn.startsWith(QLatin1String("defaultHub")))
                    offends = true;
            }
        }
        if (offends)
            offenders << rel;
    }

    QStringList unexpected;
    QStringList stale;
    for (const QString &o : offenders) {
        if (!kPendingTestMigration.contains(o))
            unexpected << o;
    }
    for (const QString &p : kPendingTestMigration) {
        if (!offenders.contains(p))
            stale << p;
    }
    QVERIFY2(unexpected.isEmpty(), qPrintable(
        QStringLiteral("test constructs a seam owner without a fake manager (inject "
                       "OfflineHttp / CapturingNam / SequencedNam via HttpClient): ")
        + unexpected.join(QStringLiteral(", "))));
    QVERIFY2(stale.isEmpty(), qPrintable(
        QStringLiteral("Stale kPendingTestMigration entries (already migrated — delete them): ")
        + stale.join(QStringLiteral(", "))));
}

void TestTransportSeamGuard::quickMainInstallsTransportBeforeLoad()
{
    bool ok = false;
    const QString code = stripComments(readSource(QStringLiteral("quick/main.cpp"), &ok));
    QVERIFY2(ok, "cannot read quick/main.cpp");
    const qsizetype policy  = code.indexOf(QLatin1String("TransportPolicy::setCurrent("));
    const qsizetype hub     = code.indexOf(QLatin1String("AccessControlHub accessControl"));
    const qsizetype factory = code.indexOf(QLatin1String("PolicyNamFactory namFactory"));
    const qsizetype engine  = code.indexOf(QLatin1String("QQmlApplicationEngine engine"));
    const qsizetype install = code.indexOf(QLatin1String("PolicyNamFactory::installOn(engine, namFactory)"));
    QVERIFY2(!code.contains(QLatin1String("setNetworkAccessManagerFactory(")),
             "install the factory only via PolicyNamFactory::installOn (it refuses a late install)");
    const qsizetype load    = code.indexOf(QLatin1String("engine.loadFromModule("));
    QVERIFY2(policy >= 0 && hub >= 0 && policy < hub,
             "the transport policy must be published before AccessControlHub builds its HttpClient");
    QVERIFY2(factory >= 0 && engine >= 0 && factory < engine,
             "PolicyNamFactory must be declared before (so it outlives) the engine");
    QVERIFY2(install > engine && load > install,
             "the factory must be installed on the engine before the first QML load");
}

QTEST_APPLESS_MAIN(TestTransportSeamGuard)
#include "tst_transportseamguard.moc"
