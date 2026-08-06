#include "apiconfigloader.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QSettings>

#include "apiconfig.h"

namespace ApiConfigLoader {

QString resolveBaseUrl(const QString &envValue, const QString &iniFilePath)
{
    // 1. Environment override (dev/CI).
    const QString env = envValue.trimmed();
    if (!env.isEmpty())
        return env;

    // 2. config.ini beside the exe.
    if (QFileInfo::exists(iniFilePath)) {
        QSettings ini(iniFilePath, QSettings::IniFormat);
        if (ini.status() == QSettings::NoError) {
            const QString fromIni =
                ini.value(QStringLiteral("Server/BaseURL")).toString().trimmed();
            if (!fromIni.isEmpty())
                return fromIni;
        }
    }

    // 3. Nothing configured — caller keeps ApiConfig's localhost default.
    return QString();
}

void applyFromRuntime(const QString &appDirPath)
{
    const QString resolved = resolveBaseUrl(
        qEnvironmentVariable("WITS_API_BASE_URL"),
        appDirPath + QStringLiteral("/config.ini"));
    if (!resolved.isEmpty())
        ApiConfig::setBaseUrl(resolved);
}

} // namespace ApiConfigLoader
