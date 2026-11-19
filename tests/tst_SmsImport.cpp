// End-to-end tests of the import pipeline on a real (temporary) SQLite database, wired exactly like the
// app (ApplicationManager): envelope -> receiver -> parse -> account -> categorize -> transaction ->
// review / duplicates / errors. EVERY SMS TEXT IS SYNTHETIC (see tests/sms_samples/README.md).

#include "core/ApplicationManager.h"
#include "sms/DirectoryInboxReceiver.h"
#include "sms/SmsSanitizer.h"
#include "sms/UrlImportHandler.h"

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

using namespace pf;

namespace {

const QString kAmeriaPurchase = QStringLiteral(
    "Ameriabank\nPurchase: 12,500.00 AMD\nSAS SUPERMARKET\nCard *4321 15.11.2026 14:32\nBalance: 245,000.00 AMD");

QString stamp(const QDateTime &t)
{
    return t.toString(QStringLiteral("dd.MM.yyyy HH:mm"));
}

} // namespace

class tst_SmsImport : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    std::unique_ptr<ApplicationManager> m_app;
    qint64 m_ameria = 0; // Ameriabank *4321, AMD
    qint64 m_acba = 0; // ACBA Bank *5555, AMD
    QDateTime m_now;

    SmsImportService *svc() const { return m_app->smsImportService(); }
    QString inboxDir() const { return m_dir.path() + "/sms_inbox"; }

    ImportResult send(const QString &body, const QString &sender = QString(), const QString &hint = QString(),
                      const QString &channel = sms::channel::ShortcutIntent, const QString &externalId = QString(),
                      const QDateTime &receivedAt = QDateTime())
    {
        sms::IncomingSms m;
        m.body = body;
        m.sender = sender;
        m.bankHint = hint;
        m.channel = channel;
        m.externalId = externalId;
        m.receivedAt = receivedAt.isValid() ? receivedAt : m_now;
        return svc()->process(m);
    }

    Transaction transaction(qint64 id) const
    {
        const auto t = m_app->transactionRepository().byId(id);
        return t ? t.value() : Transaction();
    }
    int transactionCount() const
    {
        QSqlQuery q(m_app->database().connection());
        q.exec("SELECT COUNT(*) FROM transactions");
        return q.next() ? q.value(0).toInt() : -1;
    }
    QString storedBody(qint64 smsImportId) const
    {
        QSqlQuery q(m_app->database().connection());
        q.prepare("SELECT body FROM sms_imports WHERE id = ?");
        q.addBindValue(qlonglong(smsImportId));
        return q.exec() && q.next() ? q.value(0).toString() : QStringLiteral("<no row>");
    }
    qint64 balanceOf(qint64 accountId) const
    {
        const auto all = m_app->accountRepository().summaries(true);
        for (const AccountSummary &s : all.value()) {
            if (s.account.id == accountId)
                return s.balance.minorUnits();
        }
        return -1;
    }
    QString categoryKey(const Transaction &t) const
    {
        if (!t.categoryId)
            return QString();
        const auto c = m_app->categoryRepository().byId(*t.categoryId);
        return c ? c.value().key : QString();
    }
    qint64 categoryId(const char *key) const { return m_app->categoryRepository().byKey(QString::fromLatin1(key)).value().id; }

    qint64 makeAccount(const QString &bank, const QString &name, const QString &last4, qint64 initialMinor = 0,
                       const QString &currency = QStringLiteral("AMD"))
    {
        Account a;
        a.bankName = bank;
        a.name = name;
        a.lastFour = last4;
        a.initialBalance = Money(initialMinor, currency);
        const auto created = m_app->accountService()->create(a);
        return created ? created.value().id : 0;
    }

    static void writeEnvelope(const QString &dir, const QString &name, const QByteArray &json)
    {
        QDir().mkpath(dir);
        QFile tmp(dir + "/" + name + ".tmp");
        QVERIFY(tmp.open(QIODevice::WriteOnly));
        tmp.write(json);
        tmp.close();
        QVERIFY(QFile::rename(tmp.fileName(), dir + "/" + name + ".json"));
    }

