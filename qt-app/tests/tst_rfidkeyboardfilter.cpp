#include <QtTest>
#include <QApplication>
#include <QKeyEvent>
#include <QSignalSpy>
#include <QWidget>
#include "rfidkeyboardfilter.h"

// Thin subclass that promotes eventFilter to public so we can call it
// directly from tests without going through QCoreApplication::sendEvent
// (which would require a full event loop and real focus).
class PublicFilter : public RfidKeyboardFilter
{
public:
    using RfidKeyboardFilter::RfidKeyboardFilter;
    // Re-expose as public
    bool callEventFilter(QObject *watched, QEvent *event)
    {
        return eventFilter(watched, event);
    }
};

class TestRfidKeyboardFilter : public QObject
{
    Q_OBJECT
private slots:
    // --- existing isTerminator tests ---
    void returnKeyIsTerminator();
    void enterKeyIsTerminator();
    void letterKeyIsNotTerminator();

    // --- new eventFilter behavioral tests ---
    void recognizedScanEmitsAndConsumesEnter();
    void inactiveLoginWindowIgnoresScan();
    void printableKeyNotConsumed();
    void emptyTextEventSkipped();
    void propagatedKeyCountedOnce();
};

// ---- isTerminator (unchanged) ----

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

// ---- Helpers ----

// QApplication::setActiveWindow is deprecated in Qt 6.11 but remains the only
// reliable way to control QApplication::activeWindow() under the offscreen
// platform used by ctest.  Suppress the warning locally rather than switching
// to activateWindow(), which is asynchronous and not guaranteed to update
// QApplication::activeWindow() without a real windowing system.
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
static void makeActive(QWidget &w) { QApplication::setActiveWindow(&w); }
QT_WARNING_POP

static bool feedPrintable(PublicFilter &filter, QWidget &watched,
                           Qt::Key key, const QString &text)
{
    QKeyEvent ev(QEvent::KeyPress, key, Qt::NoModifier, text);
    return filter.callEventFilter(&watched, &ev);
}

static bool feedTerminator(PublicFilter &filter, QWidget &watched)
{
    QKeyEvent term(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier,
                   QStringLiteral("\r"));
    return filter.callEventFilter(&watched, &term);
}

// ---- Test 1: recognized scan on the active login window ----
void TestRfidKeyboardFilter::recognizedScanEmitsAndConsumesEnter()
{
    QWidget w;
    w.show();
    makeActive(w);

    PublicFilter filter(&w);
    QSignalSpy spy(&filter, &RfidKeyboardFilter::rfidScanned);

    // Feed printable keys to the primary target (the active window) — none
    // should be consumed.
    QCOMPARE(feedPrintable(filter, w, Qt::Key_A, QStringLiteral("A")), false);
    QCOMPARE(feedPrintable(filter, w, Qt::Key_B, QStringLiteral("B")), false);
    QCOMPARE(feedPrintable(filter, w, Qt::Key_C, QStringLiteral("C")), false);
    QCOMPARE(feedPrintable(filter, w, Qt::Key_1, QStringLiteral("1")), false);

    // Feed terminator — must be consumed and signal emitted
    bool consumed = feedTerminator(filter, w);
    QVERIFY(consumed);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("ABC1"));
}

// ---- Test 2: login window is NOT the active window — scan is ignored ----
void TestRfidKeyboardFilter::inactiveLoginWindowIgnoresScan()
{
    QWidget loginWindow;
    loginWindow.show();

    QWidget other;
    other.show();
    makeActive(other); // different window is active

    PublicFilter filter(&loginWindow);
    QSignalSpy spy(&filter, &RfidKeyboardFilter::rfidScanned);

    // Keys are delivered to the active window (`other`); the active-window gate
    // blocks them before the primary-target gate is reached.
    feedPrintable(filter, other, Qt::Key_A, QStringLiteral("A"));
    feedPrintable(filter, other, Qt::Key_B, QStringLiteral("B"));
    feedPrintable(filter, other, Qt::Key_C, QStringLiteral("C"));
    feedPrintable(filter, other, Qt::Key_1, QStringLiteral("1"));

    bool consumed = feedTerminator(filter, other);

    // Gate not open — Enter must NOT be consumed, no signal emitted
    QVERIFY(!consumed);
    QCOMPARE(spy.count(), 0);
}

// ---- Test 3: printable key is never consumed even on the active window ----
void TestRfidKeyboardFilter::printableKeyNotConsumed()
{
    QWidget w;
    w.show();
    makeActive(w);

    PublicFilter filter(&w);

    bool consumed = feedPrintable(filter, w, Qt::Key_X, QStringLiteral("X"));
    QVERIFY(!consumed);
}

// ---- Test 4: empty-text event (e.g. Shift) is skipped — not consumed ----
void TestRfidKeyboardFilter::emptyTextEventSkipped()
{
    QWidget w;
    w.show();
    makeActive(w);

    PublicFilter filter(&w);
    QSignalSpy spy(&filter, &RfidKeyboardFilter::rfidScanned);

    QKeyEvent ev(QEvent::KeyPress, Qt::Key_Shift, Qt::NoModifier, QString());
    bool consumed = filter.callEventFilter(&w, &ev);

    QVERIFY(!consumed);
    QCOMPARE(spy.count(), 0);
}

// ---- Test 5: a key propagated to an ancestor is counted only once ----
void TestRfidKeyboardFilter::propagatedKeyCountedOnce()
{
    QWidget w;
    w.show();
    makeActive(w);

    // The fix resolves a key's "primary target" as focusWidget() else
    // activeWindow(). With nothing focused, target == &w, so a delivery to &w
    // is the primary copy and a delivery to any other widget is a propagated
    // ancestor copy to be ignored. Assert the precondition so the test stays
    // deterministic under the offscreen QPA.
    QVERIFY(QApplication::focusWidget() == nullptr);

    PublicFilter filter(&w);
    QSignalSpy spy(&filter, &RfidKeyboardFilter::rfidScanned);

    QWidget ancestor; // a different recipient — stands in for the propagated copy

    // Simulate Qt invoking the app-global filter twice per physical key: once
    // for the focus target (&w) and once as the key propagates to an ancestor.
    // Code "123" is >= the detector's minCodeLength (3) in its single form, so
    // the FIXED behavior still produces a recognized scan.
    feedPrintable(filter, w,        Qt::Key_1, QStringLiteral("1"));
    feedPrintable(filter, ancestor, Qt::Key_1, QStringLiteral("1"));
    feedPrintable(filter, w,        Qt::Key_2, QStringLiteral("2"));
    feedPrintable(filter, ancestor, Qt::Key_2, QStringLiteral("2"));
    feedPrintable(filter, w,        Qt::Key_3, QStringLiteral("3"));
    feedPrintable(filter, ancestor, Qt::Key_3, QStringLiteral("3"));

    // The terminator is delivered to the primary target; on a recognized scan
    // the filter consumes it, so propagation stops (no ancestor copy).
    QVERIFY(feedTerminator(filter, w));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("123"));
}

QTEST_MAIN(TestRfidKeyboardFilter)
#include "tst_rfidkeyboardfilter.moc"
