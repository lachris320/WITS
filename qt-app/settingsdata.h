#pragma once
#include <QString>

struct SettingsData
{
    // School
    QString schoolName;
    QString schoolAddress;
    QString fontFamily;
    int     fontSize          = 12;
    QString logoPath;
    QString posterPath;

    // Administrator
    QString adminName;
    QString adminPosition;

    // Library hours stored in 24-hour format (0–23)
    int     libraryOpenHour  = 8;
    int     libraryCloseHour = 17;

    // Features
    bool    guestLoginEnabled = false;
};
