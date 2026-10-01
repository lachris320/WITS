#ifndef LOGINPARSER_H
#define LOGINPARSER_H

#include <QByteArray>
#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QUrl>
#include <QVector>

// Pure, widget-free, network-free decode + decision logic for the kiosk login
// flows (student / admin / RFID / debounce). Extracted from mainwindow.cpp so
// it is unit-testable against synthetic payloads with no QNetworkAccessManager.
namespace LoginParser {

enum class LoginKind { StudentId, AdminKey };

struct LoginResult {
    bool ok = false;          // status == "success"
    bool isStudent = false;   // success AND a "student" object is present
    bool isAdmin = false;     // success AND no "student" object
    QJsonObject student;
    QString message;          // user-facing failure/invalid message when !ok
};

struct RfidResult {
    bool ok = false;          // success AND a "student" object is present
    QJsonObject student;
    QString message;
};

struct EntryEventResult {
    bool        valid      = false;   // false = malformed JSON/schema (protocol failure)
    bool        hasEntry   = false;   // valid && an entry row was returned
    qint64      latestId   = 0;       // MAX(id); 0 on empty table
    qint64      eventId    = 0;       // the returned entry's id (cursor advance target)
    bool        hasStudent = false;   // entry present && student resolved
    QJsonObject student;              // normalized student incl. photo_url; empty if unresolved
    QDateTime   at;                   // entry time, UTC (converted from server-local)
    QString     error;                // reason when !valid
};

// One row of access_recent.php (admin Access Control feed). Text only — the
// endpoint's photo_path is deliberately NOT carried (no thumbnails this slice).
struct RecentEntry {
    qint64  id       = 0;
    QString card;              // raw card (admin-authenticated feed only)
    QString createdAt;         // server-local "yyyy-MM-dd HH:mm:ss", verbatim
    int     reader   = 0;      // gate/lane number
    bool    known    = false;  // false when the endpoint's student is null
    QString name;
    QString schoolId;
    QString course;
    QString department;
};

struct RecentFeedResult {
    bool                 valid        = false;  // false = malformed / wrong shape / non-success
    QVector<RecentEntry> entries;               // newest first, as served
    int                  entriesToday = 0;
    QString              lastEntryAt;           // "" when the server sent null (no entries ever)
    QString              error;                 // reason when !valid (server message if any)
};

// Pure decode of access_recent.php. valid is true for a well-formed
// status:"success" response INCLUDING an empty entries array.
RecentFeedResult parseRecentFeed(const QByteArray &body);

// Numeric (QString::toLongLong succeeds) -> StudentId, else AdminKey.
// Mirrors mainwindow.cpp:193 exactly, including the dashed-ID quirk.
LoginKind classify(const QString &input);

LoginResult parseLoginResponse(const QByteArray &json);
RfidResult  parseRfidResponse(const QByteArray &json);

// Pure decode of turnstile_display.php. photo_url is composed from a relative
// photo_path against baseUrl (parser stays ApiConfig-free). See design spec §1.
EntryEventResult parseEntryEvent(const QByteArray &body, const QUrl &baseUrl);

// Charset/length gate before POSTing a scanned code to rfid_login.php:
// non-empty, 3..64 chars, ASCII alphanumeric only.
bool isValidRfidCode(const QString &code);

// Same code re-seen within windowMs -> true (ignore this scan). One tap = one
// visit. Mirrors mainwindow.cpp:301-303.
bool shouldDebounceRfid(const QString &lastCode, qint64 lastMs,
                        const QString &code, qint64 nowMs,
                        qint64 windowMs = 2500);

} // namespace LoginParser

#endif // LOGINPARSER_H
