#include "core/DemoData.h"

#include <QRandomGenerator>
#include <QStringList>

namespace pf {

namespace {

struct Merchant
{
    const char *category;
    const char *name;
    int minAmd; // in whole AMD
    int maxAmd;
};

// clang-format off
const Merchant kMerchants[] = {
    {"groceries",      "Carrefour",                     4000, 28000},
    {"groceries",      "SAS Supermarket",               2500, 18000},
    {"groceries",      "Yerevan City",                  3000, 22000},
    {"groceries",      "Магазин «Пятёрочка»",           1500, 9000},
    {"restaurants",    "Glovo",                         3500, 12000},
    {"restaurants",    "Cafe Central",                  2500, 9500},
    {"restaurants",    "Ռեստորան «Նոր Դար»",            6000, 24000},
    {"restaurants",    "Starbucks",                     1800, 3200},
    {"transportation", "Yandex Go",                     900,  4500},
    {"transportation", "Gazprom Neft Fuel",             8000, 20000},
    {"transportation", "Yerevan Metro",                 100,  300},
    {"shopping",       "Zara",                          12000, 65000},
    {"shopping",       "Wildberries",                   3000, 25000},
    {"shopping",       "Sil Plaza",                     5000, 30000},
    {"healthcare",     "Aleksanyan Pharmacy",           1500, 12000},
    {"healthcare",     "Izmirlian Medical Center",      15000, 40000},
    {"utilities",      "Electric Networks of Armenia",  6000, 14000},
    {"utilities",      "Veolia Jur",                    2500, 5000},
    {"utilities",      "Team Telecom",                  3500, 7000},
    {"entertainment",  "Netflix",                       3200, 4800},
    {"entertainment",  "Cinema Star",                   2000, 5000},
    {"entertainment",  "Spotify",                       1600, 1600},
};
// clang-format on

} // namespace

bool DemoData::seed(ApplicationManager &app)
{
    if (app.accountModel()->count() > 0)
        return false; // never touch a database that already has data

    CategoryRepository &categories = app.categoryRepository();
    const auto categoryId = [&](const char *key) -> qint64 {
        const auto c = categories.byKey(QString::fromLatin1(key));
        return c ? c.value().id : 0;
    };

    const auto makeAccount = [&](const char *bank, const char *name, const char *currency, CardNetwork network,
                                 const char *last4, qint64 initialMinor) -> qint64 {
        Account a;
        a.bankName = QString::fromUtf8(bank);
        a.name = QString::fromUtf8(name);
        a.cardNetwork = network;
        a.lastFour = QString::fromLatin1(last4);
        a.initialBalance = Money(initialMinor, QString::fromLatin1(currency));
        const auto created = app.accountService()->create(a);
        return created ? created.value().id : 0;
    };

    const qint64 amer = makeAccount("Ameriabank", "Everyday", "AMD", CardNetwork::Visa, "4417", 35000000);
    const qint64 acba = makeAccount("ACBA Bank", "Salary", "AMD", CardNetwork::Arca, "9082", 120000000);
    const qint64 revolut = makeAccount("Revolut", "Travel", "USD", CardNetwork::Mastercard, "2290", 85000);
    const qint64 cash = makeAccount("Cash", "Wallet", "AMD", CardNetwork::None, "", 2500000);
    if (!amer || !acba || !revolut || !cash)
        return false;

    QRandomGenerator rng(20261118);
    const QDateTime now = QDateTime::currentDateTime();
    const auto add = [&](qint64 accountId, TransactionType type, qint64 minor, const QString &currency,
                         const QDateTime &when, const QString &merchant, qint64 category,
                         const QString &description = QString()) {
        Transaction t;
        t.accountId = accountId;
        t.type = type;
        t.amount = Money(minor, currency);
        t.occurredAt = when;
        t.merchant = merchant;
        t.description = description;
        if (category > 0)
            t.categoryId = category;
        app.transactionService()->add(t, /*allowDuplicate=*/true);
    };

    // Salaries: the 1st of this month and last month.
    for (int monthsBack = 0; monthsBack < 2; ++monthsBack) {
        const QDate first = QDate(now.date().year(), now.date().month(), 1).addMonths(-monthsBack);
        add(acba, TransactionType::Income, 45000000, QStringLiteral("AMD"), QDateTime(first, QTime(6, 30)),
            QStringLiteral("Employer LLC"), categoryId("salary"), QStringLiteral("Monthly salary"));
    }

    const int merchantCount = int(sizeof(kMerchants) / sizeof(kMerchants[0]));
    for (int daysBack = 0; daysBack < 75; ++daysBack) {
        // More activity in the last week so the dashboard always has content.
        const int perDay = daysBack < 7 ? 2 + int(rng.bounded(2)) : int(rng.bounded(3));
        for (int n = 0; n < perDay; ++n) {
            const Merchant &m = kMerchants[rng.bounded(merchantCount)];
            const qint64 amd = qint64(m.minAmd) + qint64(rng.bounded(quint32(m.maxAmd - m.minAmd + 1)));
            QDateTime when(now.date().addDays(-daysBack), QTime(8 + int(rng.bounded(14)), int(rng.bounded(60))));
            if (when > now)
                when = now.addSecs(-60 * (n + 1));
            const qint64 account = rng.bounded(5) == 0 ? cash : (rng.bounded(3) == 0 ? acba : amer);
            add(account, TransactionType::Expense, amd * 100, QStringLiteral("AMD"), when,
                QString::fromUtf8(m.name), categoryId(m.category));
        }
    }

    // A few dollar expenses on the travel card.
    add(revolut, TransactionType::Expense, 1299, QStringLiteral("USD"), now.addDays(-3), QStringLiteral("Spotify Premium"),
        categoryId("entertainment"));
    add(revolut, TransactionType::Expense, 4650, QStringLiteral("USD"), now.addDays(-9), QStringLiteral("Booking.com"),
        categoryId("other"));
    add(revolut, TransactionType::Income, 20000, QStringLiteral("USD"), now.addDays(-12), QStringLiteral("Freelance payment"),
        categoryId("transfers"));
    return true;
}

void DemoData::seedSms(ApplicationManager &app)
{
    const QDateTime now = QDateTime::currentDateTime();
    const auto stamp = [&](int hoursAgo) { return now.addSecs(-hoursAgo * 3600).toString(QStringLiteral("dd.MM.yyyy HH:mm")); };
    const auto send = [&](const QString &sender, const QString &body, int hoursAgo, const QString &bank = QString()) {
        sms::IncomingSms m;
        m.sender = sender;
        m.body = body;
        m.receivedAt = now.addSecs(-hoursAgo * 3600);
        m.channel = sms::channel::ShortcutIntent;
        m.bankHint = bank;
        app.smsImportService()->process(m);
    };

    // All of these are invented for the demo; see tests/sms_samples/README.md.
    send(QStringLiteral("PF-TEST"), QStringLiteral("PF-TEST connection check"), 30);
    send(QStringLiteral("IDBank"),
         QStringLiteral("IDBank: Purchase 12,500.00 AMD, SAS SUPERMARKET YEREVAN, Card *1234, %1, Balance: 245,000.00 AMD").arg(stamp(26)), 26);
    const QString carrefour = QStringLiteral("Ameriabank\nPurchase: 18,400.00 AMD\nCARREFOUR\nCard *4417 %1\nBalance: 1,045,000.00 AMD").arg(stamp(20));
    send(QStringLiteral("Ameriabank"), carrefour, 20);
    send(QStringLiteral("Ameriabank"), carrefour, 20); // the same SMS delivered twice
    send(QStringLiteral("ACBA"),
         QStringLiteral("ACBA: Card *9082 Salary 300,000.00 AMD %1").arg(stamp(15)), 15);
    send(QStringLiteral("ACBA"),
         QStringLiteral("ACBA: Card *9082 Purchase 6,700.00 AMD %1 ZZ TRADING 77 Balance: 913,300.00 AMD").arg(stamp(9)), 9);
    send(QStringLiteral("IDBank"),
         QStringLiteral("IDBank: \u0533\u0576\u0578\u0582\u0574 8,900.00 AMD, \u053F\u0531\u0556\u0535 \u0531\u0550\u0531\u0544, \u0554\u0561\u0580\u057F *1234, %1, \u0544\u0576\u0561\u0581\u0578\u0572: 232,500.00 AMD").arg(stamp(5)), 5);
    send(QStringLiteral("IDBank"), QStringLiteral("IDBank: Your OTP code is 483920. Do not share it."), 4);
    send(QStringLiteral("UniBankX"), QStringLiteral("UniBankX: purchase 1,000 AMD at SOME SHOP"), 2);
}

} // namespace pf

