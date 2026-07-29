#include <QtTest>
#include <QDirIterator>
#include <QFile>
#include <QString>
#include <QStringList>

// Grep-guard (Phase 4d Task 12): asserts NO deprecated brand/accent token name
// survives anywhere under the QML source tree or the QuickTest data dir. Once
// the deprecated QML aliases (Theme.qml) and VM forwarding accessors
// (ThemeViewModel.h) are gone, any re-introduction of an old name is caught
// here instead of silently re-growing the alias surface the migration removed.
//
// CRITICAL: the check runs on CODE ONLY. Task 11 left owner-required
// traceability COMMENTS that literally spell banned token names (e.g.
// LButton.qml "Was a flat brand.onPrimary."). Those comments are deliberate and
// must survive, so every *.qml is comment-stripped (// line + /* */ block)
// before the substring check. A raw-substring guard would be permanently RED on
// those comments.
//
// The C++ paletteFromJson read-compat aliases in brandtheme.cpp are NOT in
// scope: they live outside qt-app/quick/ and are permanent (spec 6.4), so this
// guard never sees them.
//
// SCOPE CAVEAT: this guard scans *.qml only. Re-adding the deleted C++ VM
// forwarding accessors (ThemeViewModel.h) would NOT be caught here — QML is
// where the alias surface is actually consumed, so that is what we police.
class TestNoTokenAliases : public QObject
{
    Q_OBJECT
private slots:
    void noDeprecatedTokenNamesRemain();

private:
    static QString stripComments(const QString &src);
};

// Remove // line-comments (to EOL) and /* */ block-comments, preserving
// newlines so the remaining text still maps to the original line structure.
// String literals ARE tracked: while inside a double- or single-quoted string,
// `//` and `/*` are NOT treated as comment starts but preserved verbatim as
// code. This matters because the tree already has `//` inside QML strings (e.g.
// 'http://attacker/beacon', "file:///manifests/…" in tst_qml_admin.qml); without
// string tracking, a future banned token trailing an in-string `//` on the same
// line would be silently stripped and the guard would wrongly pass.
QString TestNoTokenAliases::stripComments(const QString &src)
{
    QString out;
    out.reserve(src.size());
    enum State { Code, LineComment, BlockComment, StringLit };
    State state = Code;
    QChar quote;      // which quote opened the current StringLit
    bool escaped = false;   // previous char was an unescaped backslash
    for (int i = 0; i < src.size(); ++i) {
        const QChar c = src.at(i);
        const QChar n = (i + 1 < src.size()) ? src.at(i + 1) : QChar();
        switch (state) {
        case Code:
            if (c == QLatin1Char('/') && n == QLatin1Char('/')) {
                state = LineComment;
                ++i;
            } else if (c == QLatin1Char('/') && n == QLatin1Char('*')) {
                state = BlockComment;
                ++i;
            } else if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
                state = StringLit;
                quote = c;
                escaped = false;
                out.append(c);
            } else {
                out.append(c);
            }
            break;
        case StringLit:
            // String content is code: preserve it verbatim so in-string
            // `//` and `/*` are never mistaken for comments.
            out.append(c);
            if (escaped) {
                escaped = false;
            } else if (c == QLatin1Char('\\')) {
                escaped = true;
            } else if (c == quote) {
                state = Code;
            } else if (c == QLatin1Char('\n')) {
                // QML strings don't span raw newlines; treat an unterminated
                // quote as ended here rather than swallowing the rest of the file.
                state = Code;
            }
            break;
        case LineComment:
            if (c == QLatin1Char('\n')) {
                state = Code;
                out.append(c);
            }
            break;
        case BlockComment:
            if (c == QLatin1Char('*') && n == QLatin1Char('/')) {
                state = Code;
                ++i;
            }
            break;
        }
    }
    return out;
}

void TestNoTokenAliases::noDeprecatedTokenNamesRemain()
{
    // Full-name entries only: "brand.onPrimary"/"brand.onKiosk" will NOT match
    // the valid new tokens "brand.on"/"brand.onMuted" (different next char), so
    // a bare "brand.on" is deliberately NOT banned. "Theme.secondary" also
    // catches the already-removed "Theme.secondarySoft" — harmless.
    const QStringList banned = {
        QStringLiteral("brand.admin"), QStringLiteral("brand.adminHover"), QStringLiteral("brand.adminSoft"),
        QStringLiteral("brand.kiosk"), QStringLiteral("brand.kioskHover"), QStringLiteral("brand.kioskSoft"),
        QStringLiteral("brand.onPrimary"), QStringLiteral("brand.onKiosk"), QStringLiteral("Theme.secondary"),
        QStringLiteral("secondarySoft"), QStringLiteral("onBrandMuted")
    };

    const QStringList dirs = {
        QStringLiteral(SRC_QML_DIR),
        QStringLiteral(SRC_QML_TEST_DIR)
    };

    for (const QString &dir : dirs) {
        QDirIterator it(dir, QStringList() << QStringLiteral("*.qml"),
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            QFile f(path);
            QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text),
                     qPrintable(QStringLiteral("cannot open ") + path));
            const QString code = stripComments(QString::fromUtf8(f.readAll()));
            f.close();
            for (const QString &tok : banned) {
                QVERIFY2(!code.contains(tok),
                         qPrintable(path + QStringLiteral(" references banned token ") + tok));
            }
        }
    }
}

QTEST_APPLESS_MAIN(TestNoTokenAliases)
#include "tst_notokenaliases.moc"
