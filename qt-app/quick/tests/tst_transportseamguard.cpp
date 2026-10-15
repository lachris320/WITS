#include <QtTest>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QStringList>
#include <algorithm>

#include "sourcescan.h"

// S1a transport-seam source guard (S1 spec §3 "Injection boundary" and
// "SSL errors are never ignored ... a source grep check enforces this").
//
// 1. No QNetworkAccessManager is constructed (heap, make_unique/shared, stack,
//    by-value member, brace/value-initialized temporary) or subclassed (any
//    access specifier, any base position) in production code under qt-app/core
//    or qt-app/quick outside the seam: owners use HttpClient (C++) or
//    PolicyNamFactory (QML). Exempt: quick/tests/, qt-app/tests/ and
//    qt-app/testsupport/ (test doubles); the frozen legacy Widgets sources
//    (qt-app root) are not scanned.
// 2. Every core network class asserts a non-null injected manager (its raw
//    pointer constructor is a test seam; production passes HttpClient::manager()).
// 3. The SSL-error bypass call never appears anywhere in client source.
// 4. setNetworkAccessManagerFactory( is called only by PolicyNamFactory::installOn
//    (quick/PolicyNamFactory.cpp), which refuses a late install.
//
// Matching is on comment-stripped code (SourceScan::stripComments, Cpp
// dialect: string, char and raw-string literals are tracked, so an in-string
// "//" or "*/*" cannot hide code). guardPatternsSelfTest pins every pattern
// against positive and negative snippets.
//
// kPendingMigration lists owners not yet moved onto the seam. Each migration
// task deletes its entries; a listed file that no longer offends FAILS as
// stale, so the list can only shrink. It is empty once S1a is complete.
namespace {
const QStringList kPendingMigration = {
};

// Alternatives: heap (new), smart-pointer factories, a named object (stack or
// by-value member, any initializer), and an unnamed temporary whose type name
// starts an expression (after = ( { [ ; or return, or at line start). The last
// deliberately skips a base initializer `: QNetworkAccessManager(parent)` /
// `, QNetworkAccessManager(parent)`, which only a subclass can write -- and
// subclassing is policed by kNamSubclass.
const QRegularExpression kNamConstruction(QStringLiteral(
    "\\bnew\\s+QNetworkAccessManager\\b"
    "|\\bmake_(?:unique|shared)\\s*<\\s*QNetworkAccessManager\\s*>"
    "|\\bQNetworkAccessManager\\s+[A-Za-z_]\\w*\\s*[;({=]"
    "|(?:[=({\\[;]|\\breturn\\b|^)\\s*QNetworkAccessManager\\s*[({]"),
    QRegularExpression::MultilineOption);
// A class/struct head whose base-specifier list (between the single ':' and
// the opening '{' or a ';') names QNetworkAccessManager: any access specifier
// or none, virtual or not, first or later base.
const QRegularExpression kNamSubclass(QStringLiteral(
    "\\b(?:class|struct)\\s+[\\w:\\s]*?\\w\\s*:(?!:)[^{;]*\\bQNetworkAccessManager\\b"));
const QString kSeamSubclassHeader = QStringLiteral("core/transport/policyenforcingnam.h");
// QQmlEngine::setNetworkAccessManagerFactory( -- only installOn may call it.
const QRegularExpression kFactoryInstall(QStringLiteral("\\bsetNetworkAccessManagerFactory\\s*\\("));
const QString kFactoryInstallOwner = QStringLiteral("quick/PolicyNamFactory.cpp");

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
    void factoryInstalledOnlyByInstallOn();
    void guardPatternsSelfTest();

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
    return SourceScan::stripComments(src, SourceScan::Dialect::Cpp);
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
    // assert seam ownership and issue no request. Test headers are scanned
    // too: QuickTestSetup.h builds the AccessControl singleton hub for every
    // QuickTest and must keep it on OfflineHttp.
    const QStringList files = sourceFiles(QStringLiteral("quick/tests"),
                                          {QStringLiteral("tst_*.cpp"), QStringLiteral("*.h")}, {});
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
    // installOn() eagerly snapshots TransportPolicy::current() via create(), so
    // a policy published after it would leave the common manager on the default.
    QVERIFY2(policy >= 0 && install >= 0 && policy < install,
             "the transport policy must be published before installOn(), which eagerly snapshots it via create()");
    QVERIFY2(policy >= 0 && hub >= 0 && policy < hub,
             "the transport policy must be published before AccessControlHub builds its HttpClient");
    QVERIFY2(factory >= 0 && engine >= 0 && factory < engine,
             "PolicyNamFactory must be declared before (so it outlives) the engine");
    QVERIFY2(install > engine && load > install,
             "the factory must be installed on the engine before the first QML load");
    // Fail closed: the install result must guard an immediate `return -1;`
    // (a real return, so it also holds in Release builds).
    const QLatin1String guard("if (!PolicyNamFactory::installOn(engine, namFactory))");
    const qsizetype guardAt = code.indexOf(guard);
    const QString afterGuard = guardAt < 0 ? QString()
        : code.mid(guardAt + guard.size()).trimmed();
    QVERIFY2(guardAt >= 0 && afterGuard.startsWith(QLatin1String("return -1;")),
             "a failed installOn() must abort startup (return -1), in Release builds too");
}

