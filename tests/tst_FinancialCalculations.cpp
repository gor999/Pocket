#include "core/Currency.h"
#include "core/Money.h"

#include <QRandomGenerator>
#include <QtTest>

using namespace pf;

class tst_FinancialCalculations : public QObject
{
    Q_OBJECT

private slots:
    void currencyRegistry();

    void parse_data();
    void parse();
    void parseAmbiguityFlag();
    void parseRejectsGarbage_data();
    void parseRejectsGarbage();
    void parseRejectsOverflow();

    void addSubtractIsExact();
    void manySmallAmountsDoNotDrift();
    void comparison();

    void toDecimalString_data();
    void toDecimalString();
    void roundTripRandomValues();

    void format_data();
    void format();
};

void tst_FinancialCalculations::currencyRegistry()
{
    QVERIFY(Currency::isSupported("AMD"));
    QVERIFY(Currency::isSupported(" usd "));
    QVERIFY(!Currency::isSupported("XXX"));
    QCOMPARE(Currency::exponent("AMD"), 2);
    QCOMPARE(Currency::exponent("JPY"), 0);
    QCOMPARE(Currency::symbol("AMD"), QString::fromUtf8("֏"));
    QVERIFY(Currency::supportedCodes().contains("RUB"));
}

void tst_FinancialCalculations::parse_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("currency");
    QTest::addColumn<qint64>("minor");

    QTest::newRow("plain integer") << "12" << "AMD" << qint64(1200);
    QTest::newRow("dot decimal") << "1234.56" << "AMD" << qint64(123456);
    QTest::newRow("one decimal digit") << "12.5" << "USD" << qint64(1250);
    QTest::newRow("comma decimal") << "12,50" << "EUR" << qint64(1250);
    QTest::newRow("english grouping") << "1,234.56" << "USD" << qint64(123456);
    QTest::newRow("european grouping") << "1.234,56" << "EUR" << qint64(123456);
    QTest::newRow("space grouping + comma") << "1 234,56" << "RUB" << qint64(123456);
    QTest::newRow("nbsp grouping") << QString::fromUtf8("12 345,6") << "AMD" << qint64(1234560);
    QTest::newRow("apostrophe grouping") << "1'234.50" << "CHF" << qint64(123450);
    QTest::newRow("multiple grouping") << "1,234,567" << "AMD" << qint64(123456700);
    QTest::newRow("negative") << "-12.5" << "USD" << qint64(-1250);
    QTest::newRow("unicode minus") << QString::fromUtf8("−12.50") << "USD" << qint64(-1250);
    QTest::newRow("leading plus") << "+3.10" << "USD" << qint64(310);
    QTest::newRow("leading dot") << ".5" << "USD" << qint64(50);
    QTest::newRow("trailing dot") << "12." << "USD" << qint64(1200);
    QTest::newRow("zero") << "0" << "AMD" << qint64(0);
    QTest::newRow("surrounding space") << "  7.05 " << "USD" << qint64(705);
    QTest::newRow("zero-decimals currency") << "1234" << "JPY" << qint64(1234);
    QTest::newRow("zero-decimals grouping") << "1,234" << "JPY" << qint64(1234);
    QTest::newRow("lowercase currency") << "5.5" << "usd" << qint64(550);
    QTest::newRow("large value") << "9,999,999,999.99" << "USD" << qint64(999999999999);
}

void tst_FinancialCalculations::parse()
{
    QFETCH(QString, text);
    QFETCH(QString, currency);
    QFETCH(qint64, minor);

    const auto money = Money::fromDecimalString(text, currency);
    QVERIFY2(money.has_value(), qPrintable(text));
    QCOMPARE(money->minorUnits(), minor);
    QCOMPARE(money->currency(), Currency::normalize(currency));
}

