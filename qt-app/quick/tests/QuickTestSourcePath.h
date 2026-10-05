#ifndef QUICKTESTSOURCEPATH_H
#define QUICKTESTSOURCEPATH_H

#include <QByteArray>
#include <QFileInfo>
#include <QString>
#include <cstring>

// Turns the .qml path a generated QuickTest main (tests/QuickTestMain.cpp.in)
// embeds into the const char* quick_test_main_with_setup() expects — or into
// an error. Qt decodes that argument with QString::fromLocal8Bit (the ANSI
// code page on Windows) and, if the decoded path does not exist, silently
// scans the current directory instead. So a missing file, or a path the code
// page cannot represent (e.g. a checkout under C:\Users\<non-Latin name>),
// must fail loudly here rather than run the wrong tests or none. Pure and
// header-only; encode/decode are injectable so tests can simulate a non-UTF-8
// code page on any platform (tests/tst_quicktestsourcepath.cpp).
namespace QuickTestSourcePath {

struct Result {
    QByteArray localPath;   // valid only when error is empty
    QString error;          // empty == ok
};

// Encode: QString -> QByteArray (local 8-bit); Decode: QByteArray -> QString.
template <typename Encode, typename Decode>
Result toQuickTestArg(const QString &qmlPath, Encode encode, Decode decode)
{
    if (!QFileInfo(qmlPath).isFile())
        return {QByteArray(), QStringLiteral("QuickTest file not found: %1").arg(qmlPath)};
    const QByteArray bytes = encode(qmlPath);
    if (decode(bytes) != qmlPath) {
        return {QByteArray(),
                QStringLiteral(
                    "QuickTest file path is not representable in the system's local 8-bit "
                    "code page: %1. quick_test_main decodes its source path with "
                    "QString::fromLocal8Bit, so it could not find this file. Move the "
                    "checkout/build to a path representable in the ANSI code page (e.g. "
                    "ASCII), or pass -input <file.qml>.")
                    .arg(qmlPath)};
    }
    return {bytes, QString()};
}

inline Result toQuickTestArg(const QString &qmlPath)
{
    return toQuickTestArg(
        qmlPath, [](const QString &s) { return s.toLocal8Bit(); },
        [](const QByteArray &b) { return QString::fromLocal8Bit(b); });
}

// True iff quick_test_main would take its test path from "-input <path>".
// Mirrors its argv loop: the other value-taking options consume their next
// argument, and an empty -input value is ignored (it falls back to sourceDir).
inline bool hasInputOverride(int argc, char **argv)
{
    static const char *const valueOptions[] = {"-import", "-plugins", "-translation",
                                               "-file-selector"};
    bool found = false;
    for (int i = 1; i < argc; ++i) {
        if (i + 1 >= argc)
            break;   // a trailing option has no value: quick_test_main ignores it
        if (std::strcmp(argv[i], "-input") == 0) {
            found = argv[i + 1][0] != '\0';   // last -input wins, as in Qt
            ++i;
            continue;
        }
        for (const char *opt : valueOptions) {
            if (std::strcmp(argv[i], opt) == 0) {
                ++i;
                break;
            }
        }
    }
    return found;
}

} // namespace QuickTestSourcePath

#endif // QUICKTESTSOURCEPATH_H
