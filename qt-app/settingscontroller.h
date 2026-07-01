#pragma once
#include <QObject>
#include "settingsdata.h"

class SettingsController : public QObject
{
    Q_OBJECT
public:
    explicit SettingsController(QObject *parent = nullptr);

    SettingsData load();
    bool         save(const SettingsData &data);

    QString importLogo(const QString &sourcePath);
    QString importPoster(const QString &sourcePath);

signals:
    void settingsSaved(const SettingsData &data);
    void logoChanged(const QString &destinationPath);
    void posterChanged(const QString &destinationPath);
    void importError(const QString &message);

private:
    QString importImageFile(const QString &sourcePath, const QString &type);
};