private slots:
    void init();
    void cleanup();

    // happy paths
    void importsPurchaseAndCategorizesIt();
    void importsIncome();
    void updatesBalancesAndMonthlyStats();
    void armenianAndRussianMessages();

    // review
    void unknownMerchantNeedsReview();
    void resolveReviewAppliesCategoryAndRemembersMerchant();
    void rememberedMerchantAutoCategorizesNextTime();
    void discardReviewRemovesTransaction();
    void ambiguousMerchantIsSuggestedNotApplied();
    void lowConfidenceParseNeedsReview();
    void resolveRejectsWrongCategoryKind();

    // duplicates
    void sameEnvelopeIdIsIdempotent();
    void sameTextTwiceIsDuplicate();
    void sameTransactionOtherWordingIsDuplicate();
    void manualTransactionIsNotDoubleCounted();
    void nearDuplicateNeedsReview();

    // rejected / ignored / failed
    void otpIsDroppedAndNeverStored();
    void promotionIsIgnored();
    void unknownBankWithAmountIsUnrecognized();
    void unknownBankWithoutAmountIsIgnored();
    void malformedAmountIsVisibleAsError();
    void emptyAndOversizedAreRejected();
    void disabledBankIsRejected();
    void cardNumberIsMaskedInStoredBody();

    // accounts
    void createsAccountWhenNoneMatches();
    void doesNotCreateAccountWhenSwitchedOff();
    void severalCandidateAccountsNeedReview();
    void matchesBankNameCaseAndSuffixInsensitively();

    // channels / setup status
    void testMessageVerifiesLinkWithoutTransaction();
    void urlChannelIsUntrustedUntilEnabled();
    void summaryLevels();

    // receivers
    void inboxFolderIsIngested();
    void badEnvelopesAreRecordedAndRemoved();
    void envelopeReaderRules();
    void urlParsing();
    void incomingUrlGoesThroughPipeline();

    // QML facade
    void facadeListsHaveExpectedFields();
    void clearHistoryKeepsReviewItems();
};

void tst_SmsImport::init()
{
    QVERIFY(m_dir.isValid());
    m_now = QDateTime::currentDateTime();
    qunsetenv("PF_SMS_INBOX");
    m_app = std::make_unique<ApplicationManager>(m_dir.path() + "/test.sqlite");
    QVERIFY(m_app->initialize());
    m_ameria = makeAccount("Ameriabank", "Everyday", "4321", 30000000);
    m_acba = makeAccount("ACBA Bank", "Salary", "5555", 0);
    QVERIFY(m_ameria > 0 && m_acba > 0);
}

void tst_SmsImport::cleanup()
{
    m_app.reset();
    QDir(m_dir.path()).removeRecursively();
    QDir().mkpath(m_dir.path());
}

// ---------------------------------------------------------------------------

void tst_SmsImport::importsPurchaseAndCategorizesIt()
{
    const ImportResult r = send(kAmeriaPurchase);
    QCOMPARE(r.status, sms::status::Imported);
    QVERIFY(r.handled);
    QVERIFY(r.transactionId > 0);

    const Transaction t = transaction(r.transactionId);
    QCOMPARE(t.accountId, m_ameria);
    QCOMPARE(int(t.type), int(TransactionType::Expense));
    QCOMPARE(t.amount.minorUnits(), qint64(1250000));
    QCOMPARE(t.merchant, QString("SAS Supermarket")); // normalized for display
    QCOMPARE(int(t.source), int(TransactionSource::BankSms));
    QCOMPARE(categoryKey(t), QString("groceries"));
    QCOMPARE(t.occurredAt, QDateTime(QDate(2026, 11, 15), QTime(14, 32)));

    const QVariantMap summary = svc()->summary();
    QCOMPARE(summary.value("importedCount").toInt(), 1);
    QCOMPARE(summary.value("needsReviewCount").toInt(), 0);
}

void tst_SmsImport::importsIncome()
{
    const ImportResult r = send("ACBA: Card *5555 Salary 300,000.00 AMD " + stamp(m_now.addSecs(-3600)));
    QCOMPARE(r.status, sms::status::Imported);
    const Transaction t = transaction(r.transactionId);
    QCOMPARE(int(t.type), int(TransactionType::Income));
    QCOMPARE(t.accountId, m_acba);
    QCOMPARE(categoryKey(t), QString("salary"));
    QCOMPARE(balanceOf(m_acba), qint64(30000000));
}

void tst_SmsImport::updatesBalancesAndMonthlyStats()
{
    const qint64 before = balanceOf(m_ameria);
    const QString when = stamp(m_now.addSecs(-1800));
    const ImportResult r = send(QString("Ameriabank\nPurchase: 12,500.00 AMD\nCARREFOUR\nCard *4321 %1").arg(when));
    QCOMPARE(r.status, sms::status::Imported);
    QCOMPARE(balanceOf(m_ameria), before - 1250000);

    // The monthly numbers are computed from the transactions table, so the import shows up at once.
    // (The test is only meaningful when "30 minutes ago" is still in the current month.)
    if (m_now.addSecs(-1800).date().month() == m_now.date().month()) {
        const QVariantMap month = m_app->analyticsService()->monthSummary("AMD");
        QCOMPARE(month.value("expenseMinor").toLongLong(), qint64(1250000));
    }
    // Models are refreshed through the same change notification the UI uses.
    QCOMPARE(m_app->transactionModel()->count(), 1);
}

