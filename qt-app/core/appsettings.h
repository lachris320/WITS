#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QSettings>
#include <QString>

// The application's ONE persistent settings scope ("MyCompany"/"MyApp").
// Every read or write of that scope — app code and test code alike — must go
// through this class instead of constructing QSettings directly.
//
// WHY THIS EXISTS (data-loss bug, Phase 4c):
//   On Qt 6 the QSettings(organization, application) constructor is hardcoded
//   to QSettings::NativeFormat and IGNORES QSettings::setDefaultFormat().
//   On Windows NativeFormat is the registry, and QSettings::setPath() is
//   documented to have no effect on the native format there. A test that did
//
//       QSettings::setDefaultFormat(QSettings::IniFormat);
//       QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tmp);
//       QSettings s("MyCompany", "MyApp");     // <- still the REAL registry
//
//   therefore read AND WROTE the developer's real
//   HKCU\Software\MyCompany\MyApp hive, silently destroying their configured
//   school name, address, admin details and library hours on every ctest run.
//   Verified empirically: the constructed object reported format=NativeFormat
//   and fileName="\HKEY_CURRENT_USER\Software\MyCompany\MyApp" while
//   QSettings::defaultFormat() was IniFormat.
//
// AppSettings resolves the format at CONSTRUCTION time, which is the seam the
// old approach lacked: isolateForTesting() flips every subsequent instance in
// the process onto a throwaway INI file, whatever constructor the caller uses.
class AppSettings : public QSettings
{
public:
    explicit AppSettings(QObject *parent = nullptr);

    // The organization/application pair the whole app shares. Exposed so
    // nothing else has to repeat the string literals.
    static QString organizationName();
    static QString applicationName();

    // TEST SEAM — never called by the application itself.
    //
    // Points every AppSettings constructed afterwards at an INI file inside a
    // process-private QTemporaryDir, and enables QStandardPaths test mode so
    // AppDataLocation writes (the logo/poster import copies files there) also
    // stay out of the developer's real profile. Idempotent, and deliberately
    // one-way: there is no un-isolate, so nothing can hand a test process back
    // to the real store half-way through a run.
    //
    // cmake/WitsTest.cmake links testsupport/settingsisolation.cpp into EVERY
    // target registered with wits_add_qttest(), and that file calls this
    // before main() — so a new test cannot reintroduce the bug by forgetting
    // a line, and a startup guard aborts any test binary that is not isolated.
    static void isolateForTesting();

    // True once isolateForTesting() has run. Lets a test assert the harness is
    // genuinely isolated instead of trusting that it is.
    static bool isIsolatedForTesting();
};

#endif // APPSETTINGS_H
