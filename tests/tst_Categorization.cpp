// Merchant normalization and rule-based categorization. Merchant strings are SYNTHETIC examples of the
// kind of text a bank puts in an SMS; real statements will differ (tests/sms_samples/README.md).

#include "categorization/MerchantNormalizer.h"
#include "categorization/StarterRules.h"
#include "core/ApplicationManager.h"

#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtTest>

using namespace pf;

namespace {
template<typename R>
bool good(const R &result)
{
    return static_cast<bool>(result);
}
} // namespace

class tst_Categorization : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    std::unique_ptr<ApplicationManager> m_app;

    QString keyOf(const CategorySuggestion &s) const
    {
        return s.categoryId ? m_app->categoryRepository().byId(*s.categoryId).value().key : QString();
    }
    qint64 idOf(const char *key) const { return m_app->categoryRepository().byKey(QString::fromLatin1(key)).value().id; }

private slots:
    void init();
    void cleanup();

    void normalizes_data();
    void normalizes();
    void normalizerIsIdempotentOnItsKey();

    void classifies_data();
    void classifies();
    void ambiguousIsSuggestionOnly_data();
    void ambiguousIsSuggestionOnly();
    void directionDecidesBetweenIncomeAndExpense();
    void userRuleBeatsStarterRule();
    void rememberMerchantUpdatesInsteadOfDuplicating();
    void disabledRuleIsIgnored();
    void systemRulesCannotBeDeletedUserRulesCan();
    void starterRulesAreWellFormed();
};

void tst_Categorization::init()
{
    QVERIFY(m_dir.isValid());
    m_app = std::make_unique<ApplicationManager>(m_dir.path() + "/t.sqlite");
    QVERIFY(m_app->initialize());
}

void tst_Categorization::cleanup()
{
    m_app.reset();
    QDir(m_dir.path()).removeRecursively();
    QDir().mkpath(m_dir.path());
}

// ---------------------------------------------------------------------------

void tst_Categorization::normalizes_data()
{
    QTest::addColumn<QString>("raw");
    QTest::addColumn<QString>("display");
    QTest::addColumn<QString>("key");

    QTest::newRow("caps-with-city") << "SAS SUPERMARKET YEREVAN" << "SAS Supermarket" << "sas supermarket";
    QTest::newRow("pos-prefix-terminal-and-store-number") << "POS 1234 CARREFOUR 12 YEREVAN AM" << "Carrefour" << "carrefour";
    QTest::newRow("legal-form-and-quotes") << QString::fromUtf8("ООО «Пятёрочка»") << QString::fromUtf8("Пятёрочка") << QString::fromUtf8("пятёрочка");
    QTest::newRow("armenian-legal-form") << QString::fromUtf8("ՍՊԸ «Նոր Դար»") << QString::fromUtf8("Նոր Դար") << QString::fromUtf8("նոր դար");
    QTest::newRow("atm-with-terminal-number") << "ATM 1234 YEREVAN" << "ATM" << "atm";
    QTest::newRow("comma-separated-city") << "Cafe Central, Yerevan, AM" << "Cafe Central" << "cafe central";
    QTest::newRow("whitespace") << "  SHOP  \t X " << "Shop X" << "shop x";
    QTest::newRow("web-prefix") << "www.netflix.com" << "netflix.com" << "netflix.com";
    QTest::newRow("only-a-number-keeps-raw") << "1234" << "1234" << "1234";
    QTest::newRow("empty") << "" << "" << "";
}

void tst_Categorization::normalizes()
{
    QFETCH(QString, raw);
    QFETCH(QString, display);
    QFETCH(QString, key);
    const NormalizedMerchant n = MerchantNormalizer::normalize(raw);
    QCOMPARE(n.display, display);
    QCOMPARE(n.key, key);
}

void tst_Categorization::normalizerIsIdempotentOnItsKey()
{
    // "Remember this merchant" stores the key; matching it again must give the same key.
    for (const QString &raw : {QString("SAS SUPERMARKET YEREVAN"), QString("POS 1234 CARREFOUR 12 YEREVAN AM"),
                               QString::fromUtf8("ООО «Пятёрочка» 5"), QString("ZZ TRADING 77")}) {
        const NormalizedMerchant once = MerchantNormalizer::normalize(raw);
        QCOMPARE(MerchantNormalizer::normalize(once.key).key, once.key);
        QCOMPARE(MerchantNormalizer::normalize(once.display).key, once.key);
    }
}

// ---------------------------------------------------------------------------

