#include "KioskViewModel.h"

#include <QNetworkAccessManager>
#include <QTimer>
#include <QSettings>
#include <QDateTime>
#include <QQuickWindow>   // complete type: installRfid() calls window->installEventFilter()
#include "apiconfig.h"
#include "loginparser.h"
#include "HttpForm.h"
#include "RfidQuickFilter.h"

KioskViewModel::KioskViewModel(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_clockTimer(new QTimer(this))
{
    m_rfidClock.start();

    // School info + guest toggle: cache the legacy QSettings keys. The admin
    // surface (Phase 4) will write these live; here we read them at startup.
    QSettings s(QStringLiteral("MyCompany"), QStringLiteral("MyApp"));
    m_schoolName    = s.value(QStringLiteral("school/name")).toString();
    m_schoolAddress = s.value(QStringLiteral("school/address")).toString();
    m_libraryHours  = s.value(QStringLiteral("school/libraryHours"),
                              QStringLiteral("6 AM – 5 PM")).toString();
    m_guestEnabled  = s.value(QStringLiteral("kiosk/guestEnabled"), false).toBool();

    connect(m_clockTimer, &QTimer::timeout, this, &KioskViewModel::tickClock);
    m_clockTimer->start(1000);
    tickClock();   // populate immediately, no 1s blank
}

void KioskViewModel::tickClock()
{
    const QDateTime now = QDateTime::currentDateTime();
    const int h24 = now.time().hour();

    m_clockMeridiem = h24 >= 12 ? QStringLiteral("PM") : QStringLiteral("AM");
    m_clockTime = now.toString(QStringLiteral("h:mm:ss"));
    m_clockDate = now.toString(QStringLiteral("dddd, MMMM d, yyyy"));
    m_greeting  = h24 < 12 ? QStringLiteral("Good morning")
                : h24 < 17 ? QStringLiteral("Good afternoon")
                           : QStringLiteral("Good evening");

    // This-hour stat rolls over when the clock hour changes.
    if (m_currentHour != h24) {
        m_currentHour = h24;
        if (m_visitorsThisHour != 0) {
            m_visitorsThisHour = 0;
            emit statsChanged();
        }
    }
    emit clockChanged();
}

void KioskViewModel::setStatus(const QString &message, const QString &severity)
{
    m_statusMessage = message;
    m_statusSeverity = severity;
    emit statusChanged();
}

void KioskViewModel::applyStudentLogin(const QJsonObject &student)
{
    m_currentFullName = student.value(QStringLiteral("name")).toString();
    m_currentCourse   = student.value(QStringLiteral("course")).toString();
    m_currentYear     = student.value(QStringLiteral("year_level")).toString();
    m_currentDept     = student.value(QStringLiteral("department")).toString();
    m_currentTime     = student.value(QStringLiteral("time_date")).toString();
    m_currentName     = m_currentFullName.section(QLatin1Char(' '), 0, 0);  // first name
    m_hasStudent      = true;

    m_recent.prepend(m_currentFullName, m_currentCourse, m_currentYear,
                     m_currentDept, m_currentTime);

    ++m_visitorsToday;
    ++m_visitorsThisHour;

    emit currentChanged();
    emit statsChanged();
    setStatus(QString(), QString());   // clear any prior error toast
}

void KioskViewModel::postForm(const QUrl &url, const QString &key,
                              const QString &value, bool rfid)
{
    HttpForm::submit(m_nam, url, {{key, value}}, this,
        [this, rfid](const QByteArray &body) {
            if (rfid) {
                const LoginParser::RfidResult r = LoginParser::parseRfidResponse(body);
                if (r.ok) applyStudentLogin(r.student);
                else      setStatus(r.message, QStringLiteral("Error"));
                return;
            }
            const LoginParser::LoginResult r = LoginParser::parseLoginResponse(body);
            if (r.ok && r.isStudent)      applyStudentLogin(r.student);
            else if (r.ok && r.isAdmin)   emit adminRequested();
            else                          setStatus(r.message, QStringLiteral("Error"));
        },
        [this]() {
            setStatus(QStringLiteral("Network error. Please try again."),
                      QStringLiteral("Error"));
        });
}

void KioskViewModel::submitLogin(const QString &input)
{
    const QString trimmed = input.trimmed();
    if (trimmed.isEmpty()) {
        setStatus(QStringLiteral("Please enter your School ID or Admin Key."),
                  QStringLiteral("Error"));
        return;
    }
    if (LoginParser::classify(trimmed) == LoginParser::LoginKind::StudentId)
        postForm(ApiConfig::endpoint(QStringLiteral("student_login.php")),
                 QStringLiteral("school_id"), trimmed, false);
    else
        postForm(ApiConfig::endpoint(QStringLiteral("admin_login.php")),
                 QStringLiteral("admin_key"), trimmed, false);
}

void KioskViewModel::handleRfidScan(const QString &code)
{
    const qint64 now = m_rfidClock.elapsed();
    if (LoginParser::shouldDebounceRfid(m_lastRfidCode, m_lastRfidMs, code, now))
        return;                               // one tap = one visit
    m_lastRfidCode = code;
    m_lastRfidMs = now;

    if (!LoginParser::isValidRfidCode(code)) {
        setStatus(QStringLiteral("Card not registered. Please see the librarian."),
                  QStringLiteral("Error"));
        return;                               // reject before any backend POST
    }
    postForm(ApiConfig::endpoint(QStringLiteral("rfid_login.php")),
             QStringLiteral("rfid_id"), code, true);
}

void KioskViewModel::installRfid(QQuickWindow *window)
{
    if (m_rfid || !window)
        return;
    m_rfid = new RfidQuickFilter(window, this);
    window->installEventFilter(m_rfid);
    connect(m_rfid, &RfidQuickFilter::rfidScanned,
            this, &KioskViewModel::handleRfidScan);
}

void KioskViewModel::requestGuest()
{
    emit guestRequested();
}
