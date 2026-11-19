#include "core/SettingsManager.h"
#include "database/AccountRepository.h"
#include "database/CategoryRepository.h"
#include "database/DatabaseManager.h"
#include "database/Migrations.h"
#include "database/SettingsRepository.h"
#include "database/TransactionRepository.h"
#include "services/AccountService.h"
#include "services/AnalyticsService.h"
#include "services/TransactionService.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtConcurrent>
#include <QtTest>

using namespace pf;

namespace {

Account makeAccount(const QString &name, const QString &currency, qint64 initialMinor = 0)
{
    Account a;
    a.bankName = "Ameriabank";
    a.name = name;
    a.cardNetwork = CardNetwork::Visa;
    a.lastFour = "1234";
    a.initialBalance = Money(initialMinor, currency);
    return a;
}

Transaction makeTx(qint64 accountId, TransactionType type, qint64 minor, const QString &currency,
                   const QDateTime &when, const QString &merchant = QString())
{
    Transaction t;
    t.accountId = accountId;
    t.type = type;
    t.amount = Money(minor, currency);
    t.occurredAt = when;
    t.merchant = merchant;
    return t;
}

} // namespace

class tst_Database : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QScopedPointer<DatabaseManager> m_db;
    QScopedPointer<AccountRepository> m_accounts;
    QScopedPointer<TransactionRepository> m_tx;
    QScopedPointer<CategoryRepository> m_categories;

private slots:
    void init();
    void cleanup();

    void migrationCreatesSchemaAndSeedsCategories();
    void upgradesVersionOneDatabase();
    void reopeningIsIdempotent();
    void refusesNewerDatabase();
    void foreignKeysAreEnforced();

    void accountCrud();
    void accountRejectsFullCardNumber();
    void accountCurrencyLockedOnceTransactionsExist();

    void balanceIsExactIntegerMath();
    void balancePerCurrencyTotals();

    void transactionCrudRoundTrip();
    void transactionRulesRejectBadInput();
    void filterByAccountTypeAndDate();
    void unicodeSearchIsCaseInsensitive();
    void searchEscapesLikeWildcards();
    void paginationIsStableAndNewestFirst();
    void periodTotals();

    void duplicateDetection();
    void fingerprintIgnoresSecondsAndCase();

    void deletingAccountCascades();
    void deletingCategoryUncategorizes();
    void builtInCategoriesAreImmutable();

    void transactionRollsBackOnFailure();
    void worksFromBackgroundThread();
    void settingsPersist();
};

void tst_Database::init()
{
    QVERIFY(m_dir.isValid());
    m_db.reset(new DatabaseManager(m_dir.filePath("test.sqlite")));
    QVERIFY2(m_db->open().isOk(), "open");
    m_accounts.reset(new AccountRepository(*m_db));
    m_tx.reset(new TransactionRepository(*m_db));
    m_categories.reset(new CategoryRepository(*m_db));
}

void tst_Database::cleanup()
{
    m_categories.reset();
    m_tx.reset();
    m_accounts.reset();
    m_db.reset();
    QFile::remove(m_dir.filePath("test.sqlite"));
    QFile::remove(m_dir.filePath("test.sqlite-wal"));
    QFile::remove(m_dir.filePath("test.sqlite-shm"));
}

void tst_Database::migrationCreatesSchemaAndSeedsCategories()
{
    QCOMPARE(m_db->schemaVersion(), 2);

    QSqlQuery q(m_db->connection());
    QVERIFY(q.exec("SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name"));
    QStringList tables;
    while (q.next())
        tables << q.value(0).toString();
    for (const QString &required : {"accounts", "transactions", "categories", "budgets", "categorization_rules",
                                    "import_history", "settings", "sms_imports"}) {
        QVERIFY2(tables.contains(required), qPrintable(required));
    }

    const auto categories = m_categories->all();
    QVERIFY(categories.isOk());
    QCOMPARE(categories.value().size(), 11);
    QStringList keys;
    for (const Category &c : categories.value()) {
        keys << c.key;
        QVERIFY(c.isSystem);
        QVERIFY(!c.icon.isEmpty());
    }
    QCOMPARE(keys,
             QStringList({"groceries", "restaurants", "transportation", "shopping", "healthcare", "utilities",
                          "entertainment", "salary", "transfers", "cash", "other"}));

    // WAL + foreign keys active on this connection.
    QVERIFY(q.exec("PRAGMA journal_mode") && q.next());
    QCOMPARE(q.value(0).toString().toLower(), QString("wal"));
    QVERIFY(q.exec("PRAGMA foreign_keys") && q.next());
    QCOMPARE(q.value(0).toInt(), 1);
}

