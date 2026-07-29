#include "KioskViewModel.h"

#include <QNetworkAccessManager>
#include <QTimer>
#include <QSettings>

#include "appsettings.h"
#include <QDateTime>
#include <QQuickWindow>   // complete type: installRfid() calls window->installEventFilter()
#include "apiconfig.h"
#include "loginparser.h"
#include "HttpForm.h"
#include "RfidQuickFilter.h"
#include "AdminSession.h"
#include "SchoolInfoUtil.h"

KioskViewModel::KioskViewModel(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_clockTimer(new QTimer(this))
{
    m_rfidClock.start();

    // School info + guest toggle: cache the legacy QSettings keys. The admin
    // Settings screen writes these live; loadSchoolInfo() is re-run by
    // reload() whenever the kiosk surface comes back into view.
    AppSettings s;
    loadSchoolInfo(s);
    m_guestEnabled = s.value(QStringLiteral("kiosk/guestEnabled"), false).toBool();

    connect(m_clockTimer, &QTimer::timeout, this, &KioskViewModel::tickClock);
    m_clockTimer->start(1000);
    tickClock();   // populate immediately, no 1s blank
}

bool KioskViewModel::loadSchoolInfo(QSettings &settings)
{
    const QString name    = settings.value(QStringLiteral("school/name")).toString();
    const QString address = settings.value(QStringLiteral("school/address")).toString();
    const QString hours   = settings.value(QStringLiteral("school/libraryHours"),
                                           QStringLiteral("6 AM – 5 PM")).toString();

    // Graceful-degradation logo seam, shared with SchoolInfoViewModel via
    // SchoolInfoUtil::resolveLogoUrl so the two surfaces cannot drift: QML
    // keys its placeholder fallback off hasLogo, never off "is logoUrl
    // non-empty" or a raw path string, so a rotted path can never reach an
    // Image element. Re-statting on every reload is what lets a deleted logo
    // flip hasLogo back to false.
    const QString logoPath = settings.value(QStringLiteral("school/logoPath")).toString();
    bool hasLogo = false;
    const QUrl logoUrl = SchoolInfoUtil::resolveLogoUrl(logoPath, &hasLogo);

    const bool changed = name != m_schoolName || address != m_schoolAddress
                      || hours != m_libraryHours || hasLogo != m_hasLogo
                      || logoUrl != m_logoUrl;

    m_schoolName    = name;
    m_schoolAddress = address;
    m_libraryHours  = hours;
    m_hasLogo       = hasLogo;
    m_logoUrl       = logoUrl;
    return changed;
}

void KioskViewModel::reload()
{
    // The writer (SettingsController::save) uses its own QSettings object over
    // the same store; sync() is what makes those writes visible here.
    AppSettings s;
    s.sync();
    if (loadSchoolInfo(s))
        emit schoolInfoChanged();
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

void KioskViewModel::applyLoginResponse(const QByteArray &body, const QString &heldKey)
{
    const LoginParser::LoginResult r = LoginParser::parseLoginResponse(body);
    if (r.ok && r.isStudent) {
        applyStudentLogin(r.student);
    } else if (r.ok && r.isAdmin) {
        AdminSession::instance().setKey(heldKey);   // §3.3 capture, before the signal
        emit adminRequested();
    } else {
        setStatus(r.message, QStringLiteral("Error"));
    }
}

void KioskViewModel::postForm(const QUrl &url, const QString &key,
                              const QString &value, bool rfid)
{
    HttpForm::submit(m_nam, url, {{key, value}}, this,
        [this, rfid, value](const QByteArray &body) {
            if (rfid) {
                const LoginParser::RfidResult r = LoginParser::parseRfidResponse(body);
                if (r.ok) applyStudentLogin(r.student);
                else      setStatus(r.message, QStringLiteral("Error"));
                return;
            }
            // Non-RFID: student_login.php or admin_login.php. `value` is the
            // POSTed credential — for the admin branch it IS the admin key, so
            // the reply handler already has the key in scope (spec §3.3's
            // "thread it through postForm into the reply handler").
            applyLoginResponse(body, value);
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
