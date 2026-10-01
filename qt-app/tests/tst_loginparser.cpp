#include <QtTest>
#include <QJsonObject>
#include <QUrl>
#include "apiconfig.h"
#include "loginparser.h"

class TestLoginParser : public QObject
{
    Q_OBJECT
private slots:
    void classifyPureDigitsIsStudent();
    void classifyNonNumericIsAdmin();
    void classifyDashedIdIsAdmin_legacyQuirk();
    void parseStudentSuccess();
    void parseAdminSuccess();
    void parseFailureCarriesMessage();
    void parseInvalidJsonIsNotOk();
    void parseRfidStudentSuccess();
    void parseRfidUnregistered();
    void validRfidCodeAcceptsAlnum();
    void invalidRfidCodeRejectsBounds();
    void debounceIgnoresSameCodeInWindow();
    void debounceAllowsDifferentCode();
    void debounceAllowsSameCodeAfterWindow();
    void parseEntryEvent_emptyPoll();
    void parseEntryEvent_entryWithStudentComposesPhotoUrl();
    void parseEntryEvent_foreignHostPhotoUrlDropped();
    void parseEntryEvent_fileSchemePhotoUrlDropped();
    void parseEntryEvent_sameHostAbsolutePhotoUrlAccepted();
    void parseEntryEvent_defaultPortConfiguredBaseAcceptsPortlessPhoto();
    void parseEntryEvent_noPhotoYieldsEmptyPhotoUrl();
    void parseEntryEvent_orphanedStudentNull();
    void parseEntryEvent_localTimeConvertedToUtc();
    void parseEntryEvent_malformedIsInvalid();
    void parseRecentFeed_validListWithCounts();
    void parseRecentFeed_nullStudentIsUnknownRow();
    void parseRecentFeed_emptyButValid();
    void parseRecentFeed_serverErrorCarriesMessage();
    void parseRecentFeed_malformedIsInvalid();
    void parseRecentFeed_cardAndReaderAreStrict();
};

void TestLoginParser::classifyPureDigitsIsStudent()
{
    QCOMPARE(LoginParser::classify("202300123"), LoginParser::LoginKind::StudentId);
}
void TestLoginParser::classifyNonNumericIsAdmin()
{
    QCOMPARE(LoginParser::classify("letmein"), LoginParser::LoginKind::AdminKey);
}
void TestLoginParser::classifyDashedIdIsAdmin_legacyQuirk()
{
    // "2023-00123".toLongLong() fails (the dash) -> legacy treats it as an
    // admin key. Parity: reproduce exactly, do not "fix" it here.
    QCOMPARE(LoginParser::classify("2023-00123"), LoginParser::LoginKind::AdminKey);
}

void TestLoginParser::parseStudentSuccess()
{
    const QByteArray json =
        R"({"status":"success","student":{"name":"Maria Santos","course":"BSCE"}})";
    LoginParser::LoginResult r = LoginParser::parseLoginResponse(json);
    QVERIFY(r.ok);
    QVERIFY(r.isStudent);
    QVERIFY(!r.isAdmin);
    QCOMPARE(r.student.value("name").toString(), QStringLiteral("Maria Santos"));
}
void TestLoginParser::parseAdminSuccess()
{
    // success with no "student" object -> admin (legacy: else adminWin->show()).
    LoginParser::LoginResult r = LoginParser::parseLoginResponse(R"({"status":"success"})");
    QVERIFY(r.ok);
    QVERIFY(!r.isStudent);
    QVERIFY(r.isAdmin);
}
void TestLoginParser::parseFailureCarriesMessage()
{
    LoginParser::LoginResult r =
        LoginParser::parseLoginResponse(R"({"status":"error","message":"Bad key"})");
    QVERIFY(!r.ok);
    QCOMPARE(r.message, QStringLiteral("Bad key"));
}
void TestLoginParser::parseInvalidJsonIsNotOk()
{
    LoginParser::LoginResult r = LoginParser::parseLoginResponse("<html>nope");
    QVERIFY(!r.ok);
    QVERIFY(!r.message.isEmpty());   // a user-facing "invalid response" string
}

