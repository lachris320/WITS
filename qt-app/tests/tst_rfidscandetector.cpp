#include <QtTest>
#include "rfidscandetector.h"

class TestRfidScanDetector : public QObject
{
    Q_OBJECT
private slots:
    void fastBurstReturnsCode();
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

QTEST_APPLESS_MAIN(TestRfidScanDetector)
#include "tst_rfidscandetector.moc"
