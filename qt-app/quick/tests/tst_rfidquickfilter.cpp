#include <QtTest>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QQuickWindow>
#include <QSignalSpy>
#include "RfidQuickFilter.h"

// Promotes eventFilter to public and forces the active-window gate open so
// the burst can be driven synthetically under the offscreen QPA (which does
// not deliver real window activation). This mirrors tst_rfidkeyboardfilter's
// PublicFilter idiom (tst_rfidkeyboardfilter.cpp:11-20).
class PublicQuickFilter : public RfidQuickFilter
{
public:
    using RfidQuickFilter::RfidQuickFilter;
    bool callEventFilter(QObject *watched, QEvent *event)
    {
        return eventFilter(watched, event);
    }
protected:
    bool gateOpen(QObject *) const override { return m_forceOpen; }
public:
    bool m_forceOpen = true;
};

class TestRfidQuickFilter : public QObject
{
    Q_OBJECT
private slots:
    void terminatorIsRecognized();
    void recognizedScanEmitsAndConsumesEnter();
    void closedGateIgnoresScan();
    void printableKeyNotConsumed();
};

static bool feedPrintable(PublicQuickFilter &f, QQuickWindow &w,
                          Qt::Key key, const QString &text)
{
    QKeyEvent ev(QEvent::KeyPress, key, Qt::NoModifier, text);
    return f.callEventFilter(&w, &ev);
}

static bool feedTerminator(PublicQuickFilter &f, QQuickWindow &w)
{
    QKeyEvent term(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier,
                   QStringLiteral("\r"));
    return f.callEventFilter(&w, &term);
}

void TestRfidQuickFilter::terminatorIsRecognized()
{
    QVERIFY(RfidQuickFilter::isTerminator(Qt::Key_Return));
    QVERIFY(RfidQuickFilter::isTerminator(Qt::Key_Enter));
    QVERIFY(!RfidQuickFilter::isTerminator(Qt::Key_A));
}

void TestRfidQuickFilter::recognizedScanEmitsAndConsumesEnter()
{
    QQuickWindow win;
    PublicQuickFilter filter(&win);
    QSignalSpy spy(&filter, &RfidQuickFilter::rfidScanned);

    QCOMPARE(feedPrintable(filter, win, Qt::Key_A, QStringLiteral("A")), false);
    QCOMPARE(feedPrintable(filter, win, Qt::Key_B, QStringLiteral("B")), false);
    QCOMPARE(feedPrintable(filter, win, Qt::Key_C, QStringLiteral("C")), false);
    QCOMPARE(feedPrintable(filter, win, Qt::Key_1, QStringLiteral("1")), false);

    QVERIFY(feedTerminator(filter, win));           // consumed
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("ABC1"));
}

void TestRfidQuickFilter::closedGateIgnoresScan()
{
    QQuickWindow win;
    PublicQuickFilter filter(&win);
    filter.m_forceOpen = false;                     // window not active
    QSignalSpy spy(&filter, &RfidQuickFilter::rfidScanned);

    feedPrintable(filter, win, Qt::Key_A, QStringLiteral("A"));
    feedPrintable(filter, win, Qt::Key_B, QStringLiteral("B"));
    feedPrintable(filter, win, Qt::Key_C, QStringLiteral("C"));
    QVERIFY(!feedTerminator(filter, win));          // not consumed
    QCOMPARE(spy.count(), 0);
}

void TestRfidQuickFilter::printableKeyNotConsumed()
{
    QQuickWindow win;
    PublicQuickFilter filter(&win);
    QVERIFY(!feedPrintable(filter, win, Qt::Key_X, QStringLiteral("X")));
}

QTEST_MAIN(TestRfidQuickFilter)
#include "tst_rfidquickfilter.moc"