void tst_Categorization::classifies_data()
{
    QTest::addColumn<QString>("merchant");
    QTest::addColumn<QString>("kind");
    QTest::addColumn<int>("type");
    QTest::addColumn<QString>("category");
    QTest::addColumn<bool>("autoAssign"); // applied silently?

    const int E = int(TransactionType::Expense);
    const int I = int(TransactionType::Income);

    // The seven classifications named in the product brief, in English / Russian / Armenian.
    QTest::newRow("supermarket-en") << "SAS SUPERMARKET" << "Purchase" << E << "groceries" << true;
    QTest::newRow("supermarket-chain-yerevan-city") << "YEREVAN CITY" << "Purchase" << E << "groceries" << true;
    QTest::newRow("supermarket-ru") << QString::fromUtf8("Магазин Продукты") << QString::fromUtf8("Покупка") << E << "groceries" << true;
    QTest::newRow("restaurant-en") << "CAFE CENTRAL" << "Purchase" << E << "restaurants" << true;
    QTest::newRow("restaurant-hy") << QString::fromUtf8("ՌԵՍՏՈՐԱՆ ՆՈՐ ԴԱՐ") << QString::fromUtf8("Գնում") << E << "restaurants" << true;
    QTest::newRow("starbucks") << "STARBUCKS" << "Purchase" << E << "restaurants" << true;
    QTest::newRow("gas-station-en") << "SHELL GAS STATION" << "Purchase" << E << "transportation" << true;
    QTest::newRow("gas-station-ru") << QString::fromUtf8("АЗС Флэш") << QString::fromUtf8("Покупка") << E << "transportation" << true;
    QTest::newRow("fuel-brand") << "GAZPROM NEFT" << "Purchase" << E << "transportation" << true;
    QTest::newRow("taxi") << "YANDEX GO" << "Purchase" << E << "transportation" << true;
    QTest::newRow("pharmacy-en") << "PHARMACY NATALI" << "Purchase" << E << "healthcare" << true;
    QTest::newRow("pharmacy-ru") << QString::fromUtf8("Аптека Алексанян") << QString::fromUtf8("Покупка") << E << "healthcare" << true;
    QTest::newRow("pharmacy-hy") << QString::fromUtf8("ԴԵՂԱՏՈՒՆ") << QString::fromUtf8("Գնում") << E << "healthcare" << true;
    QTest::newRow("online-shopping") << "WILDBERRIES" << "Purchase" << E << "shopping" << true;
    QTest::newRow("online-shopping-fashion") << "ZARA" << "Purchase" << E << "shopping" << true;
    QTest::newRow("salary-by-operation-word") << "" << "Salary" << I << "salary" << true;
    QTest::newRow("salary-ru") << "" << QString::fromUtf8("Зарплата") << I << "salary" << true;
    QTest::newRow("salary-hy") << "" << QString::fromUtf8("Աշխատավարձ") << I << "salary" << true;
    QTest::newRow("atm-cash") << "ATM 1234 YEREVAN" << "Cash withdrawal" << E << "cash" << true;
    QTest::newRow("cash-ru") << QString::fromUtf8("Банкомат") << QString::fromUtf8("Снятие наличных") << E << "cash" << true;
    QTest::newRow("cash-hy") << QString::fromUtf8("Բանկոմատ") << QString::fromUtf8("Կանխիկացում") << E << "cash" << true;
    QTest::newRow("utilities") << "TEAM TELECOM" << "Purchase" << E << "utilities" << true;
    QTest::newRow("streaming") << "NETFLIX.COM" << "Purchase" << E << "entertainment" << true;
    // Nothing known.
    QTest::newRow("unknown-merchant") << "ZZ TRADING" << "Purchase" << E << "" << false;
    QTest::newRow("empty-merchant-plain-purchase") << "" << "Purchase" << E << "" << false;
}

void tst_Categorization::classifies()
{
    QFETCH(QString, merchant);
    QFETCH(QString, kind);
    QFETCH(int, type);
    QFETCH(QString, category);
    QFETCH(bool, autoAssign);
    const CategorySuggestion s = m_app->categorizationService()->classify(merchant, kind, TransactionType(type));
    QCOMPARE(keyOf(s), category);
    QCOMPARE(s.autoAssign(CategorizationService::kAutoAssignThreshold), autoAssign);
}

void tst_Categorization::ambiguousIsSuggestionOnly_data()
{
    QTest::addColumn<QString>("merchant");
    QTest::addColumn<QString>("category"); // what is proposed

    // Delivery apps carry meals and groceries; "bar"/"pub" may be a shop or a venue.
    QTest::newRow("glovo") << "GLOVO" << "restaurants";
    QTest::newRow("yandex-food") << "Yandex Food" << "restaurants";
    QTest::newRow("bar") << "BAR 24" << "restaurants";
}

void tst_Categorization::ambiguousIsSuggestionOnly()
{
    QFETCH(QString, merchant);
    QFETCH(QString, category);
    const CategorySuggestion s = m_app->categorizationService()->classify(merchant, "Purchase", TransactionType::Expense);
    QCOMPARE(keyOf(s), category);
    QVERIFY2(!s.autoAssign(CategorizationService::kAutoAssignThreshold), "ambiguous merchants must not be applied silently");
    QVERIFY(s.confidence > 0);
}

void tst_Categorization::directionDecidesBetweenIncomeAndExpense()
{
    // "Salary" as an operation word on an expense must not yield the (income-only) Salary category.
    const CategorySuggestion expense = m_app->categorizationService()->classify("", "Salary", TransactionType::Expense);
    QVERIFY(keyOf(expense) != "salary");
    const CategorySuggestion income = m_app->categorizationService()->classify("", "Salary", TransactionType::Income);
    QCOMPARE(keyOf(income), QString("salary"));
}

