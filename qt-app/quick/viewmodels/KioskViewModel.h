#ifndef KIOSKVIEWMODEL_H
#define KIOSKVIEWMODEL_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QUrl>
#include <qqml.h>
#include "RecentLoginsModel.h"

class QNetworkAccessManager;
class QQuickWindow;
class QTimer;
class RfidQuickFilter;

// installRfid()'s QQuickWindow* parameter is a Q_INVOKABLE argument, so moc's
// generated metatype registration needs the complete type — the forward
// declaration above is enough for the class body, but moc's generated
// mocs_compilation.cpp translation unit only sees this header. Q_MOC_INCLUDE
// tells moc to pull in the full <QQuickWindow> header for that generated file.
Q_MOC_INCLUDE("QQuickWindow")

// The kiosk surface's only QML-facing object. Owns login orchestration (via
// LoginParser + a QNetworkAccessManager), the attendance model, a 1 Hz clock,
// session stat counters, school info, and the guest-enabled flag.
class KioskViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(RecentLoginsModel *recentLogins READ recentLogins CONSTANT)
    Q_PROPERTY(bool hasStudent READ hasStudent NOTIFY currentChanged)
    Q_PROPERTY(QString currentName READ currentName NOTIFY currentChanged)
    Q_PROPERTY(QString currentFullName READ currentFullName NOTIFY currentChanged)
    Q_PROPERTY(QString currentCourse READ currentCourse NOTIFY currentChanged)
    Q_PROPERTY(QString currentYear READ currentYear NOTIFY currentChanged)
    Q_PROPERTY(QString currentDept READ currentDept NOTIFY currentChanged)
    Q_PROPERTY(QString currentTime READ currentTime NOTIFY currentChanged)
    Q_PROPERTY(int visitorsToday READ visitorsToday NOTIFY statsChanged)
    Q_PROPERTY(int visitorsThisHour READ visitorsThisHour NOTIFY statsChanged)
    Q_PROPERTY(QString clockTime READ clockTime NOTIFY clockChanged)
    Q_PROPERTY(QString clockMeridiem READ clockMeridiem NOTIFY clockChanged)
    Q_PROPERTY(QString clockDate READ clockDate NOTIFY clockChanged)
    Q_PROPERTY(QString greeting READ greeting NOTIFY clockChanged)
    Q_PROPERTY(QString schoolName READ schoolName NOTIFY schoolInfoChanged)
    Q_PROPERTY(QString schoolAddress READ schoolAddress NOTIFY schoolInfoChanged)
    Q_PROPERTY(QString libraryHours READ libraryHours NOTIFY schoolInfoChanged)
    Q_PROPERTY(bool guestEnabled READ guestEnabled NOTIFY guestEnabledChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
    Q_PROPERTY(QString statusSeverity READ statusSeverity NOTIFY statusChanged)

public:
    explicit KioskViewModel(QObject *parent = nullptr);

    RecentLoginsModel *recentLogins() { return &m_recent; }
    bool hasStudent() const { return m_hasStudent; }
    QString currentName() const { return m_currentName; }
    QString currentFullName() const { return m_currentFullName; }
    QString currentCourse() const { return m_currentCourse; }
    QString currentYear() const { return m_currentYear; }
    QString currentDept() const { return m_currentDept; }
    QString currentTime() const { return m_currentTime; }
    int visitorsToday() const { return m_visitorsToday; }
    int visitorsThisHour() const { return m_visitorsThisHour; }
    QString clockTime() const { return m_clockTime; }
    QString clockMeridiem() const { return m_clockMeridiem; }
    QString clockDate() const { return m_clockDate; }
    QString greeting() const { return m_greeting; }
    QString schoolName() const { return m_schoolName; }
    QString schoolAddress() const { return m_schoolAddress; }
    QString libraryHours() const { return m_libraryHours; }
    bool guestEnabled() const { return m_guestEnabled; }
    QString statusMessage() const { return m_statusMessage; }
    QString statusSeverity() const { return m_statusSeverity; }

    // QML entry points.
    Q_INVOKABLE void submitLogin(const QString &input);
    Q_INVOKABLE void handleRfidScan(const QString &code);
    Q_INVOKABLE void installRfid(QQuickWindow *window);
    Q_INVOKABLE void requestGuest();

    // Network-free seam: apply an already-parsed student record (used by the
    // network reply handlers AND directly by unit tests).
    void applyStudentLogin(const QJsonObject &student);

    // Network-free seam: decode an admin_login.php / student_login.php
    // response exactly as the reply handler does. On the admin branch it
    // captures heldKey (the admin key that was POSTed) into AdminSession
    // BEFORE emitting adminRequested(). Used by the reply handler AND tests.
    void applyLoginResponse(const QByteArray &json, const QString &heldKey);

signals:
    void currentChanged();
    void statsChanged();
    void clockChanged();
    void schoolInfoChanged();
    void guestEnabledChanged();
    void statusChanged();
    void adminRequested();
    void guestRequested();

private:
    void tickClock();
    void setStatus(const QString &message, const QString &severity);
    void postForm(const QUrl &url, const QString &key, const QString &value,
                  bool rfid);

    RecentLoginsModel m_recent;
    QNetworkAccessManager *m_nam = nullptr;
    QTimer *m_clockTimer = nullptr;
    RfidQuickFilter *m_rfid = nullptr;

    bool m_hasStudent = false;
    QString m_currentName, m_currentFullName, m_currentCourse,
            m_currentYear, m_currentDept, m_currentTime;
    int m_visitorsToday = 0;
    int m_visitorsThisHour = 0;
    int m_currentHour = -1;
    QString m_clockTime, m_clockMeridiem, m_clockDate, m_greeting;
    QString m_schoolName, m_schoolAddress, m_libraryHours;
    bool m_guestEnabled = false;
    QString m_statusMessage, m_statusSeverity;

    // RFID debounce state (LoginParser::shouldDebounceRfid decides).
    QElapsedTimer m_rfidClock;
    QString m_lastRfidCode;
    qint64 m_lastRfidMs = -100000;
};

#endif // KIOSKVIEWMODEL_H
