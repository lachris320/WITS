#ifndef LOGINPARSER_H
#define LOGINPARSER_H

#include <QByteArray>
#include <QJsonObject>
#include <QString>

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

// Numeric (QString::toLongLong succeeds) -> StudentId, else AdminKey.
// Mirrors mainwindow.cpp:193 exactly, including the dashed-ID quirk.
LoginKind classify(const QString &input);

LoginResult parseLoginResponse(const QByteArray &json);
RfidResult  parseRfidResponse(const QByteArray &json);

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