void tst_Database::upgradesVersionOneDatabase()
{
    // Build a database exactly as a Phase 1 install would have left it (schema v1 only).
    m_categories.reset();
    m_tx.reset();
    m_accounts.reset();
    m_db.reset();
    const QString path = m_dir.filePath("v1.sqlite");
    {
        QSqlDatabase raw = QSqlDatabase::addDatabase("QSQLITE", "pf_v1_fixture");
        raw.setDatabaseName(path);
        QVERIFY(raw.open());
        QSqlQuery pragma(raw);
        QVERIFY(pragma.exec("PRAGMA foreign_keys = ON"));
        QVERIFY(pf::schema::migrations().first().apply(raw).isOk());
        // Phase 1 had 10 built-ins and no `cash`: remove it to mimic that install, and add user data.
        QVERIFY(pragma.exec("DELETE FROM categories WHERE key = 'cash'"));
        QVERIFY(pragma.exec("INSERT INTO accounts (bank_name, name, currency, created_at) VALUES ('Old', 'Kept', 'AMD', 1)"));
        QVERIFY(pragma.exec("PRAGMA user_version = 1"));
        raw.close();
    }
    QSqlDatabase::removeDatabase("pf_v1_fixture");

    DatabaseManager upgraded(path);
    QVERIFY2(upgraded.open().isOk(), "v1 -> v2 upgrade must succeed");
    QCOMPARE(upgraded.schemaVersion(), 2);

    CategoryRepository categories(upgraded);
    const auto all = categories.all();
    QVERIFY(all.isOk());
    QCOMPARE(all.value().size(), 11);
    QVERIFY(categories.byKey("cash").isOk());
    // Order matches Category::builtIn(): cash sits before "other".
    QCOMPARE(all.value().at(9).key, QString("cash"));
    QCOMPARE(all.value().at(10).key, QString("other"));

    QSqlQuery q(upgraded.connection());
    QVERIFY(q.exec("SELECT COUNT(*) FROM categorization_rules WHERE is_system = 1 AND confidence > 0") && q.next());
    QVERIFY2(q.value(0).toInt() > 50, "starter rules were seeded");
    QVERIFY(q.exec("SELECT COUNT(*) FROM accounts WHERE name = 'Kept'") && q.next());
    QCOMPARE(q.value(0).toInt(), 1); // user data untouched
    QVERIFY(q.exec("SELECT COUNT(*) FROM sms_imports") && q.next());
}

void tst_Database::reopeningIsIdempotent()
{
    const auto created = m_accounts->insert(makeAccount("Main", "AMD", 5000));
    QVERIFY(created.isOk());

    m_categories.reset();
    m_tx.reset();
    m_accounts.reset();
    m_db.reset(new DatabaseManager(m_dir.filePath("test.sqlite")));
    QVERIFY(m_db->open().isOk());
    m_accounts.reset(new AccountRepository(*m_db));
    m_categories.reset(new CategoryRepository(*m_db));

    QCOMPARE(m_db->schemaVersion(), 2);
    QCOMPARE(m_categories->all().value().size(), 11); // not seeded twice
    const auto list = m_accounts->summaries();
    QVERIFY(list.isOk());
    QCOMPARE(list.value().size(), 1);
    QCOMPARE(list.value().first().balance.minorUnits(), qint64(5000));
}

void tst_Database::refusesNewerDatabase()
{
    {
        QSqlQuery q(m_db->connection());
        QVERIFY(q.exec("PRAGMA user_version = 99"));
    }
    m_categories.reset();
    m_tx.reset();
    m_accounts.reset();
    m_db.reset(new DatabaseManager(m_dir.filePath("test.sqlite")));
    const Status s = m_db->open();
    QVERIFY(!s.isOk());
    QVERIFY2(s.message().contains("newer"), qPrintable(s.message()));
    // Reset the version so cleanup/other tests are unaffected.
    QSqlQuery q(m_db->connection());
    QVERIFY(q.exec("PRAGMA user_version = 1"));
}

void tst_Database::foreignKeysAreEnforced()
{
    const auto ghost = m_tx->insert(makeTx(9999, TransactionType::Expense, 100, "AMD", QDateTime::currentDateTime()));
    QVERIFY(!ghost.isOk()); // account 9999 does not exist
}

void tst_Database::accountCrud()
{
    auto created = m_accounts->insert(makeAccount("Salary card", "AMD", 100000));
    QVERIFY(created.isOk());
    Account a = created.value();
    QVERIFY(a.id > 0);

    auto loaded = m_accounts->byId(a.id);
    QVERIFY(loaded.isOk());
    QCOMPARE(loaded.value().name, QString("Salary card"));
    QCOMPARE(loaded.value().currency(), QString("AMD"));
    QCOMPARE(loaded.value().cardNetwork, CardNetwork::Visa);
    QCOMPARE(loaded.value().lastFour, QString("1234"));
    QCOMPARE(loaded.value().initialBalance.minorUnits(), qint64(100000));

    a.name = "Renamed";
    a.cardNetwork = CardNetwork::Arca;
    a.lastFour.clear();
    QVERIFY(m_accounts->update(a).isOk());
    loaded = m_accounts->byId(a.id);
    QCOMPARE(loaded.value().name, QString("Renamed"));
    QCOMPARE(loaded.value().cardNetwork, CardNetwork::Arca);
    QVERIFY(loaded.value().lastFour.isEmpty());

    QVERIFY(m_accounts->setArchived(a.id, true).isOk());
    QCOMPARE(m_accounts->summaries(false).value().size(), 0);
    QCOMPARE(m_accounts->summaries(true).value().size(), 1);

    QVERIFY(m_accounts->remove(a.id).isOk());
    QVERIFY(!m_accounts->byId(a.id).isOk());

    Account ghost = a;
    ghost.id = 4242;
    QVERIFY(!m_accounts->update(ghost).isOk());
}

