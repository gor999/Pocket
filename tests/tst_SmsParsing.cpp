// Parser tests. EVERY SAMPLE HERE IS SYNTHETIC: the layouts were invented to exercise the parsers
// (see tests/sms_samples/README.md). They are NOT copies of real IDBank / Ameriabank / ACBA
// messages, so a passing test says nothing about real bank wording. Add real (anonymised)
// samples to the `realSamples` table when you have them.

#include "sms/ParserRegistry.h"
#include "sms/SmsSanitizer.h"
#include "sms/SmsText.h"

#include <QtTest>

using namespace pf;
using namespace pf::sms;

Q_DECLARE_METATYPE(pf::TransactionType)

namespace {

IncomingSms msg(const QString &body, const QString &sender = QString(), const QString &hint = QString())
{
    IncomingSms m;
    m.body = body;
    m.sender = sender;
    m.bankHint = hint;
    m.receivedAt = QDateTime(QDate(2026, 11, 16), QTime(9, 0)); // fixed "now" for reproducible dates
    return m;
}

} // namespace

class tst_SmsParsing : public QObject
{
    Q_OBJECT

private slots:
    void parsesSyntheticSamples_data();
    void parsesSyntheticSamples();

    void realSamples_data();
    void realSamples();

    void failures_data();
    void failures();

    void implausibleAmountGoesToReview();
    void signDoesNotLeakIntoOperationWord();
    void genericFallbackIsLessConfident();
    void bankHintWinsOverText();
    void unknownBank();
    void everyBuiltInBankListsFilterPhrases();

    void amounts_data();
    void amounts();
    void dates_data();
    void dates();
    void languageDetection_data();
    void languageDetection();

    void sanitizerMasksCardNumbers();
    void sanitizerDetectsSecrets_data();
    void sanitizerDetectsSecrets();
    void sanitizerCleansControlCharacters();
};

// ---------------------------------------------------------------------------
// Synthetic samples per bank and language
// ---------------------------------------------------------------------------

