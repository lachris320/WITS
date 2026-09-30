#include "loginparser.h"

#include <QJsonDocument>
#include <QJsonValue>
#include <QTimeZone>
#include <QUrl>

namespace {

// Constrain an untrusted photo reference to baseUrl's origin. QUrl::resolved()
// on an *absolute* reference (RFC 3986) returns that reference unchanged, so a
// backend/MITM-supplied absolute URL (foreign host, file://, UNC path) would
// otherwise escape the intended host/scheme and bind straight to the kiosk
// Image.source. Accept only same-origin http(s); drop anything else to empty.
QString sameOriginPhotoUrl(const QString &candidate, const QUrl &baseUrl)
{
    if (candidate.isEmpty())
        return QString();
    const QUrl resolved = baseUrl.resolved(QUrl(candidate));
    const QString scheme = resolved.scheme().toLower();
    if (scheme != QLatin1String("http") && scheme != QLatin1String("https"))
        return QString();
    if (resolved.host().compare(baseUrl.host(), Qt::CaseInsensitive) != 0)
        return QString();
    if (resolved.port() != baseUrl.port())
        return QString();
    return resolved.toString();
}

} // namespace

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

EntryEventResult parseEntryEvent(const QByteArray &body, const QUrl &baseUrl)
{
    EntryEventResult r;
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) { r.error = QStringLiteral("Not a JSON object"); return r; }
    const QJsonObject obj = doc.object();
    if (obj.value(QStringLiteral("status")).toString() != QLatin1String("success")) {
        r.error = QStringLiteral("status != success"); return r;
    }
    const QJsonValue latestVal = obj.value(QStringLiteral("latest_id"));
    if (!latestVal.isDouble()) { r.error = QStringLiteral("latest_id not a number"); return r; }
    const qint64 latestId = latestVal.toInteger(-1);
    if (latestId < 0) { r.error = QStringLiteral("latest_id negative"); return r; }

    const QJsonValue entryVal = obj.value(QStringLiteral("entry"));
    if (entryVal.isNull()) {                 // valid empty poll
        r.valid = true; r.latestId = latestId; return r;
    }
    if (!entryVal.isObject()) { r.error = QStringLiteral("entry not object/null"); return r; }

    const QJsonObject entry = entryVal.toObject();
    const QJsonValue idVal = entry.value(QStringLiteral("id"));
    if (!idVal.isDouble()) { r.error = QStringLiteral("entry.id not a number"); return r; }
    const qint64 eventId = idVal.toInteger(-1);
    if (eventId <= 0) { r.error = QStringLiteral("entry.id not positive"); return r; }

    const QString createdAt = entry.value(QStringLiteral("created_at")).toString();
    QDateTime at = QDateTime::fromString(createdAt, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!at.isValid()) { r.error = QStringLiteral("created_at unparseable"); return r; }
    // server-local wall clock (same-host deployment); setTimeSpec is deprecated in Qt 6.11
    at = QDateTime(at.date(), at.time(), QTimeZone::LocalTime);

    const QJsonValue studentVal = entry.value(QStringLiteral("student"));
    QJsonObject student;
    bool hasStudent = false;
    if (studentVal.isObject()) {
        hasStudent = true;
        student = studentVal.toObject();
        // Both the passthrough and the fallback are constrained to baseUrl's
        // origin (see sameOriginPhotoUrl) — the response is untrusted input.
        QString photoUrl =
            sameOriginPhotoUrl(student.value(QStringLiteral("photo_url")).toString(), baseUrl);
        if (photoUrl.isEmpty()) {
            photoUrl =
                sameOriginPhotoUrl(student.value(QStringLiteral("photo_path")).toString(), baseUrl);
        }
        student.insert(QStringLiteral("photo_url"), photoUrl);   // always present
    } else if (!studentVal.isNull()) {
        r.error = QStringLiteral("student not object/null"); return r;   // present but wrong type
    }

    r.valid = true;
    r.hasEntry = true;
    r.latestId = latestId;
    r.eventId = eventId;
    r.hasStudent = hasStudent;
    r.student = student;
    r.at = at.toUTC();
    return r;
}

} // namespace LoginParser