void tst_SmsImport::armenianAndRussianMessages()
{
    const QString hy = QString::fromUtf8("ACBA: Քարտ *5555 Մուտք 100,000.00 AMD ") + stamp(m_now.addSecs(-7200));
    const ImportResult a = send(hy);
    // Armenian income without a merchant: parsed fine, but nothing tells us the category.
    QCOMPARE(a.status, sms::status::NeedsReview);
    QCOMPARE(a.reviewReason, sms::review::Category);
    QCOMPARE(transaction(a.transactionId).amount.minorUnits(), qint64(10000000));

    const ImportResult b = send(QString::fromUtf8("ACBA: Карта *5555 Покупка 2 300,00 AMD ") + stamp(m_now.addSecs(-3600))
                                + QString::fromUtf8(" PHARMACY NATALI Остаток: 50 000,00 AMD"));
    QCOMPARE(b.status, sms::status::Imported);
    const Transaction t = transaction(b.transactionId);
    QCOMPARE(t.amount.minorUnits(), qint64(230000));
    QCOMPARE(categoryKey(t), QString("healthcare"));
    QCOMPARE(balanceOf(m_acba), qint64(10000000 - 230000));
}

// ---------------------------------------------------------------------------
// Review
// ---------------------------------------------------------------------------

void tst_SmsImport::unknownMerchantNeedsReview()
{
    const ImportResult r = send("ACBA: Card *5555 Purchase 6,700.00 AMD " + stamp(m_now.addSecs(-3600)) + " ZZ TRADING 77");
    QCOMPARE(r.status, sms::status::NeedsReview);
    QCOMPARE(r.reviewReason, sms::review::Category);
    QVERIFY(!transaction(r.transactionId).categoryId.has_value());
    const QVariantList review = svc()->reviewItems();
    QCOMPARE(review.size(), 1);
    // The normalizer drops the trailing store number and keeps short all-caps words.
    QCOMPARE(review.first().toMap().value("merchant").toString(), QString("ZZ Trading"));
}

void tst_SmsImport::resolveReviewAppliesCategoryAndRemembersMerchant()
{
    const QString base = "ACBA: Card *5555 Purchase %1 AMD %2 ZZ TRADING 77";
    const ImportResult r1 = send(base.arg("6,700.00", stamp(m_now.addSecs(-7200))));
    const ImportResult r2 = send(base.arg("1,200.00", stamp(m_now.addSecs(-3600))));
    QCOMPARE(r1.status, sms::status::NeedsReview);
    QCOMPARE(r2.status, sms::status::NeedsReview);

    const qint64 shopping = categoryId("shopping");
    const QVariantMap ok = svc()->resolveReview(r1.smsImportId, shopping, /*remember=*/true);
    QVERIFY2(ok.value("ok").toBool(), qPrintable(ok.value("error").toString()));
    QCOMPARE(transaction(r1.transactionId).categoryId.value_or(0), shopping);
    // The remembered merchant also settles the other waiting item.
    QCOMPARE(transaction(r2.transactionId).categoryId.value_or(0), shopping);
    QVERIFY(svc()->reviewItems().isEmpty());
    QCOMPARE(svc()->summary().value("needsReviewCount").toInt(), 0);

    // A second resolve of the same item is refused.
    QVERIFY(!svc()->resolveReview(r1.smsImportId, shopping, false).value("ok").toBool());
}

void tst_SmsImport::rememberedMerchantAutoCategorizesNextTime()
{
    const QString base = "ACBA: Card *5555 Purchase %1 AMD %2 ZZ TRADING 77";
    const ImportResult first = send(base.arg("6,700.00", stamp(m_now.addSecs(-7200))));
    QVERIFY(svc()->resolveReview(first.smsImportId, categoryId("healthcare"), true).value("ok").toBool());

    const ImportResult next = send(base.arg("2,100.00", stamp(m_now.addSecs(-600))));
    QCOMPARE(next.status, sms::status::Imported);
    QCOMPARE(categoryKey(transaction(next.transactionId)), QString("healthcare"));
}

void tst_SmsImport::discardReviewRemovesTransaction()
{
    const ImportResult r = send("ACBA: Card *5555 Purchase 6,700.00 AMD " + stamp(m_now.addSecs(-3600)) + " ZZ TRADING 77");
    QCOMPARE(transactionCount(), 1);
    QVERIFY(svc()->discardReview(r.smsImportId).value("ok").toBool());
    QCOMPARE(transactionCount(), 0);
    QCOMPARE(balanceOf(m_acba), qint64(0));
    QVERIFY(svc()->reviewItems().isEmpty());
}