void tst_SmsParsing::parsesSyntheticSamples_data()
{
    QTest::addColumn<QString>("body");
    QTest::addColumn<QString>("bank");
    QTest::addColumn<int>("type"); // TransactionType
    QTest::addColumn<qint64>("minor");
    QTest::addColumn<QString>("currency");
    QTest::addColumn<QString>("merchant");
    QTest::addColumn<QString>("card");
    QTest::addColumn<qint64>("balanceMinor"); // -1 = none
    QTest::addColumn<QString>("language");
    QTest::addColumn<int>("minConfidence");

    const int E = int(TransactionType::Expense);
    const int I = int(TransactionType::Income);

    // --- IDBank (single line, comma separated) -------------------------------------------
    QTest::newRow("idbank-en-purchase")
        << "IDBank: Purchase 12,500.00 AMD, SAS SUPERMARKET YEREVAN, Card *1234, 15.11.2026 14:32, Balance: 245,000.00 AMD"
        << "idbank" << E << qint64(1250000) << "AMD" << "SAS SUPERMARKET YEREVAN" << "1234" << qint64(24500000) << "en" << 90;
    QTest::newRow("idbank-en-credit-no-merchant")
        << "IDBank: Credit 500,000.00 AMD, Card *1234, 01.11.2026 10:00, Balance: 745,000.00 AMD"
        << "idbank" << I << qint64(50000000) << "AMD" << "" << "1234" << qint64(74500000) << "en" << 75;
    QTest::newRow("idbank-hy-purchase")
        << QString::fromUtf8("IDBank: Գնում 8,900.00 AMD, ԿԱՖԵ ԱՐԱՄ, Քարտ *1234, 15.11.2026 12:05, Մնացորդ: 232,500.00 AMD")
        << "idbank" << E << qint64(890000) << "AMD" << QString::fromUtf8("ԿԱՖԵ ԱՐԱՄ") << "1234" << qint64(23250000) << "hy" << 90;
    QTest::newRow("idbank-ru-space-thousands-decimal-comma")
        << QString::fromUtf8("IDBank: Покупка 3 450,00 AMD, PHARMACY NATALI, Карта *1234, 14.11.2026 18:20, Остаток: 100 000,00 AMD")
        << "idbank" << E << qint64(345000) << "AMD" << "PHARMACY NATALI" << "1234" << qint64(10000000) << "ru" << 90;
    QTest::newRow("idbank-en-refund-is-income")
        << "IDBank: Refund 4,500.00 AMD, ZARA, Card *1234, 15.11.2026 14:32"
        << "idbank" << I << qint64(450000) << "AMD" << "ZARA" << "1234" << qint64(-1) << "en" << 90;
    QTest::newRow("idbank-en-foreign-currency-with-fee-ignored")
        << "IDBank: Purchase 100.00 USD, SHOP, Card *1234, 15.11.2026 14:32, Fee: 1.00 USD"
        << "idbank" << E << qint64(10000) << "USD" << "SHOP" << "1234" << qint64(-1) << "en" << 90;

    // --- Ameriabank (multi-line) ----------------------------------------------------------
    QTest::newRow("ameriabank-en-purchase")
        << "Ameriabank\nPurchase: 12,500.00 AMD\nSAS SUPERMARKET\nCard *4321 15.11.2026 14:32\nBalance: 245,000.00 AMD"
        << "ameriabank" << E << qint64(1250000) << "AMD" << "SAS SUPERMARKET" << "4321" << qint64(24500000) << "en" << 90;
    QTest::newRow("ameriabank-en-credit")
        << "Ameriabank\nCredit: 800,000.00 AMD\nCard *4321 10.11.2026 09:10\nBalance: 1,045,000.00 AMD"
        << "ameriabank" << I << qint64(80000000) << "AMD" << "" << "4321" << qint64(104500000) << "en" << 75;

    // --- ACBA (single line, card first) ---------------------------------------------------
    QTest::newRow("acba-en-purchase")
        << "ACBA: Card *5555 Purchase 12,500.00 AMD 15.11.2026 14:32 SAS SUPERMARKET Balance: 245,000.00 AMD"
        << "acba" << E << qint64(1250000) << "AMD" << "SAS SUPERMARKET" << "5555" << qint64(24500000) << "en" << 90;
    QTest::newRow("acba-en-cash-withdrawal")
        << "ACBA: Card *5555 Cash withdrawal 50,000.00 AMD 15.11.2026 14:32 ATM 1234 YEREVAN"
        << "acba" << E << qint64(5000000) << "AMD" << "ATM 1234 YEREVAN" << "5555" << qint64(-1) << "en" << 90;
    QTest::newRow("acba-en-salary")
        << "ACBA: Card *5555 Salary 300,000.00 AMD 05.11.2026 09:00"
        << "acba" << I << qint64(30000000) << "AMD" << "" << "5555" << qint64(-1) << "en" << 75;
    QTest::newRow("acba-ru-purchase")
        << QString::fromUtf8("ACBA: Карта *5555 Покупка 2 300,00 AMD 15.11.2026 14:32 GLOVO Остаток: 50 000,00 AMD")
        << "acba" << E << qint64(230000) << "AMD" << "GLOVO" << "5555" << qint64(5000000) << "ru" << 90;
    QTest::newRow("acba-hy-income")
        << QString::fromUtf8("ACBA: Քարտ *5555 Մուտք 100,000.00 AMD 15.11.2026 10:00")
        << "acba" << I << qint64(10000000) << "AMD" << "" << "5555" << qint64(-1) << "hy" << 75;
}

void tst_SmsParsing::parsesSyntheticSamples()
{
    QFETCH(QString, body);
    QFETCH(QString, bank);
    QFETCH(int, type);
    QFETCH(qint64, minor);
    QFETCH(QString, currency);
    QFETCH(QString, merchant);
    QFETCH(QString, card);
    QFETCH(qint64, balanceMinor);
    QFETCH(QString, language);
    QFETCH(int, minConfidence);

    const ParseOutcome o = ParserRegistry::withBuiltIns().parse(msg(body));
    QVERIFY2(o.ok(), qPrintable(o.errorCode + ": " + o.errorMessage));
    const ParsedBankSms &p = *o.parsed;
    QCOMPARE(p.bankId, bank);
    QCOMPARE(int(p.type), type);
    QCOMPARE(p.amount.minorUnits(), minor);
    QCOMPARE(p.amount.currency(), currency);
    QCOMPARE(p.merchant, merchant);
    QCOMPARE(p.cardLastFour, card);
    QCOMPARE(languageKey(p.language), language);
    if (balanceMinor < 0) {
        QVERIFY(!p.balance.has_value());
    } else {
        QVERIFY(p.balance.has_value());
        QCOMPARE(p.balance->minorUnits(), balanceMinor);
    }
    QVERIFY2(p.confidence >= minConfidence, qPrintable(QString::number(p.confidence)));
    QVERIFY(p.occurredAt.isValid());
    QVERIFY(p.occurredAt <= QDateTime(QDate(2026, 11, 17), QTime(0, 0)));
    QVERIFY(p.amount.isPositive());
}