void TestLoginParser::parseRfidStudentSuccess()
{
    const QByteArray json =
        R"({"status":"success","student":{"name":"Jose Ramirez"}})";
    LoginParser::RfidResult r = LoginParser::parseRfidResponse(json);
    QVERIFY(r.ok);
    QCOMPARE(r.student.value("name").toString(), QStringLiteral("Jose Ramirez"));
}
void TestLoginParser::parseRfidUnregistered()
{
    LoginParser::RfidResult r = LoginParser::parseRfidResponse(R"({"status":"error"})");
    QVERIFY(!r.ok);
}

void TestLoginParser::validRfidCodeAcceptsAlnum()
{
    QVERIFY(LoginParser::isValidRfidCode("ABC1"));
    QVERIFY(LoginParser::isValidRfidCode("0004829173"));
}
void TestLoginParser::invalidRfidCodeRejectsBounds()
{
    QVERIFY(!LoginParser::isValidRfidCode(""));          // empty
    QVERIFY(!LoginParser::isValidRfidCode("AB"));        // < 3
    QVERIFY(!LoginParser::isValidRfidCode(QString(65, 'A')));  // > 64
    QVERIFY(!LoginParser::isValidRfidCode("AB C1"));     // space
    QVERIFY(!LoginParser::isValidRfidCode("AB;C1"));     // punctuation
}

void TestLoginParser::debounceIgnoresSameCodeInWindow()
{
    QVERIFY(LoginParser::shouldDebounceRfid("ABC1", 1000, "ABC1", 2000, 2500));
}
void TestLoginParser::debounceAllowsDifferentCode()
{
    QVERIFY(!LoginParser::shouldDebounceRfid("ABC1", 1000, "XYZ9", 2000, 2500));
}
void TestLoginParser::debounceAllowsSameCodeAfterWindow()
{
    QVERIFY(!LoginParser::shouldDebounceRfid("ABC1", 1000, "ABC1", 4000, 2500));
}

void TestLoginParser::parseEntryEvent_emptyPoll()
{
    const QByteArray body = R"({"status":"success","latest_id":5,"entry":null})";
    const auto r = LoginParser::parseEntryEvent(body, QUrl("http://localhost/loams_api/"));
    QVERIFY(r.valid);
    QVERIFY(!r.hasEntry);
    QCOMPARE(r.latestId, Q_INT64_C(5));
}

void TestLoginParser::parseEntryEvent_entryWithStudentComposesPhotoUrl()
{
    const QByteArray body = R"({"status":"success","latest_id":11,"entry":{
        "id":11,"card":"ABC","created_at":"2026-09-29 08:30:00","reader":0,
        "student":{"name":"Jane Cruz","photo_path":"uploads/jane.jpg"}}})";
    const auto r = LoginParser::parseEntryEvent(body, QUrl("http://localhost/loams_api/"));
    QVERIFY(r.valid);
    QVERIFY(r.hasEntry);
    QCOMPARE(r.eventId, Q_INT64_C(11));
    QVERIFY(r.hasStudent);
    QCOMPARE(r.student.value("photo_url").toString(),
             QStringLiteral("http://localhost/loams_api/uploads/jane.jpg"));
    QVERIFY(!r.student.contains("card"));   // raw card never surfaced
}

void TestLoginParser::parseEntryEvent_foreignHostPhotoUrlDropped()
{
    // A backend/MITM-supplied absolute photo_url on a foreign host must NOT be
    // bound to the kiosk Image.source. It is dropped to empty (QML falls back to
    // initials). The photo_path fallback is ALSO foreign here, so it stays empty.
    const QByteArray body = R"({"status":"success","latest_id":1,"entry":{
        "id":1,"created_at":"2026-09-29 08:30:00",
        "student":{"name":"A","photo_url":"http://evil.example/x.png","photo_path":"http://evil.example/y.jpg"}}})";
    const auto r = LoginParser::parseEntryEvent(body, QUrl("http://localhost/loams_api/"));
    QVERIFY(r.hasStudent);
    QVERIFY(r.student.contains("photo_url"));   // key always present
    QVERIFY(r.student.value("photo_url").toString().isEmpty());
}