void tst_FinancialCalculations::parseAmbiguityFlag()
{
    bool ambiguous = false;
    auto m = Money::fromDecimalString("1,234", "USD", &ambiguous);
    QVERIFY(m.has_value());
    QVERIFY(ambiguous); // could be 1,234 or an over-precise 1.234
    QCOMPARE(m->minorUnits(), qint64(123400));

    ambiguous = true;
    m = Money::fromDecimalString("1,234.50", "USD", &ambiguous);
    QVERIFY(m.has_value());
    QVERIFY(!ambiguous);

    m = Money::fromDecimalString("12.50", "USD", &ambiguous);
    QVERIFY(!ambiguous);
}

void tst_FinancialCalculations::parseRejectsGarbage_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("currency");

    QTest::newRow("empty") << "" << "AMD";
    QTest::newRow("blank") << "   " << "AMD";
    QTest::newRow("letters") << "abc" << "AMD";
    QTest::newRow("digits+letters") << "12a" << "AMD";
    QTest::newRow("currency symbol inside") << "$12" << "USD";
    QTest::newRow("only sign") << "-" << "USD";
    QTest::newRow("only separator") << "." << "USD";
    QTest::newRow("bad grouping") << "1.2.3" << "USD";
    QTest::newRow("bad grouping 2") << "1,23,456" << "USD";
    QTest::newRow("too many decimals") << "12.3456" << "USD";
    QTest::newRow("too many decimals JPY") << "12.5" << "JPY";
    QTest::newRow("two decimal separators") << "1,234.56.78" << "USD";
    QTest::newRow("thousands after decimal") << "1.23,456" << "USD";
    QTest::newRow("unsupported currency") << "12" << "XXX";
    QTest::newRow("empty currency") << "12" << "";
}

void tst_FinancialCalculations::parseRejectsGarbage()
{
    QFETCH(QString, text);
    QFETCH(QString, currency);
    QVERIFY2(!Money::fromDecimalString(text, currency).has_value(), qPrintable(text));
}

void tst_FinancialCalculations::parseRejectsOverflow()
{
    QVERIFY(!Money::fromDecimalString("99999999999999999999", "AMD").has_value());
    QVERIFY(Money::fromDecimalString("92233720368547758.07", "AMD").has_value()); // INT64_MAX minor units
    QVERIFY(!Money::fromDecimalString("92233720368547758.08", "AMD").has_value());
}

void tst_FinancialCalculations::addSubtractIsExact()
{
    const Money a(1050, "USD");
    const Money b(250, "USD");
    QCOMPARE((a + b).minorUnits(), qint64(1300));
    QCOMPARE((a - b).minorUnits(), qint64(800));
    QCOMPARE((b - a).minorUnits(), qint64(-800));
    QCOMPARE((-a).minorUnits(), qint64(-1050));
    QCOMPARE(Money(-5, "USD").abs().minorUnits(), qint64(5));

    Money acc = Money::zero("USD");
    acc += a;
    acc -= b;
    QCOMPARE(acc, Money(800, "USD"));
}

void tst_FinancialCalculations::manySmallAmountsDoNotDrift()
{
    // The classic floating-point trap: ten times 0.10 must be exactly 1.00,
    // and a million times 0.01 exactly 10,000.00.
    Money tenCents = Money::zero("USD");
    for (int i = 0; i < 10; ++i)
        tenCents += *Money::fromDecimalString("0.10", "USD");
    QCOMPARE(tenCents.toDecimalString(), QString("1.00"));

    Money million = Money::zero("USD");
    const Money cent(1, "USD");
    for (int i = 0; i < 1'000'000; ++i)
        million += cent;
    QCOMPARE(million.toDecimalString(), QString("10000.00"));

    // 0.1 + 0.2 == 0.3 exactly.
    const Money sum = *Money::fromDecimalString("0.1", "USD") + *Money::fromDecimalString("0.2", "USD");
    QCOMPARE(sum, *Money::fromDecimalString("0.3", "USD"));
}