// Real, anonymised bank SMS go here once available (one row per message). Empty on purpose:
// the sandbox had no access to real messages and none were invented as "real".
void tst_SmsParsing::realSamples_data()
{
    QTest::addColumn<QString>("body");
    QTest::addColumn<QString>("bank");
    QTest::addColumn<qint64>("minor");
    QTest::addColumn<int>("type");
    // QTest::newRow("idbank-2026-xx") << "<paste anonymised SMS>" << "idbank" << qint64(...) << int(TransactionType::Expense);
    QSKIP("No real bank SMS samples have been added yet - see tests/sms_samples/README.md");
}

void tst_SmsParsing::realSamples()
{
    QFETCH(QString, body);
    QFETCH(QString, bank);
    QFETCH(qint64, minor);
    QFETCH(int, type);
    const ParseOutcome o = ParserRegistry::withBuiltIns().parse(msg(body));
    QVERIFY2(o.ok(), qPrintable(o.errorCode));
    QCOMPARE(o.parsed->bankId, bank);
    QCOMPARE(o.parsed->amount.minorUnits(), minor);
    QCOMPARE(int(o.parsed->type), type);
}

// ---------------------------------------------------------------------------
// Failures
// ---------------------------------------------------------------------------

void tst_SmsParsing::failures_data()
{
    QTest::addColumn<QString>("body");
    QTest::addColumn<QString>("errorCode");

    QTest::newRow("promotion") << "IDBank: New offer! 0% installment for 12 months" << "not_a_transaction";
    QTest::newRow("card-blocked-notice") << "IDBank: Card *1234 has been blocked" << "not_a_transaction";
    QTest::newRow("balance-only") << "Ameriabank: Balance 100,000.00 AMD" << "not_a_transaction";
    QTest::newRow("due-date-reminder") << "IDBank: Your loan payment is due on 20.11.2026" << "not_a_transaction";
    QTest::newRow("zero-amount") << "IDBank: Purchase 0.00 AMD, SHOP, Card *1234, 15.11.2026 14:32" << "invalid_amount";
    // A transaction word plus numbers, but nothing readable as an amount: must be visible as a failure.
    QTest::newRow("malformed-amount") << "IDBank: Purchase 12,5.00.1 AMD, SHOP, Card *1234, 15.11.2026 14:32" << "no_amount";
    QTest::newRow("unknown-currency-word") << "IDBank: Purchase 100.00 XYZ, SHOP, Card *1234, 15.11.2026 14:32" << "no_amount";
    QTest::newRow("unknown-bank") << "Hello, this is mom. Please send 5000 AMD" << "unknown_bank";
}

void tst_SmsParsing::failures()
{
    QFETCH(QString, body);
    QFETCH(QString, errorCode);
    const ParseOutcome o = ParserRegistry::withBuiltIns().parse(msg(body));
    QVERIFY2(!o.ok(), "must not produce a transaction");
    QCOMPARE(o.errorCode, errorCode);
}

void tst_SmsParsing::implausibleAmountGoesToReview()
{
    const auto o = ParserRegistry::withBuiltIns().parse(
        msg("IDBank: Purchase 99,999,999,999.00 AMD, SHOP, Card *1234, 15.11.2026 14:32"));
    QVERIFY(o.ok());
    QVERIFY(o.parsed->warnings.contains("implausible_amount"));
    QVERIFY2(o.parsed->confidence < 60, "must fall below the review threshold");
}

void tst_SmsParsing::signDoesNotLeakIntoOperationWord()
{
    const auto o = ParserRegistry::withBuiltIns().parse(msg("IDBank: Purchase -500.00 AMD, SHOP, Card *1234, 15.11.2026 14:32"));
    QVERIFY(o.ok());
    QCOMPARE(o.parsed->kindText, QString("Purchase"));
    QCOMPARE(o.parsed->amount.minorUnits(), qint64(50000)); // the sign never makes the amount negative
    QCOMPARE(int(o.parsed->type), int(TransactionType::Expense));
}

void tst_SmsParsing::genericFallbackIsLessConfident()
{
    // Wording no template knows: the bank-agnostic extractor still gets the facts, but with a lower confidence.
    const auto reg = ParserRegistry::withBuiltIns();
    const auto o = reg.parse(msg("ACBA: your card *5555 debit 4,200 AMD to ZARA 14.11.2026"));
    QVERIFY(o.ok());
    QVERIFY(o.parsed->parserId.endsWith("/generic"));
    QCOMPARE(o.parsed->amount.minorUnits(), qint64(420000));
    QCOMPARE(o.parsed->merchant, QString("ZARA"));
    QCOMPARE(o.parsed->cardLastFour, QString("5555"));
    QVERIFY(o.parsed->confidence < 70);
    QVERIFY(o.parsed->confidence >= 50);

    // A bare "payment 5,000 AMD" has no merchant, card or time: too little to trust, so it goes to review.
    const auto amb = reg.parse(msg("Ameriabank: payment 5,000 AMD"));
    QVERIFY(amb.ok());
    QVERIFY(amb.parsed->confidence < 60);
}

