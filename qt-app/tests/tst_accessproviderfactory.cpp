#include <QtTest>
#include <QVariantMap>
#include "accesscontrol/accesstypes.h"
#include "accesscontrol/accessproviderfactory.h"
#include "accesscontrol/mockprovider.h"

using namespace AccessControl;

class TestAccessProviderFactory : public QObject
{
    Q_OBJECT
private slots:
    void availableListsRegisteredDescriptors();
    void createBuildsRegisteredProvider();
    void createUnknownIdReturnsNull();
    void createWithNullParentReturnsNull();
    void createParentsProviderToOwner();
    void createEnforcesParentWhenCreatorIgnoresIt();
};

static AccessProviderFactory makeFactoryWithMock()
{
    AccessProviderFactory f;
    f.registerProvider(MockProvider::defaultDescriptor(),
        [](const ProviderDescriptor &d, const QVariantMap &, QObject *parent) -> IAccessProvider * {
            return new MockProvider(d, parent);
        });
    return f;
}

void TestAccessProviderFactory::availableListsRegisteredDescriptors()
{
    const AccessProviderFactory f = makeFactoryWithMock();
    const QList<ProviderDescriptor> list = f.available();
    QCOMPARE(list.size(), 1);
    QCOMPARE(list.first().providerId, QStringLiteral("mock"));
}

void TestAccessProviderFactory::createBuildsRegisteredProvider()
{
    const AccessProviderFactory f = makeFactoryWithMock();
    QObject owner;
    IAccessProvider *p = f.create(MockProvider::defaultDescriptor(), {}, &owner);
    QVERIFY(p != nullptr);
    QCOMPARE(p->descriptor().providerId, QStringLiteral("mock"));
}

void TestAccessProviderFactory::createUnknownIdReturnsNull()
{
    const AccessProviderFactory f = makeFactoryWithMock();
    QObject owner;
    ProviderDescriptor unknown;
    unknown.providerId = QStringLiteral("does-not-exist");
    QVERIFY(f.create(unknown, {}, &owner) == nullptr);
}

void TestAccessProviderFactory::createWithNullParentReturnsNull()
{
    // Ownership is mandatory: a null parent must yield nullptr, never an
    // unowned provider, even for a registered id.
    const AccessProviderFactory f = makeFactoryWithMock();
    QVERIFY(f.create(MockProvider::defaultDescriptor(), {}, nullptr) == nullptr);
}

void TestAccessProviderFactory::createParentsProviderToOwner()
{
    const AccessProviderFactory f = makeFactoryWithMock();
    auto *owner = new QObject;
    IAccessProvider *p = f.create(MockProvider::defaultDescriptor(), {}, owner);
    QVERIFY(p != nullptr);
    QCOMPARE(p->parent(), owner);
    delete owner;   // deleting the parent must delete the provider — no leak
}

void TestAccessProviderFactory::createEnforcesParentWhenCreatorIgnoresIt()
{
    // A misbehaving creator that ignores the parent argument must NOT be able to
    // leave the provider unowned — the factory reparents the result.
    AccessProviderFactory f;
    f.registerProvider(MockProvider::defaultDescriptor(),
        [](const ProviderDescriptor &d, const QVariantMap &, QObject *) -> IAccessProvider * {
            return new MockProvider(d, nullptr);   // forgets to pass the parent
        });
    auto *owner = new QObject;
    IAccessProvider *p = f.create(MockProvider::defaultDescriptor(), {}, owner);
    QVERIFY(p != nullptr);
    QCOMPARE(p->parent(), owner);   // factory enforced ownership anyway
    delete owner;                    // no leak
}

QTEST_MAIN(TestAccessProviderFactory)
#include "tst_accessproviderfactory.moc"