void tst_Database::accountRejectsFullCardNumber()
{
    SettingsRepository settingsRepo(*m_db);
    SettingsManager settings(settingsRepo);
    AccountService service(*m_accounts, settings);

    // Service-level validation.
    Account a = makeAccount("Card", "AMD");
    a.lastFour = "4111111111111111";
    QVERIFY(!service.create(a).isOk());
    a.lastFour = "12a4";
    QVERIFY(!AccountService::validate(a).isOk());
    a.lastFour = "123";
    QVERIFY(!AccountService::validate(a).isOk());
    a.lastFour = "1234";
    QVERIFY(AccountService::validate(a).isOk());
    a.bankName = "  ";
    QVERIFY(!AccountService::validate(a).isOk());

    // Defense in depth: the database CHECK constraint rejects it as well.
    Account bypass = makeAccount("Card", "AMD");
    bypass.lastFour = "4111111111111111";
    QVERIFY(!m_accounts->insert(bypass).isOk());
    bypass.lastFour = "12ab";
    QVERIFY(!m_accounts->insert(bypass).isOk());
}

void tst_Database::accountCurrencyLockedOnceTransactionsExist()
{
    SettingsRepository settingsRepo(*m_db);
    SettingsManager settings(settingsRepo);
    AccountService service(*m_accounts, settings);

    auto acc = service.create(makeAccount("Wallet", "AMD"));
    QVERIFY(acc.isOk());
    Account a = acc.value();

    a.initialBalance = Money(0, "USD");
    QVERIFY(service.update(a).isOk()); // fine while empty

    TransactionService txs(*m_tx, *m_accounts);
    const AddOutcome out = txs.add(makeTx(a.id, TransactionType::Expense, 500, "USD", QDateTime::currentDateTime()));
    QVERIFY2(out.saved, qPrintable(out.error));

    a.initialBalance = Money(0, "EUR");
    const Status s = service.update(a);
    QVERIFY(!s.isOk());
    QVERIFY(s.message().contains("currency"));
}

void tst_Database::balanceIsExactIntegerMath()
{
    const Account a = m_accounts->insert(makeAccount("Main", "USD", 100000)).value(); // $1000.00
    const QDateTime now = QDateTime::currentDateTime();

    // 1000 x $0.10 expenses and 3 incomes; floating point would drift here.
    QVERIFY(m_db->transaction([&]() -> Status {
        for (int i = 0; i < 1000; ++i) {
            const auto r = m_tx->insert(makeTx(a.id, TransactionType::Expense, 10, "USD", now.addSecs(-i)));
            if (!r)
                return r.status();
        }
        for (const qint64 minor : {1000050, 33, 7}) {
            const auto r = m_tx->insert(makeTx(a.id, TransactionType::Income, minor, "USD", now));
            if (!r)
                return r.status();
        }
        return Status::ok();
    }).isOk());

    // 100000 - 1000*10 + 1000050 + 33 + 7 = 1090090
    const auto balance = m_accounts->balance(a.id);
    QVERIFY(balance.isOk());
    QCOMPARE(balance.value().minorUnits(), qint64(1090090));
    QCOMPARE(balance.value().toDecimalString(), QString("10900.90"));

    const auto summaries = m_accounts->summaries();
    QCOMPARE(summaries.value().first().balance.minorUnits(), qint64(1090090));
    QCOMPARE(summaries.value().first().transactionCount, 1003);
}

