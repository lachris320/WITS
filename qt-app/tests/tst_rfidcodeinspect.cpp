#include <QtTest>
#include "rfidcodeinspect.h"

class TstRfidCodeInspect : public QObject
{
    Q_OBJECT
private slots:
    void preservesLeadingZeros();
    void reportsLengthAndHex();
    void exposesHiddenWhitespace();
};

void TstRfidCodeInspect::preservesLeadingZeros()
{
    // The whole point: a 10-digit code with leading zeros must survive verbatim.
    QCOMPARE(RfidCodeInspect::describe("0003241370"),
             QStringLiteral("value=\"0003241370\" len=10 hex=30 30 30 33 32 34 31 33 37 30"));
}

void TstRfidCodeInspect::reportsLengthAndHex()
{
    QCOMPARE(RfidCodeInspect::describe("12"),
             QStringLiteral("value=\"12\" len=2 hex=31 32"));
}

void TstRfidCodeInspect::exposesHiddenWhitespace()
{
    // A stray leading space or trailing CR is the kind of thing the hex dump must reveal.
    QCOMPARE(RfidCodeInspect::describe(" 12\r"),
             QStringLiteral("value=\" 12\r\" len=4 hex=20 31 32 0d"));
}

QTEST_APPLESS_MAIN(TstRfidCodeInspect)
#include "tst_rfidcodeinspect.moc"
