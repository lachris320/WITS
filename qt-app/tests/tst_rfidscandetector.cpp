#include <QtTest>
#include "rfidscandetector.h"

class TestRfidScanDetector : public QObject
{
    Q_OBJECT
private slots:
    void fastBurstReturnsCode();
    void humanPacedReturnsNullopt();
    void slowThenFastReturnsFastTail();
    void overflowResetsBuffer();
    void burstWithoutTerminatorEmitsNothing();
    void belowMinLengthRejected();
    void carriageReturnIsTerminator();
};

void TestRfidScanDetector::fastBurstReturnsCode()
{
    RfidScanDetector d(60, 3, 64);
    QVERIFY(!d.feedKey('A', 0).has_value());
    QVERIFY(!d.feedKey('B', 5).has_value());
    QVERIFY(!d.feedKey('C', 10).has_value());
    QVERIFY(!d.feedKey('1', 15).has_value());
    auto result = d.feedKey('\n', 20); // terminator
    QVERIFY(result.has_value());
    QCOMPARE(*result, QString("ABC1"));
}

void TestRfidScanDetector::humanPacedReturnsNullopt()
{
    RfidScanDetector d(60, 3, 64);
    d.feedKey('1', 0);
    d.feedKey('2', 200);   // 200ms gap > 60 -> human pace
    d.feedKey('3', 400);
    d.feedKey('4', 600);
    auto result = d.feedKey('\n', 800);
    QVERIFY(!result.has_value());
}

void TestRfidScanDetector::slowThenFastReturnsFastTail()
{
    RfidScanDetector d(60, 3, 64);
    d.feedKey('X', 0);
    d.feedKey('Y', 300);   // slow
    d.feedKey('A', 600);   // slow gap -> burst restarts here
    d.feedKey('B', 605);   // fast
    d.feedKey('C', 610);
    d.feedKey('D', 615);
    auto result = d.feedKey('\n', 620);
    QVERIFY(result.has_value());
    QCOMPARE(*result, QString("ABCD"));
}

void TestRfidScanDetector::overflowResetsBuffer()
{
    RfidScanDetector d(60, 3, 8); // small cap for the test
    qint64 t = 0;
    for (int i = 0; i < 9; ++i) // 9 fast chars > cap of 8
        d.feedKey('A', t++);
    auto result = d.feedKey('\n', t);
    QVERIFY(!result.has_value()); // overflow emptied the buffer
}

void TestRfidScanDetector::burstWithoutTerminatorEmitsNothing()
{
    RfidScanDetector d(60, 3, 64);
    QVERIFY(!d.feedKey('A', 0).has_value());
    QVERIFY(!d.feedKey('B', 5).has_value());
    QVERIFY(!d.feedKey('C', 10).has_value());
}

void TestRfidScanDetector::belowMinLengthRejected()
{
    RfidScanDetector d(60, 3, 64);
    d.feedKey('A', 0);
    d.feedKey('B', 5);
    auto result = d.feedKey('\n', 10); // only 2 chars, min is 3
    QVERIFY(!result.has_value());
}

void TestRfidScanDetector::carriageReturnIsTerminator()
{
    RfidScanDetector d(60, 3, 64);
    d.feedKey('A', 0);
    d.feedKey('B', 5);
    d.feedKey('C', 10);
    auto result = d.feedKey('\r', 15); // CR, not LF
    QVERIFY(result.has_value());
    QCOMPARE(*result, QString("ABC"));
}

QTEST_APPLESS_MAIN(TestRfidScanDetector)
#include "tst_rfidscandetector.moc"