void tst_SmsParsing::bankHintWinsOverText()
{
    const auto reg = ParserRegistry::withBuiltIns();
    // The Shortcut action says "ameriabank"; the text carries no brand at all.
    const auto id = reg.identify(msg("Purchase: 1,000.00 AMD\nSHOP\nCard *4321 15.11.2026 14:32", QString(), "ameriabank"));
    QVERIFY(id.parser);
    QCOMPARE(id.parser->id(), QString("ameriabank"));
    QVERIFY(id.fromHint);
    // "auto" or an unknown hint falls back to detection from the text.
    const auto autoId = reg.identify(msg("ACBA: Card *5555 Salary 1.00 AMD", QString(), "auto"));
    QVERIFY(autoId.parser);
    QCOMPARE(autoId.parser->id(), QString("acba"));
}

void tst_SmsParsing::unknownBank()
{
    const auto reg = ParserRegistry::withBuiltIns();
    const auto id = reg.identify(msg("UniBankX: purchase 1,000 AMD at SOME SHOP"));
    QVERIFY(!id.parser);
}

void tst_SmsParsing::everyBuiltInBankListsFilterPhrases()
{
    const ParserRegistry registry = ParserRegistry::withBuiltIns(); // parsers() points into the registry
    const auto parsers = registry.parsers();
    QVERIFY(parsers.size() >= 3);
    QStringList ids;
    for (const BankSmsParser *p : parsers) {
        ids << p->id();
        QVERIFY2(!p->filterPhrases().isEmpty(), qPrintable(p->id())); // needed for the Shortcuts automation
        QVERIFY(!p->bankName().isEmpty());
    }
    QVERIFY(ids.contains("idbank") && ids.contains("ameriabank") && ids.contains("acba"));
    QCOMPARE(ids.removeDuplicates(), 0);
}

// ---------------------------------------------------------------------------
// Low-level text helpers
// ---------------------------------------------------------------------------

void tst_SmsParsing::amounts_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("currency");
    QTest::addColumn<qint64>("minor"); // -1: unparseable
    QTest::addColumn<bool>("ambiguous");

    QTest::newRow("comma-thousands-dot-decimal") << "12,500.00" << "AMD" << qint64(1250000) << false;
    QTest::newRow("space-thousands-comma-decimal") << "3 450,00" << "AMD" << qint64(345000) << false;
    QTest::newRow("nbsp-thousands") << QString::fromUtf8("1 234,50") << "USD" << qint64(123450) << false;
    QTest::newRow("dot-thousands-comma-decimal") << "1.234,56" << "EUR" << qint64(123456) << false;
    QTest::newRow("plain-integer") << "500" << "AMD" << qint64(50000) << false;
    QTest::newRow("one-decimal") << "15.9" << "USD" << qint64(1590) << false;
    QTest::newRow("lone-thousands-group-amd") << "5,000" << "AMD" << qint64(500000) << false;
    // Three digits after a separator cannot be the decimals of a 2-decimal currency, so this is 1,234.00.
    QTest::newRow("lone-thousands-group-usd") << "1,234" << "USD" << qint64(123400) << false;
    QTest::newRow("garbage") << "12,5.00.1" << "AMD" << qint64(-1) << false;
}

void tst_SmsParsing::amounts()
{
    QFETCH(QString, text);
    QFETCH(QString, currency);
    QFETCH(qint64, minor);
    QFETCH(bool, ambiguous);
    bool amb = false;
    const auto m = text::parseAmount(text, currency, &amb);
    if (minor == -1) {
        QVERIFY(!m.has_value());
        return;
    }
    QCOMPARE(amb, ambiguous);
    QVERIFY(m.has_value());
    QCOMPARE(m->minorUnits(), minor);
}