void tst_FinancialCalculations::comparison()
{
    QVERIFY(Money(100, "AMD") < Money(101, "AMD"));
    QVERIFY(Money(100, "AMD") <= Money(100, "AMD"));
    QVERIFY(Money(-1, "AMD") < Money(0, "AMD"));
    QVERIFY(Money(5, "AMD") == Money(5, "AMD"));
    QVERIFY(Money(5, "AMD") != Money(5, "USD")); // equality never aborts
    QVERIFY(Money::zero("AMD").isZero());
    QVERIFY(Money(-1, "AMD").isNegative());
    QVERIFY(!Money().isValid());
}

void tst_FinancialCalculations::toDecimalString_data()
{
    QTest::addColumn<qint64>("minor");
    QTest::addColumn<QString>("currency");
    QTest::addColumn<QString>("expected");

    QTest::newRow("zero") << qint64(0) << "USD" << "0.00";
    QTest::newRow("one cent") << qint64(1) << "USD" << "0.01";
    QTest::newRow("padded fraction") << qint64(1205) << "USD" << "12.05";
    QTest::newRow("negative small") << qint64(-5) << "USD" << "-0.05";
    QTest::newRow("negative") << qint64(-123456) << "AMD" << "-1234.56";
    QTest::newRow("no decimals") << qint64(1234) << "JPY" << "1234";
    QTest::newRow("int64 min") << std::numeric_limits<qint64>::min() << "USD" << "-92233720368547758.08";
}

void tst_FinancialCalculations::toDecimalString()
{
    QFETCH(qint64, minor);
    QFETCH(QString, currency);
    QFETCH(QString, expected);
    QCOMPARE(Money(minor, currency).toDecimalString(), expected);
}

void tst_FinancialCalculations::roundTripRandomValues()
{
    QRandomGenerator rng(20260101);
    const QStringList currencies = {"AMD", "USD", "EUR", "RUB", "JPY", "KRW"};
    for (int i = 0; i < 5000; ++i) {
        const QString cur = currencies.at(int(rng.bounded(currencies.size())));
        // Up to ~10^12 minor units, both signs.
        const qint64 minor = qint64(rng.bounded(1'000'000'000'000ull)) * (rng.bounded(2) ? 1 : -1);
        const Money original(minor, cur);
        const auto parsed = Money::fromDecimalString(original.toDecimalString(), cur);
        QVERIFY2(parsed.has_value(), qPrintable(original.toDecimalString()));
        QCOMPARE(*parsed, original);
    }
}

void tst_FinancialCalculations::format_data()
{
    QTest::addColumn<qint64>("minor");
    QTest::addColumn<QString>("currency");
    QTest::addColumn<QString>("expected");

    const QString nbsp = QString::fromUtf8(" ");
    QTest::newRow("AMD suffix") << qint64(123456) << "AMD" << QString("1,234.56" + nbsp + QString::fromUtf8("֏"));
    QTest::newRow("USD prefix") << qint64(123456) << "USD" << QString("$1,234.56");
    QTest::newRow("negative USD") << qint64(-1250) << "USD" << QString("-$12.50");
    QTest::newRow("negative AMD") << qint64(-5) << "AMD" << QString("-0.05" + nbsp + QString::fromUtf8("֏"));
    QTest::newRow("EUR") << qint64(100) << "EUR" << QString::fromUtf8("€") + "1.00";
    QTest::newRow("JPY no decimals") << qint64(1234567) << "JPY" << QString::fromUtf8("¥") + "1,234,567";
    QTest::newRow("zero") << qint64(0) << "USD" << QString("$0.00");
    QTest::newRow("word-like symbol gap") << qint64(1000) << "CAD" << QString("CA$10.00");
}

void tst_FinancialCalculations::format()
{
    QFETCH(qint64, minor);
    QFETCH(QString, currency);
    QFETCH(QString, expected);
    QCOMPARE(Money(minor, currency).format(QLocale("en_US")), expected);

    // Without the symbol only the number remains.
    QVERIFY(!Money(minor, currency).format(QLocale("en_US"), false).contains(Currency::symbol(currency)));
}

QTEST_GUILESS_MAIN(tst_FinancialCalculations)
#include "tst_FinancialCalculations.moc"