void tst_SmsImport::ambiguousMerchantIsSuggestedNotApplied()
{
    // Glovo delivers meals and groceries: the app proposes "Restaurants" but the user decides.
    const ImportResult r = send(QString::fromUtf8("ACBA: Карта *5555 Покупка 2 300,00 AMD ") + stamp(m_now.addSecs(-3600))
                                + QString::fromUtf8(" GLOVO Остаток: 50 000,00 AMD"));
    QCOMPARE(r.status, sms::status::NeedsReview);
    QCOMPARE(r.reviewReason, sms::review::Category);
    QVERIFY(!transaction(r.transactionId).categoryId.has_value());
    const QVariantMap item = svc()->reviewItems().first().toMap();
    QCOMPARE(item.value("suggestedName").toString(), m_app->categoryRepository().byKey("restaurants").value().displayName());
    QVERIFY(item.value("suggestionConfidence").toInt() < CategorizationService::kAutoAssignThreshold);
    // Accepting the suggestion is one call.
    QVERIFY(svc()->resolveReview(r.smsImportId, item.value("suggestedId").toLongLong(), false).value("ok").toBool());
    QCOMPARE(categoryKey(transaction(r.transactionId)), QString("restaurants"));
}

void tst_SmsImport::lowConfidenceParseNeedsReview()
{
    // No merchant, card, or time: imported (the money is real) but the user is asked to confirm.
    const ImportResult r = send("Ameriabank: payment 5,000 AMD");
    QCOMPARE(r.status, sms::status::NeedsReview);
    QCOMPARE(r.reviewReason, sms::review::Parse);
}

void tst_SmsImport::resolveRejectsWrongCategoryKind()
{
    const ImportResult r = send("ACBA: Card *5555 Purchase 6,700.00 AMD " + stamp(m_now.addSecs(-3600)) + " ZZ TRADING 77");
    const QVariantMap res = svc()->resolveReview(r.smsImportId, categoryId("salary"), false); // income category on an expense
    QVERIFY(!res.value("ok").toBool());
    QCOMPARE(svc()->reviewItems().size(), 1); // still waiting
}

// ---------------------------------------------------------------------------
// Duplicates
// ---------------------------------------------------------------------------

void tst_SmsImport::sameEnvelopeIdIsIdempotent()
{
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const ImportResult a = send(kAmeriaPurchase, "", "", sms::channel::ShortcutIntent, id);
    const ImportResult b = send(kAmeriaPurchase, "", "", sms::channel::ShortcutIntent, id);
    QCOMPARE(a.status, sms::status::Imported);
    QVERIFY(b.alreadyProcessed);
    QCOMPARE(b.smsImportId, a.smsImportId);
    QCOMPARE(transactionCount(), 1);
    QCOMPARE(svc()->summary().value("duplicateCount").toInt(), 0); // not even logged twice
}

void tst_SmsImport::sameTextTwiceIsDuplicate()
{
    const ImportResult a = send(kAmeriaPurchase);
    const ImportResult b = send(kAmeriaPurchase, "", "", sms::channel::ShortcutIntent, "", m_now.addSecs(20));
    QCOMPARE(a.status, sms::status::Imported);
    QCOMPARE(b.status, sms::status::Duplicate);
    QCOMPARE(b.transactionId, a.transactionId);
    QCOMPARE(transactionCount(), 1);
    QCOMPARE(balanceOf(m_ameria), qint64(30000000 - 1250000));
}

void tst_SmsImport::sameTransactionOtherWordingIsDuplicate()
{
    // Same operation, two different texts (e.g. the second one has an extra balance line): not the same
    // body, but the same account/amount/minute/merchant, so it must not be counted twice.
    const ImportResult a = send("Ameriabank\nPurchase: 12,500.00 AMD\nSAS SUPERMARKET\nCard *4321 15.11.2026 14:32");
    const ImportResult b = send(kAmeriaPurchase, "", "", sms::channel::ShortcutIntent, "", m_now.addSecs(1000));
    QCOMPARE(a.status, sms::status::Imported);
    QCOMPARE(b.status, sms::status::Duplicate);
    QCOMPARE(transactionCount(), 1);
}

void tst_SmsImport::manualTransactionIsNotDoubleCounted()
{
    Transaction manual;
    manual.accountId = m_ameria;
    manual.type = TransactionType::Expense;
    manual.amount = Money(1250000, "AMD");
    manual.occurredAt = QDateTime(QDate(2026, 11, 15), QTime(14, 32));
    manual.merchant = "SAS Supermarket";
    QVERIFY(m_app->transactionService()->add(manual).saved);

    const ImportResult r = send(kAmeriaPurchase);
    QCOMPARE(r.status, sms::status::Duplicate);
    QCOMPARE(transactionCount(), 1);
}