void tst_SmsParsing::dates_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QDateTime>("expected");
    QTest::addColumn<bool>("hasTime");

    QTest::newRow("day-first") << "on 15.11.2026 14:32" << QDateTime(QDate(2026, 11, 15), QTime(14, 32)) << true;
    QTest::newRow("day-first-slashes") << "03/11/2026 09:05" << QDateTime(QDate(2026, 11, 3), QTime(9, 5)) << true;
    QTest::newRow("iso") << "2026-11-15 14:32:10" << QDateTime(QDate(2026, 11, 15), QTime(14, 32, 10)) << true;
    QTest::newRow("two-digit-year") << "15.11.26 14:32" << QDateTime(QDate(2026, 11, 15), QTime(14, 32)) << true;
    QTest::newRow("no-year") << "15.11 14:32" << QDateTime(QDate(2026, 11, 15), QTime(14, 32)) << true;
    QTest::newRow("date-only") << "14.11.2026" << QDateTime(QDate(2026, 11, 14), QTime(12, 0)) << false;
}

void tst_SmsParsing::dates()
{
    QFETCH(QString, text);
    QFETCH(QDateTime, expected);
    QFETCH(bool, hasTime);
    const auto d = text::findDateTime(text, QDateTime(QDate(2026, 11, 16), QTime(9, 0)));
    QVERIFY(d.has_value());
    QCOMPARE(d->value, expected);
    QCOMPARE(d->hasTime, hasTime);
}

void tst_SmsParsing::languageDetection_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("language");
    QTest::newRow("en") << "Purchase 5,000 AMD at SAS" << "en";
    QTest::newRow("ru") << QString::fromUtf8("Покупка 5 000 AMD, Остаток 100 000") << "ru";
    QTest::newRow("hy") << QString::fromUtf8("Գնում 5,000 AMD, Մնացորդ 100,000") << "hy";
}

void tst_SmsParsing::languageDetection()
{
    QFETCH(QString, text);
    QFETCH(QString, language);
    QCOMPARE(languageKey(text::detectLanguage(text)), language);
}

// ---------------------------------------------------------------------------
// Sanitizer
// ---------------------------------------------------------------------------

void tst_SmsParsing::sanitizerMasksCardNumbers()
{
    const QString masked = SmsSanitizer::sanitizeForStorage("card 4111111111111111 purchase 100.00 AMD CVV: 123");
    QVERIFY2(!masked.contains("4111111111111111"), qPrintable(masked));
    QVERIFY(masked.contains("1111")); // only the last four survive
    QVERIFY2(!masked.contains("CVV: 123"), qPrintable(masked));
    // Spaced 16-digit numbers too.
    QVERIFY(!SmsSanitizer::maskSensitive("4111 1111 1111 1111").contains("4111 1111 1111"));
    // Amounts and dates are untouched.
    QCOMPARE(SmsSanitizer::maskSensitive("12,500.00 AMD 15.11.2026"), QString("12,500.00 AMD 15.11.2026"));
}

void tst_SmsParsing::sanitizerDetectsSecrets_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<bool>("secret");
    QTest::newRow("otp-en") << "IDBank: Your OTP code is 483920. Do not share it." << true;
    QTest::newRow("otp-ru") << QString::fromUtf8("Ваш одноразовый пароль 483920, никому не сообщайте") << true;
    QTest::newRow("3ds") << "3-D Secure code 123456 for purchase of 5,000 AMD" << true;
    QTest::newRow("pin") << "Your new PIN is 4821" << true;
    QTest::newRow("normal-purchase") << "IDBank: Purchase 12,500.00 AMD, SAS, Card *1234" << false;
}

void tst_SmsParsing::sanitizerDetectsSecrets()
{
    QFETCH(QString, text);
    QFETCH(bool, secret);
    QCOMPARE(SmsSanitizer::isSecretBearing(text), secret);
}

void tst_SmsParsing::sanitizerCleansControlCharacters()
{
    // Control and bidi characters are built from code points so the source file stays free of them.
    const QString dirty = QStringLiteral("IDBank:") + QChar(0) + QStringLiteral(" Purchase") + QChar(0x202E)
        + QStringLiteral(" 5.00 AMD\r\n\r\nSHOP") + QChar(7) + QStringLiteral("\t\t  x");
    const QString clean = SmsSanitizer::clean(dirty);
    for (const QChar c : clean) {
        QVERIFY2(c == QLatin1Char('\n') || c == QLatin1Char(' ') || c.isPrint(), qPrintable(QString::number(c.unicode(), 16)));
    }
    QVERIFY(!clean.contains(QChar(0x202E))); // bidi override must not survive
    QVERIFY(!clean.contains('\r'));
    QCOMPARE(SmsSanitizer::clean(clean), clean); // idempotent
    QCOMPARE(SmsSanitizer::cleanSender("ID\nBank\u0007"), QString("IDBank"));
}

QTEST_APPLESS_MAIN(tst_SmsParsing)
#include "tst_SmsParsing.moc"
