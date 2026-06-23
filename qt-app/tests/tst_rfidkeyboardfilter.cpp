#include <QtTest>
#include "rfidkeyboardfilter.h"

class TestRfidKeyboardFilter : public QObject
{
    Q_OBJECT
private slots:
    void returnKeyIsTerminator();
    void enterKeyIsTerminator();
    void letterKeyIsNotTerminator();
};

void TestRfidKeyboardFilter::returnKeyIsTerminator()
{
    QVERIFY(RfidKeyboardFilter::isTerminator(Qt::Key_Return));
}

void TestRfidKeyboardFilter::enterKeyIsTerminator()
{
    QVERIFY(RfidKeyboardFilter::isTerminator(Qt::Key_Enter)); // keypad Enter
}

void TestRfidKeyboardFilter::letterKeyIsNotTerminator()
{
    QVERIFY(!RfidKeyboardFilter::isTerminator(Qt::Key_A));
}

QTEST_GUILESS_MAIN(TestRfidKeyboardFilter)
#include "tst_rfidkeyboardfilter.moc"