void tst_Database::balancePerCurrencyTotals()
{
    const Account amd1 = m_accounts->insert(makeAccount("AMD 1", "AMD", 100000)).value();
    const Account amd2 = m_accounts->insert(makeAccount("AMD 2", "AMD", 50000)).value();
    const Account usd = m_accounts->insert(makeAccount("USD", "USD", 2500)).value();
    QVERIFY(m_tx->insert(makeTx(amd1.id, TransactionType::Expense, 30000, "AMD", QDateTime::currentDateTime())).isOk());
    QVERIFY(m_tx->insert(makeTx(usd.id, TransactionType::Income, 500, "USD", QDateTime::currentDateTime())).isOk());
    Account archived = amd2;
    archived.archived = true;
    QVERIFY(m_accounts->update(archived).isOk());

    auto totals = m_accounts->totalsByCurrency(false);
    QVERIFY(totals.isOk());
    QCOMPARE(totals.value().size(), 2);
    QHash<QString, qint64> byCurrency;
    for (const Money &m : totals.value())
        byCurrency[m.currency()] = m.minorUnits();
    QCOMPARE(byCurrency["AMD"], qint64(70000)); // archived account excluded
    QCOMPARE(byCurrency["USD"], qint64(3000));

    totals = m_accounts->totalsByCurrency(true);
    for (const Money &m : totals.value())
        byCurrency[m.currency()] = m.minorUnits();
    QCOMPARE(byCurrency["AMD"], qint64(120000));
}

void tst_Database::transactionCrudRoundTrip()
{
    const Account a = m_accounts->insert(makeAccount("Main", "AMD")).value();
    const Category groceries = m_categories->byKey("groceries").value();

    Transaction t = makeTx(a.id, TransactionType::Expense, 123450, "AMD",
                           QDateTime(QDate(2026, 3, 14), QTime(18, 30, 15)), "  Carrefour  ");
    t.description = "Weekly shopping";
    t.categoryId = groceries.id;
    t.source = TransactionSource::PastedNotification;

    const auto inserted = m_tx->insert(t);
    QVERIFY(inserted.isOk());
    QVERIFY(inserted.value().id > 0);

    auto loaded = m_tx->byId(inserted.value().id);
    QVERIFY(loaded.isOk());
    const Transaction &l = loaded.value();
    QCOMPARE(l.accountId, a.id);
    QCOMPARE(l.categoryId.value_or(0), groceries.id);
    QCOMPARE(l.type, TransactionType::Expense);
    QCOMPARE(l.amount, Money(123450, "AMD"));
    QCOMPARE(l.occurredAt, t.occurredAt);
    QCOMPARE(l.merchant, QString("Carrefour")); // trimmed
    QCOMPARE(l.description, QString("Weekly shopping"));
    QCOMPARE(l.source, TransactionSource::PastedNotification);
    QVERIFY(!l.importId.has_value());
    QCOMPARE(l.signedAmount().minorUnits(), qint64(-123450));

    Transaction edited = l;
    edited.type = TransactionType::Income;
    edited.amount = Money(999, "AMD");
    edited.categoryId.reset();
    edited.merchant = "Refund";
    QVERIFY(m_tx->update(edited).isOk());
    loaded = m_tx->byId(l.id);
    QCOMPARE(loaded.value().type, TransactionType::Income);
    QCOMPARE(loaded.value().amount.minorUnits(), qint64(999));
    QVERIFY(!loaded.value().categoryId.has_value());
    QCOMPARE(loaded.value().signedAmount().minorUnits(), qint64(999));

    QVERIFY(m_tx->remove(l.id).isOk());
    QVERIFY(!m_tx->byId(l.id).isOk());
    Transaction ghost = l;
    ghost.id = 777777;
    QVERIFY(!m_tx->update(ghost).isOk());
}

void tst_Database::transactionRulesRejectBadInput()
{
    SettingsRepository settingsRepo(*m_db);
    SettingsManager settings(settingsRepo);
    TransactionService svc(*m_tx, *m_accounts);
    const Account amd = m_accounts->insert(makeAccount("AMD", "AMD")).value();
    const QDateTime now = QDateTime::currentDateTime();

    QVERIFY(!svc.add(makeTx(amd.id, TransactionType::Expense, 0, "AMD", now)).saved); // zero
    QVERIFY(!svc.add(makeTx(amd.id, TransactionType::Expense, -5, "AMD", now)).saved); // negative
    QVERIFY(!svc.add(makeTx(amd.id, TransactionType::Expense, 100, "USD", now)).saved); // currency mismatch
    QVERIFY(!svc.add(makeTx(12345, TransactionType::Expense, 100, "AMD", now)).saved); // unknown account
    QVERIFY(!svc.add(makeTx(amd.id, TransactionType::Expense, 100, "AMD", QDateTime())).saved); // no date
    QVERIFY(!svc.add(makeTx(amd.id, TransactionType::Expense, 100, "AMD", now.addYears(1))).saved); // far future
    QVERIFY(!svc.add(makeTx(amd.id, TransactionType::Expense, 100, "AMD", QDateTime(QDate(1990, 1, 1), QTime()))).saved);
    QCOMPARE(m_tx->count({}).value(), 0);

    // QML facade parses the text amount with the account's currency.
    QVariantMap fields{{"accountId", amd.id}, {"type", "expense"}, {"amount", "1 250,50"},
                       {"occurredAt", now}, {"merchant", "Test"}};
    const QVariantMap saved = svc.save(fields);
    QVERIFY2(saved.value("ok").toBool(), qPrintable(saved.value("error").toString()));
    QCOMPARE(m_tx->byId(saved.value("id").toLongLong()).value().amount.minorUnits(), qint64(125050));

    fields["amount"] = "abc";
    QVERIFY(!svc.save(fields).value("ok").toBool());
    fields["amount"] = "10";
    fields["type"] = "bogus";
    QVERIFY(!svc.save(fields).value("ok").toBool());
}