void TestTransportSeamGuard::factoryInstalledOnlyByInstallOn()
{
    // A direct setNetworkAccessManagerFactory() skips installOn's refusals
    // (factory already set / QML already loaded / engine already built a
    // manager) and its PolicyEnforcingNam check. Scanned tree-wide, tests
    // included: no test needs a raw install today (the refusal paths in
    // tst_policynamfactory go through installOn). Only this file is exempt,
    // because guardPatternsSelfTest and quickMainInstallsTransportBeforeLoad
    // spell the call in string literals. A future test that must install a
    // foreign factory to provoke a refusal should be added to `allowed` with
    // a reason, rather than exempting a whole test directory.
    const QStringList allowed = {
        kFactoryInstallOwner,
        QStringLiteral("quick/tests/tst_transportseamguard.cpp"),
    };
    const QStringList files = sourceFiles(QString(), {QStringLiteral("*.cpp"), QStringLiteral("*.h")},
                                          {QStringLiteral("libs/")});
    QVERIFY2(files.size() > 50, "scan found too few files — is SRC_ROOT wrong?");

    QStringList offenders;
    for (const QString &rel : files) {
        bool ok = false;
        const QString code = stripComments(readSource(rel, &ok));
        QVERIFY2(ok, qPrintable(QStringLiteral("cannot read ") + rel));
        if (!allowed.contains(rel) && kFactoryInstall.match(code).hasMatch())
            offenders << rel;
    }
    QVERIFY2(offenders.isEmpty(), qPrintable(
        QStringLiteral("install the QML network factory only via PolicyNamFactory::installOn: ")
        + offenders.join(QStringLiteral(", "))));

    // Positive control: the sanctioned call site is still where the
    // allow-list says, so a move cannot leave a stale exemption behind.
    bool ok = false;
    const QString owner = stripComments(readSource(kFactoryInstallOwner, &ok));
    QVERIFY2(ok && kFactoryInstall.match(owner).hasMatch(),
             "quick/PolicyNamFactory.cpp no longer installs the factory — update kFactoryInstallOwner");
}