void tst_SmsImport::nearDuplicateNeedsReview()
{
    // A manual entry typed a few minutes off, with a different merchant spelling: probably the same purchase.
    Transaction manual;
    manual.accountId = m_ameria;
    manual.type = TransactionType::Expense;
    manual.amount = Money(1250000, "AMD");
    manual.occurredAt = QDateTime(QDate(2026, 11, 15), QTime(14, 29));
    manual.merchant = "SAS";
    QVERIFY(m_app->transactionService()->add(manual).saved);

    const ImportResult r = send(kAmeriaPurchase);
    QCOMPARE(r.status, sms::status::NeedsReview);
    QCOMPARE(r.reviewReason, sms::review::PossibleDuplicate);
    QCOMPARE(transactionCount(), 2);
    // The user can remove the duplicate with one action.
    QVERIFY(svc()->discardReview(r.smsImportId).value("ok").toBool());
    QCOMPARE(transactionCount(), 1);
}

// ---------------------------------------------------------------------------
// Rejected / ignored / failed
// ---------------------------------------------------------------------------

void tst_SmsImport::otpIsDroppedAndNeverStored()
{
    const ImportResult r = send("IDBank: Your OTP code is 483920. Do not share it.");
    QCOMPARE(r.status, sms::status::Ignored);
    QCOMPARE(storedBody(r.smsImportId), QString()); // text is not kept, not even masked
    QCOMPARE(transactionCount(), 0);
    // A purchase confirmation that also carries a 3-D Secure code is treated the same way.
    const ImportResult s = send("3-D Secure code 123456 for purchase of 5,000 AMD at SHOP");
    QCOMPARE(s.status, sms::status::Ignored);
    QCOMPARE(storedBody(s.smsImportId), QString());
}

void tst_SmsImport::promotionIsIgnored()
{
    const ImportResult r = send("IDBank: New offer! 0% installment for 12 months");
    QCOMPARE(r.status, sms::status::Ignored);
    QCOMPARE(transactionCount(), 0);
    QCOMPARE(svc()->summary().value("errorCount").toInt(), 0); // promotions are not "errors"
}

void tst_SmsImport::unknownBankWithAmountIsUnrecognized()
{
    const ImportResult r = send("UniBankX: purchase 1,000 AMD at SOME SHOP");
    QCOMPARE(r.status, sms::status::Unrecognized);
    QCOMPARE(r.errorCode, sms::errorcode::UnknownBank);
    QCOMPARE(transactionCount(), 0);
    QCOMPARE(svc()->errors(10).size(), 1);
}

void tst_SmsImport::unknownBankWithoutAmountIsIgnored()
{
    const ImportResult r = send("Hello, are we still meeting tomorrow?");
    QCOMPARE(r.status, sms::status::Ignored);
    QCOMPARE(storedBody(r.smsImportId), QString()); // an unrelated personal message is not kept
}

void tst_SmsImport::malformedAmountIsVisibleAsError()
{
    const ImportResult r = send("IDBank: Purchase 12,5.00.1 AMD, SHOP, Card *1234, 15.11.2026 14:32");
    QCOMPARE(r.status, sms::status::Unrecognized);
    QCOMPARE(r.errorCode, sms::errorcode::NoAmount);
    QCOMPARE(transactionCount(), 0);
    QVERIFY(!svc()->errors(10).isEmpty());
    // The user can look at the text to see what the bank sent, and dismiss the entry.
    const qint64 id = svc()->errors(10).first().toMap().value("id").toLongLong();
    QVERIFY(svc()->dismissError(id).value("ok").toBool());
    QVERIFY(svc()->errors(10).isEmpty());
}

void tst_SmsImport::emptyAndOversizedAreRejected()
{
    const ImportResult e = send("   \n ");
    QCOMPARE(e.status, sms::status::Rejected);
    QCOMPARE(e.errorCode, sms::errorcode::EmptyMessage);
    QVERIFY(e.handled); // a bad message must not stay in the inbox forever

    const ImportResult big = send(QString("IDBank: Purchase 1.00 AMD ").repeated(200)); // > 2000 chars
    QCOMPARE(big.status, sms::status::Rejected);
    QCOMPARE(big.errorCode, sms::errorcode::TooLong);
    QCOMPARE(storedBody(big.smsImportId), QString());
    QCOMPARE(transactionCount(), 0);
}