void tst_Database::filterByAccountTypeAndDate()
{
    const Account a1 = m_accounts->insert(makeAccount("A1", "AMD")).value();
    const Account a2 = m_accounts->insert(makeAccount("A2", "AMD")).value();
    const Category salary = m_categories->byKey("salary").value();
    const QDateTime day1(QDate(2026, 5, 1), QTime(10, 0));
    const QDateTime day2(QDate(2026, 5, 2), QTime(10, 0));
    const QDateTime day3(QDate(2026, 5, 3), QTime(10, 0));

    QVERIFY(m_tx->insert(makeTx(a1.id, TransactionType::Expense, 100, "AMD", day1)).isOk());
    Transaction income = makeTx(a1.id, TransactionType::Income, 5000, "AMD", day2);
    income.categoryId = salary.id;
    QVERIFY(m_tx->insert(income).isOk());
    QVERIFY(m_tx->insert(makeTx(a2.id, TransactionType::Expense, 300, "AMD", day3)).isOk());

    TransactionFilter f;
    QCOMPARE(m_tx->count(f).value(), 3);

    f.accountId = a1.id;
    QCOMPARE(m_tx->count(f).value(), 2);
    f.type = TransactionType::Income;
    QCOMPARE(m_tx->query(f, 10, 0).value().size(), 1);
    f.type.reset();
    f.accountId.reset();

    f.categoryId = salary.id;
    QCOMPARE(m_tx->count(f).value(), 1);
    f.categoryId.reset();
    f.uncategorizedOnly = true;
    QCOMPARE(m_tx->count(f).value(), 2);
    f.uncategorizedOnly = false;

    f.from = day2;
    f.to = day3; // exclusive
    const auto inRange = m_tx->query(f, 10, 0);
    QCOMPARE(inRange.value().size(), 1);
    QCOMPARE(inRange.value().first().amount.minorUnits(), qint64(5000));
}

void tst_Database::unicodeSearchIsCaseInsensitive()
{
    const Account a = m_accounts->insert(makeAccount("Main", "AMD")).value();
    const QDateTime now = QDateTime::currentDateTime();
    const auto add = [&](const QString &merchant, const QString &description = QString()) {
        Transaction t = makeTx(a.id, TransactionType::Expense, 100, "AMD", now.addSecs(m_tx->count({}).value()), merchant);
        t.description = description;
        QVERIFY(m_tx->insert(t).isOk());
    };
    add(QString::fromUtf8("Գրիգոր Լուսավորիչ Սուպերմարկետ")); // Armenian
    add(QString::fromUtf8("Магазин «Пятёрочка»"), QString::fromUtf8("Продукты"));       // Russian
    add("CARREFOUR Yerevan", "weekly");                                               // English
    add("Other Place");

    const auto search = [&](const QString &text) {
        TransactionFilter f;
        f.text = text;
        return m_tx->count(f).value();
    };
    QCOMPARE(search(QString::fromUtf8("գրիգոր")), 1); // lower-case query vs Capitalized Armenian text
    QCOMPARE(search(QString::fromUtf8("ԳՐԻԳՈՐ")), 1); // upper-case query
    QCOMPARE(search(QString::fromUtf8("пятёрочка")), 1);
    QCOMPARE(search(QString::fromUtf8("ПРОДУКТЫ")), 1);
    QCOMPARE(search("carrefour"), 1);
    QCOMPARE(search("CarreFour yerevan"), 1); // all words must match
    QCOMPARE(search("carrefour moscow"), 0);
    QCOMPARE(search("place"), 1);
    QCOMPARE(search(""), 4);
}

void tst_Database::searchEscapesLikeWildcards()
{
    const Account a = m_accounts->insert(makeAccount("Main", "AMD")).value();
    const QDateTime now = QDateTime::currentDateTime();
    QVERIFY(m_tx->insert(makeTx(a.id, TransactionType::Expense, 100, "AMD", now, "100% Juice")).isOk());
    QVERIFY(m_tx->insert(makeTx(a.id, TransactionType::Expense, 100, "AMD", now.addSecs(1), "Plain Water")).isOk());
    QVERIFY(m_tx->insert(makeTx(a.id, TransactionType::Expense, 100, "AMD", now.addSecs(2), "snake_case shop")).isOk());

    TransactionFilter f;
    f.text = "%";
    QCOMPARE(m_tx->count(f).value(), 1); // only the literal percent sign
    f.text = "_";
    QCOMPARE(m_tx->count(f).value(), 1); // only the literal underscore
    f.text = "e_c";
    QCOMPARE(m_tx->count(f).value(), 1);
    f.text = "a_c";
    QCOMPARE(m_tx->count(f).value(), 0); // "_" is not a one-char wildcard
    f.text = "\\";
    QCOMPARE(m_tx->count(f).value(), 0);
    f.text = "' OR 1=1 --";
    QCOMPARE(m_tx->count(f).value(), 0); // bound parameter, not SQL
}