void TestTransportSeamGuard::guardPatternsSelfTest()
{
    // Pins every guard pattern (and the comment stripper feeding it) against
    // inline snippets: each must hit its positives and miss its negatives, so
    // a regex edit that silently stops matching -- or starts matching the
    // seam's own legitimate idioms -- fails here, not in review.
    struct Sample { const QRegularExpression *re; const char *name; const char *code; bool hit; };
    const Sample samples[] = {
        // --- raw manager construction: positives ---
        {&kNamConstruction, "construct", "auto *nam = new QNetworkAccessManager(this);", true},
        {&kNamConstruction, "construct", "auto *nam = new QNetworkAccessManager{};", true},
        {&kNamConstruction, "construct", "auto nam = std::make_unique<QNetworkAccessManager>();", true},
        {&kNamConstruction, "construct", "auto nam = std::make_shared<QNetworkAccessManager>(this);", true},
        {&kNamConstruction, "construct", "QNetworkAccessManager nam;", true},
        {&kNamConstruction, "construct", "QNetworkAccessManager nam{this};", true},
        {&kNamConstruction, "construct", "QNetworkAccessManager m_nam{nullptr};", true},
        {&kNamConstruction, "construct", "auto nam = QNetworkAccessManager{};", true},
        {&kNamConstruction, "construct", "auto nam = QNetworkAccessManager();", true},
        {&kNamConstruction, "construct", "    QNetworkAccessManager{}.get(req);", true},
        {&kNamConstruction, "construct", "return QNetworkAccessManager{};", true},
        {&kNamConstruction, "construct", "use(QNetworkAccessManager{});", true},
        // string-aware stripping: in-string "//" and "*/*" must not hide code
        {&kNamConstruction, "construct", "const QUrl u(\"http://host/\"); auto *n = new QNetworkAccessManager(this);", true},
        {&kNamConstruction, "construct", "const char *a = \"*/*\"; auto *n = new QNetworkAccessManager(this); /* note */", true},
        {&kNamConstruction, "construct", "auto j = R\"({\"u\":\"http://x\"})\"; auto *n = new QNetworkAccessManager(this);", true},
        {&kNamConstruction, "construct", "auto j = R\"(a\"b/*)\"; auto *n = new QNetworkAccessManager(this); /* note */", true},
        {&kNamConstruction, "construct", "const QChar q('\"'); const char *u = \"http://h\"; auto *n = new QNetworkAccessManager(this);", true},
        // --- raw manager construction: negatives ---
        {&kNamConstruction, "construct", "QNetworkAccessManager *nam = nullptr;", false},
        {&kNamConstruction, "construct", "void f(QNetworkAccessManager &nam);", false},
        {&kNamConstruction, "construct", "explicit Hub(QNetworkAccessManager *, QObject * = nullptr) = delete;", false},
        {&kNamConstruction, "construct", "auto *m = qobject_cast<QNetworkAccessManager *>(obj);", false},
        {&kNamConstruction, "construct", "return QNetworkAccessManager::createRequest(op, req, data);", false},
        {&kNamConstruction, "construct", "Nam::Nam(QObject *p)\n    : QNetworkAccessManager(p)\n{\n}", false},
        {&kNamConstruction, "construct", "Nam::Nam(QObject *p) : Mixin(), QNetworkAccessManager(p) {}", false},
        {&kNamConstruction, "construct", "class QNetworkAccessManager;", false},
        {&kNamConstruction, "construct", "Q_ASSERT_X(nam, \"X\", \"null QNetworkAccessManager: pass HttpClient::manager()\");", false},
        {&kNamConstruction, "construct", "// auto *n = new QNetworkAccessManager(this);", false},
        {&kNamConstruction, "construct", "/* QNetworkAccessManager nam; */", false},
        // --- subclassing: positives ---
        {&kNamSubclass, "subclass", "class A : public QNetworkAccessManager\n{", true},
        {&kNamSubclass, "subclass", "class A : QNetworkAccessManager {", true},
        {&kNamSubclass, "subclass", "class A : protected QNetworkAccessManager {", true},
        {&kNamSubclass, "subclass", "struct A : private QNetworkAccessManager {", true},
        {&kNamSubclass, "subclass", "class A final : public QObject, public QNetworkAccessManager {", true},
        {&kNamSubclass, "subclass", "class A\n    : public Base\n    , public QNetworkAccessManager\n{", true},
        {&kNamSubclass, "subclass", "class A : public virtual QNetworkAccessManager {", true},
        // --- subclassing: negatives ---
        {&kNamSubclass, "subclass", "class A : public QObject\n{\n    QNetworkAccessManager *m_nam = nullptr;\n};", false},
        {&kNamSubclass, "subclass", "class QNetworkAccessManager;", false},
        {&kNamSubclass, "subclass", "Nam::Nam(QObject *p) : QNetworkAccessManager(p) {}", false},
        {&kNamSubclass, "subclass", "return QNetworkAccessManager::createRequest(op, req, data);", false},
        {&kNamSubclass, "subclass", "enum class Mode : int { A, B };\nQNetworkAccessManager *m = nullptr;", false},
        // --- factory install: positives ---
        {&kFactoryInstall, "factory", "engine.setNetworkAccessManagerFactory(&factory);", true},
        {&kFactoryInstall, "factory", "engine->setNetworkAccessManagerFactory (f);", true},
        // --- factory install: negatives ---
        {&kFactoryInstall, "factory", "if (engine.networkAccessManagerFactory())", false},
        {&kFactoryInstall, "factory", "// engine.setNetworkAccessManagerFactory(&f);", false},
    };
    QStringList failures;
    for (const Sample &s : samples) {
        const QString code = stripComments(QString::fromUtf8(s.code));
        if (s.re->match(code).hasMatch() != s.hit) {
            failures << QStringLiteral("%1 should %2: %3")
                            .arg(QLatin1String(s.name), s.hit ? QStringLiteral("match") : QStringLiteral("NOT match"),
                                 QString::fromUtf8(s.code));
        }
    }
    QVERIFY2(failures.isEmpty(), qPrintable(failures.join(QStringLiteral("\n  "))));
}

QTEST_APPLESS_MAIN(TestTransportSeamGuard)
#include "tst_transportseamguard.moc"