void tst_SmsImport::disabledBankIsRejected()
{
    svc()->setBankEnabled("acba", false);
    const ImportResult r = send("ACBA: Card *5555 Salary 300,000.00 AMD " + stamp(m_now.addSecs(-3600)));
    QCOMPARE(r.status, sms::status::Rejected);
    QCOMPARE(r.errorCode, sms::errorcode::BankDisabled);
    QCOMPARE(transactionCount(), 0);
    svc()->setBankEnabled("acba", true);
    QCOMPARE(send("ACBA: Card *5555 Salary 300,000.00 AMD " + stamp(m_now.addSecs(-3000))).status, sms::status::Imported);
}

void tst_SmsImport::cardNumberIsMaskedInStoredBody()
{
    const ImportResult r = send("IDBank: card 4111111111111111 purchase 100.00 AMD SHOP");
    QVERIFY(r.smsImportId > 0);
    const QString body = storedBody(r.smsImportId);
    QVERIFY2(!body.contains("4111111111111111"), qPrintable(body));
    QVERIFY(!body.isEmpty());
}

// ---------------------------------------------------------------------------
// Accounts
// ---------------------------------------------------------------------------

void tst_SmsImport::createsAccountWhenNoneMatches()
{
    const ImportResult r = send(
        "IDBank: Purchase 12,500.00 AMD, SAS SUPERMARKET YEREVAN, Card *1234, 15.11.2026 14:32, Balance: 245,000.00 AMD");
    QCOMPARE(r.status, sms::status::Imported);
    const Transaction t = transaction(r.transactionId);
    QVERIFY(t.accountId != m_ameria && t.accountId != m_acba);
    // The new account starts so that its balance equals the balance the bank reported.
    QCOMPARE(balanceOf(t.accountId), qint64(24500000));
    const auto account = m_app->accountRepository().byId(t.accountId);
    QCOMPARE(account.value().bankName, QString("IDBank"));
    QCOMPARE(account.value().lastFour, QString("1234"));
    QCOMPARE(account.value().currency(), QString("AMD"));

    // The next message for the same card reuses it.
    const ImportResult next = send("IDBank: Purchase 1,000.00 AMD, SAS SUPERMARKET YEREVAN, Card *1234, 15.11.2026 15:10");
    QCOMPARE(transaction(next.transactionId).accountId, t.accountId);
}

void tst_SmsImport::doesNotCreateAccountWhenSwitchedOff()
{
    svc()->setAutoCreateAccounts(false);
    const ImportResult r = send(
        "IDBank: Purchase 12,500.00 AMD, SAS SUPERMARKET YEREVAN, Card *1234, 15.11.2026 14:32, Balance: 245,000.00 AMD");
    QCOMPARE(r.status, sms::status::Rejected);
    QCOMPARE(r.errorCode, sms::errorcode::NoAccount);
    QCOMPARE(transactionCount(), 0);
    QCOMPARE(m_app->accountRepository().summaries(true).value().size(), 2); // nothing was created
}

void tst_SmsImport::severalCandidateAccountsNeedReview()
{
    makeAccount("Ameriabank", "Savings", "9999", 0); // two Ameriabank AMD cards; the SMS names none of them
    const ImportResult r = send("Ameriabank\nPurchase: 12,500.00 AMD\nSAS SUPERMARKET\n" + stamp(m_now.addSecs(-3600)));
    QVERIFY2(r.status == sms::status::NeedsReview, qPrintable(r.status + " / " + r.errorCode));
    QVERIFY(r.reviewReason == sms::review::Account || r.reviewReason == sms::review::Parse);
    QCOMPARE(transactionCount(), 1);
}

void tst_SmsImport::matchesBankNameCaseAndSuffixInsensitively()
{
    // "ACBA Bank" (account) vs "ACBA" (parser): same bank.
    const ImportResult r = send("ACBA: Card *5555 Salary 10,000.00 AMD " + stamp(m_now.addSecs(-3600)));
    QCOMPARE(transaction(r.transactionId).accountId, m_acba);
    QCOMPARE(m_app->accountRepository().summaries(true).value().size(), 2);
}

// ---------------------------------------------------------------------------
// Setup status / channels
// ---------------------------------------------------------------------------

void tst_SmsImport::testMessageVerifiesLinkWithoutTransaction()
{
    QCOMPARE(svc()->summary().value("level").toString(), QString("notSetUp"));
    const ImportResult r = send("PF-TEST connection check");
    QCOMPARE(r.status, sms::status::Ignored);
    QCOMPARE(r.errorCode, QString("test"));
    QCOMPARE(transactionCount(), 0);
    QCOMPARE(svc()->summary().value("level").toString(), QString("linkVerified"));
    QVERIFY(svc()->summary().value("lastTestAt").toDateTime().isValid());
}