void tst_Database::paginationIsStableAndNewestFirst()
{
    const Account a = m_accounts->insert(makeAccount("Main", "AMD")).value();
    const QDateTime base(QDate(2026, 1, 1), QTime(12, 0));
    QVERIFY(m_db->transaction([&]() -> Status {
        for (int i = 0; i < 95; ++i) {
            // Several rows share a timestamp to exercise the id tiebreak.
            const auto r = m_tx->insert(makeTx(a.id, TransactionType::Expense, 100 + i, "AMD", base.addSecs((i / 5) * 60)));
            if (!r)
                return r.status();
        }
        return Status::ok();
    }).isOk());

    QSet<qint64> seen;
    qint64 lastTime = std::numeric_limits<qint64>::max();
    for (int offset = 0; offset < 95; offset += 40) {
        const auto page = m_tx->query({}, 40, offset);
        QVERIFY(page.isOk());
        for (const Transaction &t : page.value()) {
            QVERIFY2(!seen.contains(t.id), "row returned twice");
            seen.insert(t.id);
            QVERIFY(t.occurredAt.toSecsSinceEpoch() <= lastTime);
            lastTime = t.occurredAt.toSecsSinceEpoch();
        }
    }
    QCOMPARE(seen.size(), 95);
}

void tst_Database::periodTotals()
{
    const Account amd = m_accounts->insert(makeAccount("AMD", "AMD")).value();
    const Account usd = m_accounts->insert(makeAccount("USD", "USD")).value();
    const auto at = [](int m, int d) { return QDateTime(QDate(2026, m, d), QTime(12, 0)); };

    QVERIFY(m_tx->insert(makeTx(amd.id, TransactionType::Income, 500000, "AMD", at(3, 1))).isOk());
    QVERIFY(m_tx->insert(makeTx(amd.id, TransactionType::Expense, 120050, "AMD", at(3, 15))).isOk());
    QVERIFY(m_tx->insert(makeTx(amd.id, TransactionType::Expense, 80000, "AMD", at(3, 31))).isOk());
    QVERIFY(m_tx->insert(makeTx(amd.id, TransactionType::Expense, 999, "AMD", at(4, 1))).isOk()); // next month
    QVERIFY(m_tx->insert(makeTx(amd.id, TransactionType::Expense, 999, "AMD", at(2, 28))).isOk()); // previous month
    QVERIFY(m_tx->insert(makeTx(usd.id, TransactionType::Expense, 2599, "USD", at(3, 10))).isOk());

    const auto range = AnalyticsService::monthRange(2026, 3);
    const auto totals = m_tx->totals(range.first, range.second);
    QVERIFY(totals.isOk());
    QCOMPARE(totals.value().size(), 2);
    for (const CurrencyTotals &t : totals.value()) {
        if (t.currency == "AMD") {
            QCOMPARE(t.income.minorUnits(), qint64(500000));
            QCOMPARE(t.expense.minorUnits(), qint64(200050));
            QCOMPARE(t.net().minorUnits(), qint64(299950));
        } else {
            QCOMPARE(t.currency, QString("USD"));
            QCOMPARE(t.income.minorUnits(), qint64(0));
            QCOMPARE(t.expense.minorUnits(), qint64(2599));
        }
    }

    // Restricting to one account.
    const auto onlyUsd = m_tx->totals(range.first, range.second, usd.id);
    QCOMPARE(onlyUsd.value().size(), 1);

    // Analytics facade for a currency without any data returns zeros.
    SettingsRepository settingsRepo(*m_db);
    SettingsManager settings(settingsRepo);
    AnalyticsService analytics(*m_tx, settings);
    const auto eur = analytics.monthTotals(2026, 3, "EUR");
    QVERIFY(eur.isOk());
    QVERIFY(eur.value().income.isZero() && eur.value().expense.isZero());
    const auto amdTotals = analytics.monthTotals(2026, 3, "AMD");
    QCOMPARE(amdTotals.value().expense.minorUnits(), qint64(200050));
}

