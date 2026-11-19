#include "services/AccountService.h"

#include "core/Currency.h"

namespace pf {

namespace {

QVariantMap ok(qint64 id = 0)
{
    return {{QStringLiteral("ok"), true}, {QStringLiteral("id"), qlonglong(id)}};
}

QVariantMap fail(const QString &message)
{
    return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), message}};
}

bool allDigits(const QString &s)
{
    for (const QChar c : s) {
        if (c < QLatin1Char('0') || c > QLatin1Char('9'))
            return false;
    }
    return true;
}

} // namespace

AccountService::AccountService(AccountRepository &repository, SettingsManager &settings, QObject *parent)
    : QObject(parent)
    , m_repository(repository)
    , m_settings(settings)
{
}

Status AccountService::validate(const Account &a)
{
    if (a.bankName.trimmed().isEmpty())
        return Status::error(tr("Enter the bank name."));
    if (a.name.trimmed().isEmpty())
        return Status::error(tr("Enter an account name."));
    if (!a.initialBalance.isValid() || !Currency::isSupported(a.currency()))
        return Status::error(tr("Choose a supported currency."));
    if (!a.lastFour.isEmpty() && (a.lastFour.size() != 4 || !allDigits(a.lastFour))) {
        // Deliberately strict: a longer digit string might be a full card number.
        return Status::error(tr("Enter only the last four digits of the card. Full card numbers are never stored."));
    }
    return Status::ok();
}

Result<Account> AccountService::create(Account account)
{
    if (const Status v = validate(account); !v)
        return v;
    const auto inserted = m_repository.insert(account);
    if (inserted)
        emit changed();
    return inserted;
}

Status AccountService::update(const Account &account)
{
    if (const Status v = validate(account); !v)
        return v;

    const auto existing = m_repository.byId(account.id);
    if (!existing)
        return existing.status();
    if (existing.value().currency() != account.currency()) {
        const auto count = m_repository.transactionCount(account.id);
        if (!count)
            return count.status();
        if (count.value() > 0)
            return Status::error(tr("The currency cannot be changed because this account already has transactions."));
    }

    const Status s = m_repository.update(account);
    if (s)
        emit changed();
    return s;
}

Status AccountService::remove(qint64 id)
{
    const Status s = m_repository.remove(id);
    if (s)
        emit changed();
    return s;
}

Status AccountService::setArchived(qint64 id, bool archived)
{
    const Status s = m_repository.setArchived(id, archived);
    if (s)
        emit changed();
    return s;
}

QVariantMap AccountService::save(const QVariantMap &f)
{
    const QString currency = Currency::normalize(f.value(QStringLiteral("currency")).toString());
    const QString balanceText = f.value(QStringLiteral("initialBalance")).toString().trimmed();

    Money initial = Money::zero(currency);
    if (!balanceText.isEmpty()) {
        const auto parsed = Money::fromDecimalString(balanceText, currency);
        if (!parsed)
            return fail(tr("The initial balance is not a valid amount."));
        initial = *parsed;
    }

    Account a;
    a.id = f.value(QStringLiteral("id")).toLongLong();
    a.bankName = f.value(QStringLiteral("bankName")).toString().trimmed();
    a.name = f.value(QStringLiteral("name")).toString().trimmed();
    a.cardNetwork = cardNetworkFromKey(f.value(QStringLiteral("cardNetwork")).toString());
    a.lastFour = f.value(QStringLiteral("lastFour")).toString().trimmed();
    a.initialBalance = initial;

    if (a.id == 0) {
        const auto created = create(a);
        return created ? ok(created.value().id) : fail(created.error());
    }
    // Preserve archive flag when editing.
    if (const auto existing = m_repository.byId(a.id); existing)
        a.archived = existing.value().archived;
    const Status s = update(a);
    return s ? ok(a.id) : fail(s.message());
}

QVariantMap AccountService::deleteAccount(qint64 accountId)
{
    const Status s = remove(accountId);
    return s ? ok(accountId) : fail(s.message());
}

QVariantMap AccountService::archiveAccount(qint64 accountId, bool archived)
{
    const Status s = setArchived(accountId, archived);
    return s ? ok(accountId) : fail(s.message());
}

QVariantList AccountService::totalsByCurrency() const
{
    QVariantList list;
    const auto totals = m_repository.totalsByCurrency(false);
    if (!totals)
        return list;
    const QLocale locale = m_settings.locale();
    for (const Money &m : totals.value()) {
        list << QVariantMap{
            {QStringLiteral("currency"), m.currency()},
            {QStringLiteral("minor"), qlonglong(m.minorUnits())},
            {QStringLiteral("text"), m.format(locale)},
        };
    }
    return list;
}

QVariantList AccountService::cardNetworks() const
{
    QVariantList list;
    for (const CardNetwork n : {CardNetwork::None, CardNetwork::Visa, CardNetwork::Mastercard, CardNetwork::Arca,
                                CardNetwork::AmericanExpress, CardNetwork::Mir, CardNetwork::UnionPay,
                                CardNetwork::Other}) {
        list << QVariantMap{{QStringLiteral("value"), toKey(n)}, {QStringLiteral("label"), displayName(n)}};
    }
    return list;
}

QStringList AccountService::suggestedBanks() const
{
    // Quick-pick suggestions only. The bank name is free text, so any bank
    // (Armenian or international) can be entered.
    return {QStringLiteral("Ameriabank"), QStringLiteral("ACBA Bank"), QStringLiteral("Ardshinbank"),
            QStringLiteral("Inecobank"),  QStringLiteral("IDBank"),    QStringLiteral("Converse Bank"),
            QStringLiteral("Evocabank"),  QStringLiteral("Unibank"),   QStringLiteral("Fast Bank"),
            QStringLiteral("Revolut"),    QStringLiteral("Wise"),      QStringLiteral("Cash")};
}

} // namespace pf
