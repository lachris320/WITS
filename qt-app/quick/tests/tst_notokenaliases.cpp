#include <QtTest>
#include <QDirIterator>
#include <QFile>
#include <QString>
#include <QStringList>

#include "sourcescan.h"

// Grep-guard (Phase 4d Task 12): asserts NO deprecated brand/accent token name
// survives anywhere under the QML source tree or the QuickTest data dir. Once
// the deprecated QML aliases (Theme.qml) and VM forwarding accessors
// (ThemeViewModel.h) are gone, any re-introduction of an old name is caught
// here instead of silently re-growing the alias surface the migration removed.
//
// CRITICAL: the check runs on CODE ONLY. Task 11 left owner-required
// traceability COMMENTS that literally spell banned token names (e.g.
// LButton.qml "Was a flat brand.onPrimary."). Those comments are deliberate and
// must survive, so every *.qml is comment-stripped (// line + /* */ block,
// string-literal aware: SourceScan::stripComments in testsupport/sourcescan.h,
// default QmlJs dialect) before the substring check. A raw-substring guard
// would be permanently RED on those comments.
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
};

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
            const QString code = SourceScan::stripComments(QString::fromUtf8(f.readAll()));
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