void TestLoginParser::parseEntryEvent_fileSchemePhotoUrlDropped()
{
    // A file:// URL escapes the http(s) transport entirely (local disclosure).
    const QByteArray body = R"({"status":"success","latest_id":1,"entry":{
        "id":1,"created_at":"2026-09-29 08:30:00",
        "student":{"name":"A","photo_url":"file:///C:/Users/secret.png"}}})";
    const auto r = LoginParser::parseEntryEvent(body, QUrl("http://localhost/loams_api/"));
    QVERIFY(r.hasStudent);
    QVERIFY(r.student.contains("photo_url"));
    QVERIFY(r.student.value("photo_url").toString().isEmpty());
}

void TestLoginParser::parseEntryEvent_sameHostAbsolutePhotoUrlAccepted()
{
    // An absolute photo_url matching baseUrl scheme + host + port is legitimate.
    const QByteArray body = R"({"status":"success","latest_id":1,"entry":{
        "id":1,"created_at":"2026-09-29 08:30:00",
        "student":{"name":"A","photo_url":"http://localhost/loams_api/uploads/ok.png"}}})";
    const auto r = LoginParser::parseEntryEvent(body, QUrl("http://localhost/loams_api/"));
    QVERIFY(r.hasStudent);
    QCOMPARE(r.student.value("photo_url").toString(),
             QStringLiteral("http://localhost/loams_api/uploads/ok.png"));
}

void TestLoginParser::parseEntryEvent_defaultPortConfiguredBaseAcceptsPortlessPhoto()
{
    // BaseURL configured as http://srv.test:80/loams_api/ must be the same
    // origin as a photo_url on http://srv.test/ -- the base is normalized
    // (default port stripped) exactly as the hub receives it.
    const QUrl base(ApiConfig::normalizedBaseUrl(QStringLiteral("http://srv.test:80/loams_api/")));
    const QByteArray body = R"({"status":"success","latest_id":1,"entry":{
        "id":1,"created_at":"2026-09-29 08:30:00",
        "student":{"name":"A","photo_url":"http://srv.test/uploads/x.jpg"}}})";
    const auto r = LoginParser::parseEntryEvent(body, base);
    QVERIFY(r.hasStudent);
    QCOMPARE(r.student.value("photo_url").toString(),
             QStringLiteral("http://srv.test/uploads/x.jpg"));
}

void TestLoginParser::parseEntryEvent_noPhotoYieldsEmptyPhotoUrl()
{
    const QByteArray body = R"({"status":"success","latest_id":1,"entry":{
        "id":1,"created_at":"2026-09-29 08:30:00","student":{"name":"A"}}})";
    const auto r = LoginParser::parseEntryEvent(body, QUrl("http://localhost/loams_api/"));
    QVERIFY(r.hasStudent);
    QVERIFY(r.student.contains("photo_url"));
    QVERIFY(r.student.value("photo_url").toString().isEmpty());
}

void TestLoginParser::parseEntryEvent_orphanedStudentNull()
{
    const QByteArray body = R"({"status":"success","latest_id":9,"entry":{
        "id":9,"created_at":"2026-09-29 08:30:00","student":null}})";
    const auto r = LoginParser::parseEntryEvent(body, QUrl("http://localhost/loams_api/"));
    QVERIFY(r.valid);
    QVERIFY(r.hasEntry);
    QVERIFY(!r.hasStudent);
    QVERIFY(r.student.isEmpty());
}

void TestLoginParser::parseEntryEvent_localTimeConvertedToUtc()
{
    const QByteArray body = R"({"status":"success","latest_id":2,"entry":{
        "id":2,"created_at":"2026-09-29 08:30:00","student":null}})";
    const auto r = LoginParser::parseEntryEvent(body, QUrl("http://localhost/loams_api/"));
    QVERIFY(r.at.isValid());
    QCOMPARE(r.at.timeSpec(), Qt::UTC);
    QDateTime local(QDate(2026, 9, 29), QTime(8, 30, 0));   // Qt::LocalTime
    QCOMPARE(r.at, local.toUTC());
}

