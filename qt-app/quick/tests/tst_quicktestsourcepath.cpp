#include <QtTest>
#include <QByteArrayList>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <vector>
#include "QuickTestSourcePath.h"

// Unit test for the PURE QuickTestSourcePath helper (quick/tests/
// QuickTestSourcePath.h) that every generated QuickTest main
// (tests/QuickTestMain.cpp.in) uses to hand its .qml path to
// quick_test_main_with_setup(). Qt decodes that path with
// QString::fromLocal8Bit — the ANSI code page on Windows — and, if the decoded
// path does not exist, silently scans the current directory instead. The
// helper must turn a missing or code-page-unrepresentable path into an error,
// never into a silently-wrong byte string. The Windows code page is simulated
// with injected Latin-1 encode/decode (CP1252 covers Latin-1's letters).
class TestQuickTestSourcePath : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void missingFileIsAnError();
    void directoryIsAnError();
    void asciiPathRoundTrips();
    void nonAsciiPathWithRealLocal8Bit();
    void latin1RepresentablePathEncodesAsLatin1();
    void pathOutsideCodePageIsAnError();
    void hasInputOverride_data();
    void hasInputOverride();

private:
    static QString makeFile(const QString &dir, const QString &name);

    QTemporaryDir m_root;
};

namespace {

QByteArray latin1Encode(const QString &s) { return s.toLatin1(); }
QString latin1Decode(const QByteArray &b) { return QString::fromLatin1(b); }

} // namespace

QString TestQuickTestSourcePath::makeFile(const QString &dir, const QString &name)
{
    if (!QDir().mkpath(dir))
        return QString();
    const QString path = QDir(dir).filePath(name);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return QString();
    f.write("import QtQuick\n");
    return path;
}

void TestQuickTestSourcePath::initTestCase()
{
    QVERIFY(m_root.isValid());
}

void TestQuickTestSourcePath::missingFileIsAnError()
{
    const QString path = QDir(m_root.path()).filePath(QStringLiteral("tst_missing.qml"));
    const QuickTestSourcePath::Result r = QuickTestSourcePath::toQuickTestArg(path);
    QVERIFY(!r.error.isEmpty());
    QVERIFY2(r.error.contains(path), qPrintable(r.error));
    QVERIFY(r.localPath.isEmpty());
}

void TestQuickTestSourcePath::directoryIsAnError()
{
    // A directory would make quick_test_main run every tst_*.qml under it.
    const QuickTestSourcePath::Result r = QuickTestSourcePath::toQuickTestArg(m_root.path());
    QVERIFY(!r.error.isEmpty());
    QVERIFY(r.localPath.isEmpty());
}

void TestQuickTestSourcePath::asciiPathRoundTrips()
{
    const QString path = makeFile(QDir(m_root.path()).filePath(QStringLiteral("ascii")),
                                  QStringLiteral("tst_ascii.qml"));
    QVERIFY(!path.isEmpty());

    const QuickTestSourcePath::Result r = QuickTestSourcePath::toQuickTestArg(path);
    QVERIFY2(r.error.isEmpty(), qPrintable(r.error));
    QCOMPARE(QString::fromLocal8Bit(r.localPath), path);
}

void TestQuickTestSourcePath::nonAsciiPathWithRealLocal8Bit()
{
    const QString path = makeFile(
        QDir(m_root.path()).filePath(QStringLiteral("wïts-é-日本")),
        QStringLiteral("tst_é_日本.qml"));
    QVERIFY(!path.isEmpty());

    const QuickTestSourcePath::Result r = QuickTestSourcePath::toQuickTestArg(path);
#ifdef Q_OS_WIN
    // The real ANSI code page may or may not cover these characters; either
    // way the helper must agree with what quick_test_main would decode.
    if (QString::fromLocal8Bit(path.toLocal8Bit()) != path) {
        QVERIFY(!r.error.isEmpty());
        QVERIFY(r.localPath.isEmpty());
        return;
    }
#endif
    // Qt 6 on Unix: local 8-bit is always UTF-8, so every path is representable.
    QVERIFY2(r.error.isEmpty(), qPrintable(r.error));
    QCOMPARE(QString::fromLocal8Bit(r.localPath), path);
}

