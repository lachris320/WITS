#include <QtTest>
#include <QNetworkRequest>
#include <QSslConfiguration>
#include <QThread>
#include <atomic>
#include <memory>
#include <type_traits>
#include <vector>

#include "transport/transportpolicy.h"

// Compile-time shape of the spec's policy object (§2): shared read-only, immutable.
static_assert(std::is_const_v<std::remove_reference_t<decltype(*TransportPolicy::current())>>,
              "policies are shared as std::shared_ptr<const TransportPolicy>");
static_assert(!std::is_copy_assignable_v<TransportPolicy>, "TransportPolicy is immutable");
static_assert(!std::is_copy_constructible_v<TransportPolicy>, "TransportPolicy is shared, not copied");

class TestTransportPolicy : public QObject
{
    Q_OBJECT
private slots:
    void cleanup();
    void passthroughModeReported();
    void prepareIsIdentityInPassthrough();
    void defaultCurrentIsPassthroughAndNeverNull();
    void setCurrentSwapsSnapshotHoldersKeepTheirs();
    void setCurrentNullIsIgnored();
    void concurrentReadersAlwaysSeeACompleteSnapshot();
};

void TestTransportPolicy::cleanup()
{
    TransportPolicy::setCurrent(TransportPolicy::makePassthrough());
}

void TestTransportPolicy::passthroughModeReported()
{
    QVERIFY(TransportPolicy::makePassthrough()->mode() == TransportPolicy::Mode::Passthrough);
}

void TestTransportPolicy::prepareIsIdentityInPassthrough()
{
    const auto policy = TransportPolicy::makePassthrough();

    QNetworkRequest req(QUrl(QStringLiteral("http://localhost/loams_api/student_login.php")));
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/x-www-form-urlencoded"));
    req.setRawHeader("X-Test", "s1a");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(1234);
    QSslConfiguration ssl;
    ssl.setProtocol(QSsl::TlsV1_2OrLater);
    req.setSslConfiguration(ssl);

    const QNetworkRequest out = policy->prepare(req);
    QVERIFY(out == req);                                  // nothing added, removed or changed
    QCOMPARE(out.url(), req.url());                       // http default allowed (no upgrade)
    QCOMPARE(out.rawHeaderList(), req.rawHeaderList());
    QCOMPARE(out.attribute(QNetworkRequest::RedirectPolicyAttribute),
             req.attribute(QNetworkRequest::RedirectPolicyAttribute));
    QVERIFY(out.sslConfiguration() == ssl);               // no TLS change in S1a

    // A request with NO redirect policy keeps none: S1a must not force
    // ManualRedirectPolicy (that enforcement belongs to S1e).
    const QNetworkRequest bare(QUrl(QStringLiteral("http://localhost/loams_api/get_departments.php")));
    QVERIFY(!policy->prepare(bare).attribute(QNetworkRequest::RedirectPolicyAttribute).isValid());
}

void TestTransportPolicy::defaultCurrentIsPassthroughAndNeverNull()
{
    const auto p = TransportPolicy::current();
    QVERIFY(p);
    QVERIFY(p->mode() == TransportPolicy::Mode::Passthrough);
}

void TestTransportPolicy::setCurrentSwapsSnapshotHoldersKeepTheirs()
{
    const auto original = TransportPolicy::current();
    const auto replacement = TransportPolicy::makePassthrough();
    QVERIFY(original.get() != replacement.get());

    TransportPolicy::setCurrent(replacement);
    QCOMPARE(TransportPolicy::current().get(), replacement.get());
    QVERIFY(original);                                    // holder's snapshot stays alive
    QVERIFY(original->mode() == TransportPolicy::Mode::Passthrough);   // and unchanged
}

void TestTransportPolicy::setCurrentNullIsIgnored()
{
    const auto before = TransportPolicy::current();
    QTest::ignoreMessage(QtWarningMsg, "TransportPolicy::setCurrent: null policy ignored");
    TransportPolicy::setCurrent(nullptr);
    QCOMPARE(TransportPolicy::current().get(), before.get());
}

void TestTransportPolicy::concurrentReadersAlwaysSeeACompleteSnapshot()
{
    // QQmlAbstractUrlInterceptor / the QML factory may read the policy from
    // other threads (spec §2): readers racing a writer must always get a
    // complete, non-null snapshot.
    std::atomic<bool> stop{false};
    std::atomic<int> badSeen{0};
    std::atomic<long long> reads{0};
    std::vector<std::unique_ptr<QThread>> readers;
    for (int i = 0; i < 4; ++i) {
        readers.emplace_back(QThread::create([&stop, &badSeen, &reads] {
            while (!stop.load()) {
                const auto p = TransportPolicy::current();
                if (!p || p->mode() != TransportPolicy::Mode::Passthrough)
                    ++badSeen;
                ++reads;
            }
        }));
        readers.back()->start();
    }
    QTRY_VERIFY_WITH_TIMEOUT(reads.load() > 100, 5000);
    for (int i = 0; i < 2000; ++i)
        TransportPolicy::setCurrent(TransportPolicy::makePassthrough());
    stop = true;
    for (auto &t : readers)
        QVERIFY(t->wait(5000));
    QCOMPARE(badSeen.load(), 0);
}

QTEST_MAIN(TestTransportPolicy)
#include "tst_transportpolicy.moc"