void tst_Categorization::userRuleBeatsStarterRule()
{
    auto *svc = m_app->categorizationService();
    QCOMPARE(keyOf(svc->classify("SAS SUPERMARKET", "Purchase", TransactionType::Expense)), QString("groceries"));
    // The user decides SAS purchases are "shopping" for them.
    const auto rule = svc->rememberMerchant("SAS SUPERMARKET YEREVAN", idOf("shopping"));
    QVERIFY2(good(rule), qPrintable(rule.error()));
    const CategorySuggestion s = svc->classify("SAS SUPERMARKET", "Purchase", TransactionType::Expense);
    QCOMPARE(keyOf(s), QString("shopping"));
    QVERIFY(s.isUserRule);
    QCOMPARE(s.confidence, 100);
    // Spelling variants that normalize to the same merchant are covered too.
    QCOMPARE(keyOf(svc->classify("POS 77 SAS SUPERMARKET 3 YEREVAN", "Purchase", TransactionType::Expense)), QString("shopping"));
}

void tst_Categorization::rememberMerchantUpdatesInsteadOfDuplicating()
{
    auto *svc = m_app->categorizationService();
    int userRules = 0;
    const auto countUser = [&] {
        int n = 0;
        for (const QVariant &v : svc->rules()) {
            if (!v.toMap().value("isSystem").toBool())
                ++n;
        }
        return n;
    };
    QVERIFY(good(svc->rememberMerchant("ZZ TRADING 77", idOf("shopping"))));
    userRules = countUser();
    QCOMPARE(userRules, 1);
    QVERIFY(good(svc->rememberMerchant("ZZ TRADING 78", idOf("healthcare")))); // same merchant after normalization
    QCOMPARE(countUser(), 1);
    QCOMPARE(keyOf(svc->classify("ZZ TRADING", "Purchase", TransactionType::Expense)), QString("healthcare"));
}

void tst_Categorization::disabledRuleIsIgnored()
{
    auto *svc = m_app->categorizationService();
    // Find the system rule that makes "supermarket" groceries and switch it off.
    qint64 ruleId = 0;
    for (const QVariant &v : svc->rules()) {
        const QVariantMap r = v.toMap();
        if (r.value("pattern").toString() == "supermarket" && r.value("isSystem").toBool())
            ruleId = r.value("id").toLongLong();
    }
    QVERIFY(ruleId > 0);
    QVERIFY(svc->setRuleEnabled(ruleId, false).value("ok").toBool());
    QVERIFY(keyOf(svc->classify("SAS SUPERMARKET", "Purchase", TransactionType::Expense)) != "groceries"
            || !svc->classify("SAS SUPERMARKET", "Purchase", TransactionType::Expense).autoAssign(70)
            || svc->classify("SAS SUPERMARKET", "Purchase", TransactionType::Expense).ruleId != ruleId);
    QVERIFY(svc->setRuleEnabled(ruleId, true).value("ok").toBool());
    QCOMPARE(svc->classify("SAS SUPERMARKET", "Purchase", TransactionType::Expense).ruleId, ruleId);
}

void tst_Categorization::systemRulesCannotBeDeletedUserRulesCan()
{
    auto *svc = m_app->categorizationService();
    qint64 systemId = 0;
    for (const QVariant &v : svc->rules()) {
        if (v.toMap().value("isSystem").toBool()) {
            systemId = v.toMap().value("id").toLongLong();
            break;
        }
    }
    QVERIFY(systemId > 0);
    QVERIFY(!svc->deleteRule(systemId).value("ok").toBool());

    const auto user = svc->rememberMerchant("ZZ TRADING", idOf("shopping"));
    QVERIFY(good(user));
    QVERIFY(svc->deleteRule(user.value().id).value("ok").toBool());
    QCOMPARE(keyOf(svc->classify("ZZ TRADING", "Purchase", TransactionType::Expense)), QString());
}

void tst_Categorization::starterRulesAreWellFormed()
{
    // Guards against typos in the ~110 built-in rules: every regex compiles, every category exists,
    // confidences are sane, and nothing silently applies a vague word.
    const QList<StarterRule> &rules = starterRules();
    QVERIFY(rules.size() > 50);
    for (const auto &r : rules) {
        QVERIFY2(good(m_app->categoryRepository().byKey(QString::fromLatin1(r.categoryKey))), r.categoryKey);
        QVERIFY2(r.confidence >= 0 && r.confidence <= 100, r.categoryKey);
        QVERIFY2(r.confidence < 100, "100 is reserved for user rules");
        QVERIFY2(r.priority < CategorizationService::kUserRulePriority, r.categoryKey);
        if (r.matchType == CategorizationRule::MatchType::Regex) {
            const QRegularExpression re(QString::fromUtf8(r.pattern), QRegularExpression::CaseInsensitiveOption);
            QVERIFY2(re.isValid(), r.pattern);
        }
        QVERIFY2(!QString::fromUtf8(r.pattern).trimmed().isEmpty(), r.categoryKey);
    }
}

QTEST_GUILESS_MAIN(tst_Categorization)
#include "tst_Categorization.moc"