void tst_SmsImport::urlChannelIsUntrustedUntilEnabled()
{
    const QString body = "ACBA: Card *5555 Purchase 1,800.00 AMD " + stamp(m_now.addSecs(-3600)) + " STARBUCKS";
    const ImportResult a = send(body, "", "", sms::channel::UrlScheme);
    QCOMPARE(a.status, sms::status::NeedsReview);
    QCOMPARE(a.reviewReason, sms::review::UntrustedChannel);

    svc()->setTrustUrlChannel(true);
    const ImportResult b = send(QString(body).replace("1,800.00", "1,900.00"), "", "", sms::channel::UrlScheme);
    QCOMPARE(b.status, sms::status::Imported);
}

void tst_SmsImport::summaryLevels()
{
    QCOMPARE(svc()->summary().value("level").toString(), QString("notSetUp"));
    send("PF-TEST");
    QCOMPARE(svc()->summary().value("level").toString(), QString("linkVerified"));
    send(kAmeriaPurchase, "", "", sms::channel::ShortcutIntent, "", m_now.addDays(-45)); // an old message
    QCOMPARE(svc()->summary().value("level").toString(), QString("stale"));
    send("ACBA: Card *5555 Salary 1,000.00 AMD " + stamp(m_now.addSecs(-3600)));
    QCOMPARE(svc()->summary().value("level").toString(), QString("active"));
}

// ---------------------------------------------------------------------------
// Receivers
// ---------------------------------------------------------------------------

void tst_SmsImport::inboxFolderIsIngested()
{
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QJsonObject env{{"v", 1},
                    {"id", id},
                    {"channel", "shortcut_intent"},
                    {"receivedAt", m_now.toUTC().toString(Qt::ISODate)},
                    {"sender", "Ameriabank"},
                    {"body", kAmeriaPurchase},
                    {"bank", "ameriabank"}};
    writeEnvelope(inboxDir(), "one", QJsonDocument(env).toJson());
    // A half-written file must never be read.
    QFile partial(inboxDir() + "/two.json.tmp");
    QVERIFY(partial.open(QIODevice::WriteOnly));
    partial.write("{\"v\":1,");
    partial.close();

    QCOMPARE(m_app->onAppActivated(), 1);
    QCOMPARE(transactionCount(), 1);
    QVERIFY(!QFile::exists(inboxDir() + "/one.json")); // removed only after it was stored
    QVERIFY(QFile::exists(inboxDir() + "/two.json.tmp")); // untouched

    // Delivering the same envelope again (e.g. a crash before the file was removed) changes nothing.
    writeEnvelope(inboxDir(), "one-again", QJsonDocument(env).toJson());
    m_app->onAppActivated();
    QCOMPARE(transactionCount(), 1);
    QVERIFY(!QFile::exists(inboxDir() + "/one-again.json"));

    const QVariantMap s = svc()->summary();
    QCOMPARE(s.value("pendingFiles").toInt(), 0);
    QVERIFY(s.value("receiverAvailable").toBool());
}

void tst_SmsImport::badEnvelopesAreRecordedAndRemoved()
{
    writeEnvelope(inboxDir(), "garbage", "this is not json");
    writeEnvelope(inboxDir(), "wrong-version", R"({"v":99,"id":"x","body":"IDBank: Purchase 5.00 AMD"})");
    writeEnvelope(inboxDir(), "huge", QByteArray(70 * 1024, 'x'));
    writeEnvelope(inboxDir(), "no-body", R"({"v":1,"id":"abc"})");
    m_app->onAppActivated();
    QCOMPARE(transactionCount(), 0);
    QVERIFY(QDir(inboxDir()).entryList({"*.json"}).isEmpty()); // nothing piles up
    QVERIFY(svc()->summary().value("errorCount").toInt() >= 1); // and the problem is visible
}

void tst_SmsImport::envelopeReaderRules()
{
    QTemporaryDir d;
    const auto read = [&](const QByteArray &json, bool *ok) {
        QFile f(d.path() + "/e.json");
        f.open(QIODevice::WriteOnly | QIODevice::Truncate);
        f.write(json);
        f.close();
        return sms::DirectoryInboxReceiver::readEnvelope(f.fileName(), ok);
    };
    bool ok = false;
    sms::IncomingSms m = read(R"({"v":1,"id":"i-1","channel":"shortcut_intent","receivedAt":"2026-11-15T10:32:07Z",
        "sender":"IDBank","body":"hello","bank":"idbank"})", &ok);
    QVERIFY(ok);
    QCOMPARE(m.body, QString("hello"));
    QCOMPARE(m.externalId, QString("i-1"));
    QCOMPARE(m.bankHint, QString("idbank"));
    QCOMPARE(m.receivedAt.toUTC(), QDateTime(QDate(2026, 11, 15), QTime(10, 32, 7), Qt::UTC));

    m = read(R"({"v":1,"body":42})", &ok); // wrong type
    QVERIFY(!ok);
    QVERIFY(m.body.isEmpty());
    m = read(R"([1,2,3])", &ok);
    QVERIFY(!ok);
    m = read(R"({"v":2,"body":"x"})", &ok);
    QVERIFY(!ok);
    m = read(QByteArray(sms::DirectoryInboxReceiver::kMaxFileBytes + 1, ' '), &ok);
    QVERIFY(!ok);
}

