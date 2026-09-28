#include <QtTest>
#include <QSignalSpy>
#include "accesscontrol/accesstypes.h"
#include "accesscontrol/eventbus.h"

using namespace AccessControl;

class TestEventBus : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() { registerMetaTypes(); }
    void publishReachesSubscriber();
    void publishCarriesEventPayload();
    void nestedPublishFromSubscriberIsDeliveredDepthFirst();
};

void TestEventBus::publishReachesSubscriber()
{
    EventBus bus;
    QSignalSpy spy(&bus, &EventBus::eventPublished);
    AccessEvent e;
    e.type = AccessEvent::Type::EntryObserved;
    bus.publish(e);
    QCOMPARE(spy.count(), 1);
}

void TestEventBus::publishCarriesEventPayload()
{
    EventBus bus;
    QSignalSpy spy(&bus, &EventBus::eventPublished);
    AccessEvent e;
    e.type = AccessEvent::Type::AccessGranted;
    e.gateId = QStringLiteral("gate-9");
    e.correlationId = QStringLiteral("corr-1");
    bus.publish(e);
    QCOMPARE(spy.count(), 1);
    const auto received = qvariant_cast<AccessEvent>(spy.at(0).at(0));
    QCOMPARE(received.type, AccessEvent::Type::AccessGranted);
    QCOMPARE(received.gateId, QStringLiteral("gate-9"));
    QCOMPARE(received.correlationId, QStringLiteral("corr-1"));
}

void TestEventBus::nestedPublishFromSubscriberIsDeliveredDepthFirst()
{
    EventBus bus;
    QSignalSpy spy(&bus, &EventBus::eventPublished);   // connected first
    QList<AccessEvent::Type> order;
    bool nested = false;
    // A subscriber that republishes a nested event once, on an AccessGranted.
    connect(&bus, &EventBus::eventPublished, this, [&](const AccessEvent &e) {
        order.append(e.type);
        if (e.type == AccessEvent::Type::AccessGranted && !nested) {
            nested = true;
            AccessEvent inner;
            inner.type = AccessEvent::Type::EntryObserved;
            bus.publish(inner);   // re-entrant publish
        }
    });

    AccessEvent outer;
    outer.type = AccessEvent::Type::AccessGranted;
    bus.publish(outer);

    QCOMPARE(spy.count(), 2);   // both the outer and the nested event were delivered
    // Depth-first: the nested EntryObserved completes before the outer resumes.
    QCOMPARE(order.size(), 2);
    QCOMPARE(order.at(0), AccessEvent::Type::AccessGranted);
    QCOMPARE(order.at(1), AccessEvent::Type::EntryObserved);
}

QTEST_MAIN(TestEventBus)
#include "tst_eventbus.moc"
