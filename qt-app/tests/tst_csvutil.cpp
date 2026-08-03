#include <QtTest>
#include "csvutil.h"

class TestCsvUtil : public QObject
{
    Q_OBJECT
private slots:
    void escapeField_plainFieldIsUnchanged();
    void escapeField_quotesCommaField();
    void escapeField_quotesAndDoublesEmbeddedQuote();
    void escapeField_quotesFieldWithCr();
    void escapeField_quotesFieldWithLf();
    void escapeField_quotesCommaAndQuoteTogether();
    void escapeField_singleQuoteCharBecomesDoubledAndWrapped();
    void joinRow_escapesEachCellJoinsWithCommaAndCrlf();
    void joinRow_emptyListIsJustCrlf();
};

void TestCsvUtil::escapeField_plainFieldIsUnchanged()
{
    QCOMPARE(csvutil::escapeField(QStringLiteral("plain")), QStringLiteral("plain"));
    QCOMPARE(csvutil::escapeField(QString()), QString());
}

void TestCsvUtil::escapeField_quotesCommaField()
{
    QCOMPARE(csvutil::escapeField(QStringLiteral("a,b")), QStringLiteral("\"a,b\""));
}

void TestCsvUtil::escapeField_quotesAndDoublesEmbeddedQuote()
{
    QCOMPARE(csvutil::escapeField(QStringLiteral("say \"hi\"")),
              QStringLiteral("\"say \"\"hi\"\"\""));
}

void TestCsvUtil::escapeField_quotesFieldWithCr()
{
    QCOMPARE(csvutil::escapeField(QStringLiteral("a\rb")), QStringLiteral("\"a\rb\""));
}

void TestCsvUtil::escapeField_quotesFieldWithLf()
{
    QCOMPARE(csvutil::escapeField(QStringLiteral("a\nb")), QStringLiteral("\"a\nb\""));
}

void TestCsvUtil::escapeField_quotesCommaAndQuoteTogether()
{
    // comma AND an embedded quote -> quote the whole field, double the inner quote
    QCOMPARE(csvutil::escapeField(QStringLiteral("a\",b")), QStringLiteral("\"a\"\",b\""));
}

void TestCsvUtil::escapeField_singleQuoteCharBecomesDoubledAndWrapped()
{
    QCOMPARE(csvutil::escapeField(QStringLiteral("\"")), QStringLiteral("\"\"\"\""));
}

void TestCsvUtil::joinRow_escapesEachCellJoinsWithCommaAndCrlf()
{
    const QString row = csvutil::joinRow({ QStringLiteral("a"), QStringLiteral("b,c"),
                                            QStringLiteral("d\"e") });
    QCOMPARE(row, QStringLiteral("a,\"b,c\",\"d\"\"e\"\r\n"));
}

void TestCsvUtil::joinRow_emptyListIsJustCrlf()
{
    QCOMPARE(csvutil::joinRow(QStringList{}), QStringLiteral("\r\n"));
}

QTEST_MAIN(TestCsvUtil)
#include "tst_csvutil.moc"
