#ifndef SCHOOLINFOVIEWMODEL_H
#define SCHOOLINFOVIEWMODEL_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <qqml.h>

class QSettings;

// Read-only school-identity source for the admin sidebar (Phase 3 admin
// chrome). Reads the same legacy QSettings keys KioskViewModel already
// caches (school/name, school/address) plus school/logoPath, which
// KioskViewModel does NOT read today — that gap is exactly why the sidebar
// has never shown a logo (BrandPanel.qml's "Phase 4 admin logo import"
// comment is stale; the logo path has been sitting in QSettings all along).
//
// Deliberately its own small VM rather than reusing KioskViewModel: the
// kiosk VM owns login orchestration/RFID/session stats that have no place
// on the admin surface. This gives admin and kiosk one source of truth for
// school identity; the kiosk screen can migrate to it later. Phase 4 owns
// *writing* these values (the admin logo import UI) — this VM only displays
// what is already configured.
class SchoolInfoViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString schoolName READ schoolName CONSTANT)
    Q_PROPERTY(QString schoolAddress READ schoolAddress CONSTANT)
    // A bare filesystem path is not a valid Image.source — QUrl::fromLocalFile
    // is required for QML's Image to load it. See loadFrom() in the .cpp.
    Q_PROPERTY(QUrl logoUrl READ logoUrl CONSTANT)
    // The path is user-configured (via the legacy Widgets settings dialog
    // today, Phase 4 admin import later) and can rot: file moved, deleted,
    // or never set. hasLogo is the single, explicit signal QML should key
    // its placeholder-fallback off of — never "is logoUrl non-empty".
    Q_PROPERTY(bool hasLogo READ hasLogo CONSTANT)

public:
    explicit SchoolInfoViewModel(QObject *parent = nullptr);

    // Test seam: read from an already-constructed QSettings scope (e.g. an
    // ini-file-backed QSettings under a QTemporaryDir) instead of the real
    // "MyCompany"/"MyApp" registry scope the default constructor uses.
    // Mirrors the injectable-QSettings pattern already established by
    // BrandTheme::loadCachedConfig/saveCachedConfig (core/brandtheme.h,
    // "QSettings is injected so tests use a temp-file INI store") and
    // exercised by qt-app/tests/tst_brandtheme.cpp. Unit tests for this
    // class use ONLY this constructor, so they never touch the developer's
    // real registry.
    explicit SchoolInfoViewModel(QSettings &settings, QObject *parent = nullptr);

    QString schoolName() const { return m_schoolName; }
    QString schoolAddress() const { return m_schoolAddress; }
    QUrl logoUrl() const { return m_logoUrl; }
    bool hasLogo() const { return m_hasLogo; }

private:
    void loadFrom(QSettings &settings);

    QString m_schoolName;
    QString m_schoolAddress;
    QUrl m_logoUrl;
    bool m_hasLogo = false;
};

#endif // SCHOOLINFOVIEWMODEL_H
