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

    QWidget watched;

    // Feed printable keys — none should be consumed
    QCOMPARE(feedPrintable(filter, watched, Qt::Key_A, QStringLiteral("A")), false);
    QCOMPARE(feedPrintable(filter, watched, Qt::Key_B, QStringLiteral("B")), false);
    QCOMPARE(feedPrintable(filter, watched, Qt::Key_C, QStringLiteral("C")), false);
    QCOMPARE(feedPrintable(filter, watched, Qt::Key_1, QStringLiteral("1")), false);

    // Feed terminator — must be consumed and signal emitted
    bool consumed = feedTerminator(filter, watched);
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

    QWidget watched;

    feedPrintable(filter, watched, Qt::Key_A, QStringLiteral("A"));
    feedPrintable(filter, watched, Qt::Key_B, QStringLiteral("B"));
    feedPrintable(filter, watched, Qt::Key_C, QStringLiteral("C"));
    feedPrintable(filter, watched, Qt::Key_1, QStringLiteral("1"));

    bool consumed = feedTerminator(filter, watched);

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
    QWidget watched;

    bool consumed = feedPrintable(filter, watched, Qt::Key_X, QStringLiteral("X"));
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

    QWidget watched;
    QKeyEvent ev(QEvent::KeyPress, Qt::Key_Shift, Qt::NoModifier, QString());
    bool consumed = filter.callEventFilter(&watched, &ev);

    QVERIFY(!consumed);
    QCOMPARE(spy.count(), 0);
}

QTEST_MAIN(TestRfidKeyboardFilter)
#include "tst_rfidkeyboardfilter.moc"