void tst_Database::duplicateDetection()
{
    TransactionService svc(*m_tx, *m_accounts);
    const Account a = m_accounts->insert(makeAccount("Main", "AMD")).value();
    const Account other = m_accounts->insert(makeAccount("Other", "AMD")).value();
    const QDateTime t0(QDate(2026, 6, 1), QTime(12, 0));

    AddOutcome first = svc.add(makeTx(a.id, TransactionType::Expense, 4500, "AMD", t0, "Cafe"));
    QVERIFY(first.saved);

    // Same account/amount/type 3 minutes later => flagged, NOT saved.
    AddOutcome dup = svc.add(makeTx(a.id, TransactionType::Expense, 4500, "AMD", t0.addSecs(180), "Cafe"));
    QVERIFY(!dup.saved);
    QCOMPARE(dup.duplicates.size(), 1);
    QCOMPARE(dup.duplicates.first().id, first.transaction.id);
    QVERIFY(dup.error.isEmpty());
    QCOMPARE(m_tx->count({}).value(), 1);

    // The user may confirm and save anyway.
    QVERIFY(svc.add(makeTx(a.id, TransactionType::Expense, 4500, "AMD", t0.addSecs(180), "Cafe"), true).saved);
    QCOMPARE(m_tx->count({}).value(), 2);

    // Different amount / type / account / far apart in time => not duplicates.
    QVERIFY(svc.add(makeTx(a.id, TransactionType::Expense, 4501, "AMD", t0, "Cafe")).saved);
    QVERIFY(svc.add(makeTx(a.id, TransactionType::Income, 4500, "AMD", t0, "Cafe")).saved);
    QVERIFY(svc.add(makeTx(other.id, TransactionType::Expense, 4500, "AMD", t0, "Cafe")).saved);
    QVERIFY(svc.add(makeTx(a.id, TransactionType::Expense, 4500, "AMD", t0.addSecs(3600), "Cafe")).saved);

    // The QML facade reports duplicates distinctly from errors and honours `force`.
    QVariantMap fields{{"accountId", a.id}, {"type", "expense"}, {"amount", "45"},
                       {"occurredAt", t0.addSecs(60)}, {"merchant", "Cafe"}};
    const QVariantMap res = svc.save(fields);
    QVERIFY(!res.value("ok").toBool());
    QVERIFY(res.value("duplicate").toBool());
    fields["force"] = true;
    QVERIFY(svc.save(fields).value("ok").toBool());
}

void tst_Database::fingerprintIgnoresSecondsAndCase()
{
    Transaction a = makeTx(1, TransactionType::Expense, 500, "AMD", QDateTime(QDate(2026, 1, 1), QTime(10, 0, 5)), "  Cafe   Roma ");
    Transaction b = makeTx(1, TransactionType::Expense, 500, "AMD", QDateTime(QDate(2026, 1, 1), QTime(10, 0, 40)), "cafe roma");
    QCOMPARE(a.fingerprint(), b.fingerprint());
    b.amount = Money(501, "AMD");
    QVERIFY(a.fingerprint() != b.fingerprint());
    b = a;
    b.accountId = 2;
    QVERIFY(a.fingerprint() != b.fingerprint());
    b = a;
    b.type = TransactionType::Income;
    QVERIFY(a.fingerprint() != b.fingerprint());
    QCOMPARE(a.fingerprint().size(), 64); // SHA-256 hex
}

void tst_Database::deletingAccountCascades()
{
    const Account a = m_accounts->insert(makeAccount("Main", "AMD")).value();
    const Account b = m_accounts->insert(makeAccount("Keep", "AMD")).value();
    const QDateTime now = QDateTime::currentDateTime();
    QVERIFY(m_tx->insert(makeTx(a.id, TransactionType::Expense, 100, "AMD", now)).isOk());
    QVERIFY(m_tx->insert(makeTx(a.id, TransactionType::Expense, 200, "AMD", now.addSecs(1))).isOk());
    QVERIFY(m_tx->insert(makeTx(b.id, TransactionType::Expense, 300, "AMD", now)).isOk());

    QVERIFY(m_accounts->remove(a.id).isOk());
    QCOMPARE(m_tx->count({}).value(), 1);
    QCOMPARE(m_tx->query({}, 10, 0).value().first().accountId, b.id);
}

void tst_Database::deletingCategoryUncategorizes()
{
    const Account a = m_accounts->insert(makeAccount("Main", "AMD")).value();
    Category custom;
    custom.name = "Pets";
    custom.icon = "🐶";
    custom.color = "#123456";
    custom.kind = CategoryKind::Expense;
    const auto c = m_categories->insertCustom(custom);
    QVERIFY(c.isOk());
    QVERIFY(!c.value().isSystem);
    QCOMPARE(c.value().displayName(), QString("Pets"));

    Transaction t = makeTx(a.id, TransactionType::Expense, 100, "AMD", QDateTime::currentDateTime());
    t.categoryId = c.value().id;
    const Transaction stored = m_tx->insert(t).value();
    QVERIFY(m_categories->removeCustom(c.value().id).isOk());
    QVERIFY(!m_tx->byId(stored.id).value().categoryId.has_value());
}