void TestQuickTestSourcePath::latin1RepresentablePathEncodesAsLatin1()
{
    if (latin1Decode(latin1Encode(m_root.path())) != m_root.path())
        QSKIP("temporary directory path is not Latin-1 representable");
    const QString path = makeFile(QDir(m_root.path()).filePath(QStringLiteral("wïts-é")),
                                  QStringLiteral("tst_é.qml"));
    QVERIFY(!path.isEmpty());

    const QuickTestSourcePath::Result r =
        QuickTestSourcePath::toQuickTestArg(path, latin1Encode, latin1Decode);
    QVERIFY2(r.error.isEmpty(), qPrintable(r.error));
    QCOMPARE(r.localPath, path.toLatin1());
    QVERIFY(r.localPath.contains('\xe9'));            // Latin-1 e-acute
    QVERIFY(!r.localPath.contains("\xc3\xa9"));       // not its UTF-8 bytes
}

void TestQuickTestSourcePath::pathOutsideCodePageIsAnError()
{
    // U+65E5 is not in Latin-1/CP1252: toLatin1() lossily maps it to '?'. The
    // helper must report that instead of handing Qt a path that decodes to a
    // different, nonexistent file (which makes Qt scan the cwd instead).
    const QString path = makeFile(
        QDir(m_root.path()).filePath(QStringLiteral("wïts-é-日本")),
        QStringLiteral("tst_é_日本.qml"));
    QVERIFY(!path.isEmpty());

    const QuickTestSourcePath::Result r =
        QuickTestSourcePath::toQuickTestArg(path, latin1Encode, latin1Decode);
    QVERIFY(!r.error.isEmpty());
    QVERIFY(r.localPath.isEmpty());
    QVERIFY2(r.error.contains(path), qPrintable(r.error));
    QVERIFY2(r.error.contains(QStringLiteral("code page")), qPrintable(r.error));
    QVERIFY2(r.error.contains(QStringLiteral("-input")), qPrintable(r.error));
}

void TestQuickTestSourcePath::hasInputOverride_data()
{
    QTest::addColumn<QStringList>("args");
    QTest::addColumn<bool>("expected");

    QTest::newRow("no args") << QStringList() << false;
    QTest::newRow("unrelated args") << QStringList{"-v2", "-maxwarnings", "0"} << false;
    QTest::newRow("-input value") << QStringList{"-input", "x.qml"} << true;
    QTest::newRow("-input among others")
        << QStringList{"-v2", "-input", "x.qml", "-maxwarnings", "0"} << true;
    QTest::newRow("-input last, no value") << QStringList{"-v2", "-input"} << false;
    // quick_test_main treats an empty -input value as absent (falls back to
    // the source path), so it is not an override either.
    QTest::newRow("-input empty value") << QStringList{"-input", ""} << false;
    // Mirrors quick_test_main's parser: "-input" here is -import's VALUE.
    QTest::newRow("-input as -import value") << QStringList{"-import", "-input", "x"} << false;
    QTest::newRow("prefix only") << QStringList{"-inputx", "x.qml"} << false;
}

void TestQuickTestSourcePath::hasInputOverride()
{
    QFETCH(QStringList, args);
    QFETCH(bool, expected);

    QByteArrayList storage{QByteArrayLiteral("tst_prog")};
    for (const QString &a : args)
        storage << a.toLocal8Bit();
    std::vector<char *> argv;
    for (QByteArray &b : storage)
        argv.push_back(b.data());
    argv.push_back(nullptr);

    QCOMPARE(QuickTestSourcePath::hasInputOverride(int(storage.size()), argv.data()), expected);
}

QTEST_APPLESS_MAIN(TestQuickTestSourcePath)
#include "tst_quicktestsourcepath.moc"