void TestLoginParser::parseEntryEvent_malformedIsInvalid()
{
    const QUrl base("http://localhost/loams_api/");
    QVERIFY(!LoginParser::parseEntryEvent("not json", base).valid);
    QVERIFY(!LoginParser::parseEntryEvent(R"({"status":"error"})", base).valid);
    QVERIFY(!LoginParser::parseEntryEvent(
        R"({"status":"success","latest_id":1,"entry":3})", base).valid);
    QVERIFY(!LoginParser::parseEntryEvent(
        R"({"status":"success","latest_id":1,"entry":{"id":0,"created_at":"2026-09-29 08:30:00","student":null}})", base).valid);
    QVERIFY(!LoginParser::parseEntryEvent(
        R"({"status":"success","latest_id":1,"entry":{"id":1,"created_at":"nope","student":null}})", base).valid);
    QVERIFY(!LoginParser::parseEntryEvent(
        R"({"status":"success","latest_id":-1,"entry":null})", base).valid);
    QVERIFY(!LoginParser::parseEntryEvent(
        R"({"status":"success","latest_id":1,"entry":{"id":1,"created_at":"2026-09-29 08:30:00","student":5}})", base).valid);
}

static QByteArray recentFeedBody()
{
    return QByteArray(R"({"status":"success","entries":[
        {"id":12,"card":"CARD0012","created_at":"2026-09-30 08:15:00","reader":1,
         "student":{"name":"Test Student A","school_id":"TEST-0001","course":"BS Test",
                    "department":"Dept Test","photo_path":"uploads/default.jpg"}},
        {"id":11,"card":"CARD0011","created_at":"2026-09-30 08:10:00","reader":2,"student":null}],
        "entries_today":7,"last_entry_at":"2026-09-30 08:15:00"})");
}

void TestLoginParser::parseRecentFeed_validListWithCounts()
{
    const auto r = LoginParser::parseRecentFeed(recentFeedBody());
    QVERIFY(r.valid);
    QCOMPARE(r.entries.size(), 2);
    QCOMPARE(r.entriesToday, 7);
    QCOMPARE(r.lastEntryAt, QStringLiteral("2026-09-30 08:15:00"));
    const LoginParser::RecentEntry &e = r.entries.at(0);   // newest first, as served
    QCOMPARE(e.id, Q_INT64_C(12));
    QCOMPARE(e.card, QStringLiteral("CARD0012"));
    QCOMPARE(e.createdAt, QStringLiteral("2026-09-30 08:15:00"));
    QCOMPARE(e.reader, 1);
    QVERIFY(e.known);
    QCOMPARE(e.name, QStringLiteral("Test Student A"));
    QCOMPARE(e.schoolId, QStringLiteral("TEST-0001"));
    QCOMPARE(e.course, QStringLiteral("BS Test"));
    QCOMPARE(e.department, QStringLiteral("Dept Test"));
}

void TestLoginParser::parseRecentFeed_nullStudentIsUnknownRow()
{
    const auto r = LoginParser::parseRecentFeed(recentFeedBody());
    QVERIFY(r.valid);
    const LoginParser::RecentEntry &e = r.entries.at(1);
    QCOMPARE(e.id, Q_INT64_C(11));
    QVERIFY(!e.known);
    QVERIFY(e.name.isEmpty());
    QVERIFY(e.schoolId.isEmpty());
    QCOMPARE(e.card, QStringLiteral("CARD0011"));   // still carried for the admin
    QCOMPARE(e.reader, 2);
}

void TestLoginParser::parseRecentFeed_emptyButValid()
{
    const auto r = LoginParser::parseRecentFeed(
        R"({"status":"success","entries":[],"entries_today":0,"last_entry_at":null})");
    QVERIFY(r.valid);                    // an empty feed is NOT an error
    QVERIFY(r.entries.isEmpty());
    QCOMPARE(r.entriesToday, 0);
    QVERIFY(r.lastEntryAt.isEmpty());    // null -> ""
}

void TestLoginParser::parseRecentFeed_serverErrorCarriesMessage()
{
    const auto r = LoginParser::parseRecentFeed(
        R"({"status":"error","message":"Invalid admin key"})");
    QVERIFY(!r.valid);
    QCOMPARE(r.error, QStringLiteral("Invalid admin key"));
}

