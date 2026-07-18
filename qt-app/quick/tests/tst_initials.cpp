#include <QtTest>
#include "Initials.h"

// Unit test for the PURE, Qt-Gui-free Initials::of() helper (quick/Initials.h),
// extracted from RecentLoginsModel so RecentLoginsModel (kiosk feed) and
// SearchResultsModel (admin search results) share one implementation instead
// of two copies that could drift.
class TestInitials : public QObject
{
    Q_OBJECT
private slots:
    void emptyNameYieldsEmptyString();
    void singleNameYieldsOneLetter();
    void twoWordNameYieldsBothInitials();
    void extraWordsAreIgnoredBeyondTwo();
    void extraSpacesAreSkipped();
    void lowercaseFirstLettersAreUppercased();
};

void TestInitials::emptyNameYieldsEmptyString()
{
    QCOMPARE(Initials::of(QString()), QString());
    QCOMPARE(Initials::of(QStringLiteral("")), QString());
}

void TestInitials::singleNameYieldsOneLetter()
{
    QCOMPARE(Initials::of(QStringLiteral("Cher")), QStringLiteral("C"));
}

void TestInitials::twoWordNameYieldsBothInitials()
{
    QCOMPARE(Initials::of(QStringLiteral("Maria Santos")), QStringLiteral("MS"));
}

void TestInitials::extraWordsAreIgnoredBeyondTwo()
{
    // Only the first two words contribute — a third/fourth given name must
    // not push the chip past two characters.
    QCOMPARE(Initials::of(QStringLiteral("Maria Clara Santos Reyes")), QStringLiteral("MC"));
}

void TestInitials::extraSpacesAreSkipped()
{
    // Leading/doubled/trailing spaces must not produce an empty "word" whose
    // first letter would be read (that would crash on p.at(0) or insert a
    // stray character) — Qt::SkipEmptyParts is load-bearing here.
    QCOMPARE(Initials::of(QStringLiteral("  Ana   Reyes  ")), QStringLiteral("AR"));
}

void TestInitials::lowercaseFirstLettersAreUppercased()
{
    QCOMPARE(Initials::of(QStringLiteral("maria santos")), QStringLiteral("MS"));
}

QTEST_MAIN(TestInitials)
#include "tst_initials.moc"
