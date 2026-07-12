#include <QtTest>
#include <QJsonObject>
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

QTEST_MAIN(TestLoginParser)
#include "tst_loginparser.moc"
