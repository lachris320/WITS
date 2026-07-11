#include "loginparser.h"

#include <QJsonDocument>

namespace LoginParser {

LoginKind classify(const QString &input)
{
    bool ok = false;
    input.toLongLong(&ok);            // same numeric test the legacy kiosk uses
    return ok ? LoginKind::StudentId : LoginKind::AdminKey;
}

LoginResult parseLoginResponse(const QByteArray &json)
{
    LoginResult r;
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        r.message = QStringLiteral("Invalid server response.");
        return r;
    }
    const QJsonObject obj = doc.object();
    if (obj.value("status").toString() == QLatin1String("success")) {
        r.ok = true;
        if (obj.contains("student")) {
            r.isStudent = true;
            r.student = obj.value("student").toObject();
        } else {
            r.isAdmin = true;
        }
        return r;
    }
    r.message = obj.value("message").toString();
    if (r.message.isEmpty())
        r.message = QStringLiteral("Login failed. Please check your ID or Admin Key.");
    return r;
}

RfidResult parseRfidResponse(const QByteArray &json)
{
    RfidResult r;
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        r.message = QStringLiteral("Invalid server response.");
        return r;
    }
    const QJsonObject obj = doc.object();
    if (obj.value("status").toString() == QLatin1String("success")
        && obj.contains("student")) {
        r.ok = true;
        r.student = obj.value("student").toObject();
        return r;
    }
    r.message = QStringLiteral("Card not registered. Please see the librarian.");
    return r;
}

bool isValidRfidCode(const QString &code)
{
    if (code.size() < 3 || code.size() > 64)
        return false;
    for (const QChar c : code) {
        if (!((c >= QLatin1Char('0') && c <= QLatin1Char('9'))
           || (c >= QLatin1Char('A') && c <= QLatin1Char('Z'))
           || (c >= QLatin1Char('a') && c <= QLatin1Char('z'))))
            return false;
    }
    return true;
}

bool shouldDebounceRfid(const QString &lastCode, qint64 lastMs,
                        const QString &code, qint64 nowMs, qint64 windowMs)
{
    return code == lastCode && (nowMs - lastMs) < windowMs;
}

} // namespace LoginParser