void tst_Database::builtInCategoriesAreImmutable()
{
    Category groceries = m_categories->byKey("groceries").value();
    QCOMPARE(groceries.displayName(), QString("Groceries"));
    groceries.name = "Hacked";
    QVERIFY(!m_categories->updateCustom(groceries).isOk());
    QVERIFY(!m_categories->removeCustom(groceries.id).isOk());
    QVERIFY(m_categories->byKey("groceries").isOk());

    Category empty;
    QVERIFY(!m_categories->insertCustom(empty).isOk());
}

void tst_Database::transactionRollsBackOnFailure()
{
    const Account a = m_accounts->insert(makeAccount("Main", "AMD", 1000)).value();
    const QDateTime now = QDateTime::currentDateTime();

    const Status s = m_db->transaction([&]() -> Status {
        if (!m_tx->insert(makeTx(a.id, TransactionType::Expense, 100, "AMD", now)))
            return Status::error("unexpected");
        if (!m_tx->insert(makeTx(a.id, TransactionType::Expense, 200, "AMD", now.addSecs(1))))
            return Status::error("unexpected");
        return Status::error("simulated failure after two inserts");
    });
    QVERIFY(!s.isOk());
    QCOMPARE(m_tx->count({}).value(), 0); // nothing persisted
    QCOMPARE(m_accounts->balance(a.id).value().minorUnits(), qint64(1000));

    // Nested transaction joins the outer one: inner success + outer failure => all rolled back.
    const Status nested = m_db->transaction([&]() -> Status {
        const Status inner = m_db->transaction([&]() -> Status {
            return m_tx->insert(makeTx(a.id, TransactionType::Expense, 100, "AMD", now)).status();
        });
        if (!inner)
            return inner;
        return Status::error("outer failure");
    });
    QVERIFY(!nested.isOk());
    QCOMPARE(m_tx->count({}).value(), 0);

    // And a successful transaction commits.
    QVERIFY(m_db->transaction([&]() -> Status {
        return m_tx->insert(makeTx(a.id, TransactionType::Expense, 100, "AMD", now)).status();
    }).isOk());
    QCOMPARE(m_tx->count({}).value(), 1);
}

void tst_Database::worksFromBackgroundThread()
{
    const Account a = m_accounts->insert(makeAccount("Main", "AMD")).value();
    const QDateTime now = QDateTime::currentDateTime();

    // Each worker thread transparently gets its own SQLite connection.
    QList<QFuture<bool>> futures;
    for (int worker = 0; worker < 4; ++worker) {
        futures << QtConcurrent::run([&, worker]() -> bool {
            for (int i = 0; i < 50; ++i) {
                const auto r = m_tx->insert(makeTx(a.id, TransactionType::Expense, 1, "AMD", now.addSecs(worker * 100 + i)));
                if (!r)
                    return false;
            }
            return true;
        });
    }
    for (auto &f : futures)
        QVERIFY(f.result());
    QCOMPARE(m_tx->count({}).value(), 200);
    QCOMPARE(m_accounts->balance(a.id).value().minorUnits(), qint64(-200));
}

void tst_Database::settingsPersist()
{
    {
        SettingsRepository repo(*m_db);
        SettingsManager settings(repo);
        QVERIFY(settings.load().isOk());
        QCOMPARE(settings.language(), QString("en")); // product default
        QCOMPARE(settings.theme(), QString("system"));
        QCOMPARE(settings.defaultCurrency(), QString("AMD"));
        QCOMPARE(settings.localeName(), QString("en_US"));

        QSignalSpy darkSpy(&settings, &SettingsManager::effectiveDarkChanged);
        settings.setTheme("dark");
        QVERIFY(settings.effectiveDark());
        QCOMPARE(darkSpy.count(), 1);
        settings.setTheme("bogus"); // ignored
        QCOMPARE(settings.theme(), QString("dark"));
        settings.setTheme("system");
        settings.setSystemDark(true);
        QVERIFY(settings.effectiveDark());
        settings.setSystemDark(false);
        QVERIFY(!settings.effectiveDark());

        settings.setLanguage("hy");
        QCOMPARE(settings.localeName(), QString("hy_AM"));
        settings.setLanguage("xx"); // ignored
        QCOMPARE(settings.language(), QString("hy"));
        settings.setDefaultCurrency("usd");
        QCOMPARE(settings.defaultCurrency(), QString("USD"));
        settings.setDefaultCurrency("???"); // ignored
        QCOMPARE(settings.defaultCurrency(), QString("USD"));
        settings.setTheme("light");
    }
    SettingsRepository repo(*m_db);
    SettingsManager reloaded(repo);
    QVERIFY(reloaded.load().isOk());
    QCOMPARE(reloaded.language(), QString("hy"));
    QCOMPARE(reloaded.defaultCurrency(), QString("USD"));
    QCOMPARE(reloaded.theme(), QString("light"));
}

QTEST_GUILESS_MAIN(tst_Database)
#include "tst_Database.moc"
