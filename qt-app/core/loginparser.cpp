#include "loginparser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QTimeZone>
#include <QUrl>

namespace {

// Effective port: the explicit port, or the scheme's default (80 http, 443
// https). The base URL is normalized with its default port stripped (-1), so
// raw port() values would wrongly mismatch "http://srv/" vs "http://srv:80/".
int effectivePort(const QUrl &url)
{
    const QString scheme = url.scheme().toLower();
    const int fallback = scheme == QLatin1String("https") ? 443
                         : scheme == QLatin1String("http") ? 80
                                                          : -1;
    return url.port(fallback);
}

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
    // Effective-port comparison. Scheme equality is deliberately NOT enforced
    // (deferred to the TLS track), but an https base (443) vs an http photo
    // (80) on the same host now mismatches on port -- stricter, not looser.
    if (effectivePort(resolved) != effectivePort(baseUrl))
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

RecentFeedResult parseRecentFeed(const QByteArray &body)
{
    RecentFeedResult r;
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) { r.error = QStringLiteral("Not a JSON object"); return r; }
    const QJsonObject obj = doc.object();

    if (obj.value(QStringLiteral("status")).toString() != QLatin1String("success")) {
        // Carry the server's message (e.g. requireAdminAuth's "Invalid admin
        // key") so the caller can tell an auth rejection from a generic error.
        const QString msg = obj.value(QStringLiteral("message")).toString();
        r.error = msg.isEmpty() ? QStringLiteral("status != success") : msg;
        return r;
    }

    const QJsonValue entriesVal = obj.value(QStringLiteral("entries"));
    if (!entriesVal.isArray()) { r.error = QStringLiteral("entries not an array"); return r; }

    const QJsonValue todayVal = obj.value(QStringLiteral("entries_today"));
    if (!todayVal.isDouble() || todayVal.toInteger(-1) < 0) {
        r.error = QStringLiteral("entries_today not a non-negative number"); return r;
    }

    const QJsonValue lastVal = obj.value(QStringLiteral("last_entry_at"));
    if (!lastVal.isString() && !lastVal.isNull()) {
        r.error = QStringLiteral("last_entry_at not string/null"); return r;
    }

    const QJsonArray arr = entriesVal.toArray();
    QVector<RecentEntry> entries;
    entries.reserve(arr.size());
    for (const QJsonValue &v : arr) {
        if (!v.isObject()) { r.error = QStringLiteral("entry not an object"); return r; }
        const QJsonObject e = v.toObject();

        const QJsonValue idVal = e.value(QStringLiteral("id"));
        if (!idVal.isDouble() || idVal.toInteger(-1) <= 0) {
            r.error = QStringLiteral("entry.id not positive"); return r;
        }
        const QJsonValue createdVal = e.value(QStringLiteral("created_at"));
        if (!createdVal.isString()) { r.error = QStringLiteral("created_at not a string"); return r; }
        // Strict: missing (Undefined) or wrong-typed card/reader is a shape
        // failure, never silently defaulted to ""/0.
        const QJsonValue cardVal = e.value(QStringLiteral("card"));
        if (!cardVal.isString()) { r.error = QStringLiteral("card not a string"); return r; }
        const QJsonValue readerVal = e.value(QStringLiteral("reader"));
        if (!readerVal.isDouble()) { r.error = QStringLiteral("reader not a number"); return r; }

        RecentEntry out;
        out.id = idVal.toInteger();
        out.card = cardVal.toString();
        out.createdAt = createdVal.toString();
        out.reader = readerVal.toInt();

        const QJsonValue studentVal = e.value(QStringLiteral("student"));
        if (studentVal.isObject()) {
            const QJsonObject s = studentVal.toObject();
            out.known = true;
            out.name = s.value(QStringLiteral("name")).toString();
            out.schoolId = s.value(QStringLiteral("school_id")).toString();
            out.course = s.value(QStringLiteral("course")).toString();
            out.department = s.value(QStringLiteral("department")).toString();
        } else if (!studentVal.isNull()) {
            r.error = QStringLiteral("student not object/null"); return r;
        }
        entries.append(out);
    }

    r.valid = true;
    r.entries = entries;
    r.entriesToday = static_cast<int>(todayVal.toInteger());
    r.lastEntryAt = lastVal.isString() ? lastVal.toString() : QString();
    return r;
}

} // namespace LoginParser