void tst_SmsImport::urlParsing()
{
    using sms::UrlImportHandler;
    const auto ok = UrlImportHandler::parse(QUrl(
        "personalfinance://import?body=ACBA%3A%20Card%20*5555%20Salary%201.00%20AMD&sender=ACBA&bank=acba&ts=1763200000"));
    QVERIFY(ok.has_value());
    QCOMPARE(ok->body, QString("ACBA: Card *5555 Salary 1.00 AMD"));
    QCOMPARE(ok->channel, sms::channel::UrlScheme);
    QCOMPARE(ok->bankHint, QString("acba"));
    QCOMPARE(ok->receivedAt.toSecsSinceEpoch(), qint64(1763200000));

    QVERIFY(!UrlImportHandler::parse(QUrl("personalfinance://import?sender=x")).has_value()); // no body
    QVERIFY(!UrlImportHandler::parse(QUrl("personalfinance://settings?body=x")).has_value()); // another action
    QVERIFY(!UrlImportHandler::parse(QUrl("https://example.com/import?body=x")).has_value()); // another scheme
    const QString huge(UrlImportHandler::kMaxUrlBodyChars + 1, 'a');
    const auto big = UrlImportHandler::parse(QUrl("personalfinance://import?body=" + huge));
    QVERIFY(!big.has_value() || big->body.size() <= UrlImportHandler::kMaxUrlBodyChars + 1);
}

void tst_SmsImport::incomingUrlGoesThroughPipeline()
{
    QUrl url("personalfinance://import");
    QUrlQuery q;
    q.addQueryItem("body", "ACBA: Card *5555 Salary 250,000.00 AMD " + stamp(m_now.addSecs(-3600)));
    q.addQueryItem("bank", "acba");
    url.setQuery(q);
    QVERIFY(m_app->handleIncomingUrl(url));
    QCOMPARE(transactionCount(), 1);
    QCOMPARE(svc()->reviewItems().size(), 1); // untrusted channel -> the user confirms it
    QVERIFY(!m_app->handleIncomingUrl(QUrl("personalfinance://nothing")));
}

// ---------------------------------------------------------------------------
// QML facade
// ---------------------------------------------------------------------------

void tst_SmsImport::facadeListsHaveExpectedFields()
{
    send(kAmeriaPurchase);
    send("ACBA: Card *5555 Purchase 6,700.00 AMD " + stamp(m_now.addSecs(-3600)) + " ZZ TRADING 77");
    send("UniBankX: purchase 1,000 AMD at SOME SHOP");

    const QVariantList history = svc()->history(50);
    QCOMPARE(history.size(), 2);
    const QStringList keys{"id", "status", "bankId", "bankName", "channel", "receivedAt", "importedAt", "amountText",
                           "type", "merchant", "categoryName", "categoryIcon", "categoryColor", "confidence",
                           "transactionId", "errorCode", "snippet", "suggestedName"};
    for (const QString &k : keys)
        QVERIFY2(history.first().toMap().contains(k), qPrintable(k));

    QCOMPARE(svc()->errors(50).size(), 1);
    const QVariantList banks = svc()->banks();
    QVERIFY(banks.size() >= 3);
    QVariantMap acba;
    for (const QVariant &b : banks) {
        if (b.toMap().value("id") == "acba")
            acba = b.toMap();
    }
    QVERIFY(!acba.isEmpty());
    QVERIFY(acba.value("seen").toBool());
    QVERIFY(!acba.value("filterPhrases").toStringList().isEmpty());
}

void tst_SmsImport::clearHistoryKeepsReviewItems()
{
    send("ACBA: Card *5555 Purchase 6,700.00 AMD " + stamp(m_now.addSecs(-3600)) + " ZZ TRADING 77"); // review
    send("UniBankX: purchase 1,000 AMD at SOME SHOP"); // error
    send("PF-TEST"); // ignored
    svc()->clearHistory();
    QCOMPARE(svc()->reviewItems().size(), 1);
    QVERIFY(svc()->errors(10).isEmpty());
    QCOMPARE(transactionCount(), 1);
}

QTEST_GUILESS_MAIN(tst_SmsImport)
#include "tst_SmsImport.moc"
