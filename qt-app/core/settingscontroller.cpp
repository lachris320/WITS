#include "settingscontroller.h"
#include <QSettings>

#include "appsettings.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QStandardPaths>

SettingsController::SettingsController(QObject *parent)
    : QObject(parent)
{}

SettingsData SettingsController::load()
{
    AppSettings s;
    SettingsData d;
    d.schoolName        = s.value(QLatin1String("school/name"),       QLatin1String("")).toString();
    d.schoolAddress     = s.value(QLatin1String("school/address"),    QLatin1String("")).toString();
    d.fontFamily        = s.value(QLatin1String("school/fontFamily"), QLatin1String("")).toString();
    d.fontSize          = s.value(QLatin1String("school/fontSize"),   12).toInt();
    d.logoPath          = s.value(QLatin1String("school/logoPath"),   QLatin1String("")).toString();
    d.posterPath        = s.value(QLatin1String("school/posterPath"), QLatin1String("")).toString();
    d.adminName         = s.value(QLatin1String("admin/name"),        QLatin1String("")).toString();
    d.adminPosition     = s.value(QLatin1String("admin/position"),    QLatin1String("")).toString();
    d.libraryOpenHour   = s.value(QLatin1String("library/openHour"),  8).toInt();
    d.libraryCloseHour  = s.value(QLatin1String("library/closeHour"),17).toInt();
    d.guestLoginEnabled = s.value(QLatin1String("features/guestLogin"), false).toBool();
    return d;
}

bool SettingsController::save(const SettingsData &data)
{
    AppSettings s;
    s.setValue(QLatin1String("school/name"),        data.schoolName);
    s.setValue(QLatin1String("school/address"),     data.schoolAddress);
    s.setValue(QLatin1String("school/fontFamily"),  data.fontFamily);
    s.setValue(QLatin1String("school/fontSize"),    data.fontSize);
    s.setValue(QLatin1String("school/logoPath"),    data.logoPath);
    s.setValue(QLatin1String("school/posterPath"),  data.posterPath);
    s.setValue(QLatin1String("admin/name"),         data.adminName);
    s.setValue(QLatin1String("admin/position"),     data.adminPosition);
    s.setValue(QLatin1String("library/openHour"),   data.libraryOpenHour);
    s.setValue(QLatin1String("library/closeHour"),  data.libraryCloseHour);
    s.setValue(QLatin1String("features/guestLogin"),data.guestLoginEnabled);
    s.sync();
    if (s.status() != QSettings::NoError)
        return false;
    emit settingsSaved(data);
    return true;
}

QString SettingsController::importLogo(const QString &sourcePath)
{
    return importImageFile(sourcePath, QLatin1String("logo"));
}

QString SettingsController::importPoster(const QString &sourcePath)
{
    return importImageFile(sourcePath, QLatin1String("poster"));
}

QString SettingsController::importImageFile(const QString &sourcePath, const QString &type)
{
    if (!QFile::exists(sourcePath)) {
        emit importError(tr("File not found: %1").arg(sourcePath));
        return {};
    }
    QImage img(sourcePath);
    if (img.isNull()) {
        emit importError(tr("Not a valid image: %1").arg(sourcePath));
        return {};
    }
    const QString destDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(destDir);
    const QString destPath = destDir + QLatin1Char('/') + type
                             + QLatin1Char('_') + QFileInfo(sourcePath).fileName();
    if (QFile::exists(destPath))
        QFile::remove(destPath);
    if (!QFile::copy(sourcePath, destPath)) {
        emit importError(tr("Could not copy to: %1").arg(destPath));
        return {};
    }
    if (type == QLatin1String("logo"))
        emit logoChanged(destPath);
    else
        emit posterChanged(destPath);
    return destPath;
}