void TestLoginParser::parseRecentFeed_malformedIsInvalid()
{
    QVERIFY(!LoginParser::parseRecentFeed("not json").valid);
    QVERIFY(!LoginParser::parseRecentFeed(R"({"status":"error"})").valid);
    QVERIFY(!LoginParser::parseRecentFeed(       // entries not an array
        R"({"status":"success","entries":{},"entries_today":0,"last_entry_at":null})").valid);
    QVERIFY(!LoginParser::parseRecentFeed(       // entries_today missing
        R"({"status":"success","entries":[],"last_entry_at":null})").valid);
    QVERIFY(!LoginParser::parseRecentFeed(       // entries_today negative
        R"({"status":"success","entries":[],"entries_today":-1,"last_entry_at":null})").valid);
    QVERIFY(!LoginParser::parseRecentFeed(       // last_entry_at wrong type
        R"({"status":"success","entries":[],"entries_today":0,"last_entry_at":5})").valid);
    QVERIFY(!LoginParser::parseRecentFeed(       // entry not an object
        R"({"status":"success","entries":[3],"entries_today":1,"last_entry_at":null})").valid);
    QVERIFY(!LoginParser::parseRecentFeed(       // id not positive
        R"({"status":"success","entries":[{"id":0,"card":"C","created_at":"2026-09-30 08:00:00","reader":0,"student":null}],"entries_today":1,"last_entry_at":null})").valid);
    QVERIFY(!LoginParser::parseRecentFeed(       // created_at not a string
        R"({"status":"success","entries":[{"id":1,"card":"C","created_at":5,"reader":0,"student":null}],"entries_today":1,"last_entry_at":null})").valid);
    QVERIFY(!LoginParser::parseRecentFeed(       // student wrong type
        R"({"status":"success","entries":[{"id":1,"card":"C","created_at":"2026-09-30 08:00:00","reader":0,"student":5}],"entries_today":1,"last_entry_at":null})").valid);
    QVERIFY(!LoginParser::parseRecentFeed(       // id missing
        R"({"status":"success","entries":[{"card":"C","created_at":"2026-09-30 08:00:00","reader":0,"student":null}],"entries_today":1,"last_entry_at":null})").valid);
    QVERIFY(!LoginParser::parseRecentFeed(       // id non-number
        R"({"status":"success","entries":[{"id":"1","card":"C","created_at":"2026-09-30 08:00:00","reader":0,"student":null}],"entries_today":1,"last_entry_at":null})").valid);
    QVERIFY(!LoginParser::parseRecentFeed(       // created_at missing
        R"({"status":"success","entries":[{"id":1,"card":"C","reader":0,"student":null}],"entries_today":1,"last_entry_at":null})").valid);
}

void TestLoginParser::parseRecentFeed_cardAndReaderAreStrict()
{
    // card must be a JSON string and reader a JSON number — never silently
    // defaulted to ""/0 (a shape drift must fail loudly, not render blanks).
    const auto cardMissing = LoginParser::parseRecentFeed(
        R"({"status":"success","entries":[{"id":1,"created_at":"2026-09-30 08:00:00","reader":0,"student":null}],"entries_today":1,"last_entry_at":null})");
    QVERIFY(!cardMissing.valid);
    QVERIFY(!cardMissing.error.isEmpty());

    const auto cardNonString = LoginParser::parseRecentFeed(
        R"({"status":"success","entries":[{"id":1,"card":1234,"created_at":"2026-09-30 08:00:00","reader":0,"student":null}],"entries_today":1,"last_entry_at":null})");
    QVERIFY(!cardNonString.valid);
    QVERIFY(!cardNonString.error.isEmpty());

    const auto readerMissing = LoginParser::parseRecentFeed(
        R"({"status":"success","entries":[{"id":1,"card":"C","created_at":"2026-09-30 08:00:00","student":null}],"entries_today":1,"last_entry_at":null})");
    QVERIFY(!readerMissing.valid);
    QVERIFY(!readerMissing.error.isEmpty());

    const auto readerNonNumber = LoginParser::parseRecentFeed(
        R"({"status":"success","entries":[{"id":1,"card":"C","created_at":"2026-09-30 08:00:00","reader":"1","student":null}],"entries_today":1,"last_entry_at":null})");
    QVERIFY(!readerNonNumber.valid);
    QVERIFY(!readerNonNumber.error.isEmpty());

    // Control: the same row with a string card and numeric reader is valid.
    QVERIFY(LoginParser::parseRecentFeed(
        R"({"status":"success","entries":[{"id":1,"card":"C","created_at":"2026-09-30 08:00:00","reader":0,"student":null}],"entries_today":1,"last_entry_at":null})").valid);
}

QTEST_MAIN(TestLoginParser)
#include "tst_loginparser.moc"
