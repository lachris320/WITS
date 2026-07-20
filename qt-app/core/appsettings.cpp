#include "appsettings.h"

#include <QStandardPaths>
#include <QTemporaryDir>

namespace {
// Constant-initialized POD: safe to read from a pre-main() constructor
// function in another translation unit regardless of static init order.
bool g_isolated = false;

QSettings::Format storeFormat()
{
    return g_isolated ? QSettings::IniFormat : QSettings::NativeFormat;
}
}   // namespace

AppSettings::AppSettings(QObject *parent)
    : QSettings(storeFormat(), QSettings::UserScope,
                AppSettings::organizationName(), AppSettings::applicationName(), parent)
{
}

QString AppSettings::organizationName() { return QStringLiteral("MyCompany"); }
QString AppSettings::applicationName()  { return QStringLiteral("MyApp"); }

void AppSettings::isolateForTesting()
{
    if (g_isolated)
        return;

    // Function-local static: nothing is created during a normal app run (this
    // function is never called there), and the directory is removed when the
    // process exits. Note this runs before QCoreApplication exists — that is
    // fine, QTemporaryDir only needs QDir::tempPath().
    static QTemporaryDir dir;
    if (!dir.isValid()) {
        // Failing loudly beats silently falling back to the real registry,
        // which is the exact failure mode this class exists to prevent.
        qFatal("AppSettings::isolateForTesting: could not create a temporary "
               "settings directory; refusing to run against the real store.");
    }

    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, dir.path());
    // Keeps QStandardPaths::AppDataLocation (where SettingsController copies
    // imported logos/posters) out of the real per-user profile too.
    QStandardPaths::setTestModeEnabled(true);

    g_isolated = true;
}

bool AppSettings::isIsolatedForTesting() { return g_isolated; }
