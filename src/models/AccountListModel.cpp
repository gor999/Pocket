#include "models/AccountListModel.h"

namespace pf {

AccountListModel::AccountListModel(AccountRepository &accounts, SettingsManager &settings, QObject *parent)
    : QAbstractListModel(parent)
    , m_accounts(accounts)
    , m_settings(settings)
{
    // Balances are formatted with the locale, so re-read on language change.
    connect(&m_settings, &SettingsManager::languageChanged, this, &AccountListModel::reload);
}

int AccountListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QHash<int, QByteArray> AccountListModel::roleNames() const
{
    return {
        {IdRole, "accountId"},
        {BankNameRole, "bankName"},
        {NameRole, "name"},
        {TitleRole, "title"},
        {CurrencyRole, "currency"},
        {CardNetworkRole, "cardNetwork"},
        {CardNetworkLabelRole, "cardNetworkLabel"},
        {LastFourRole, "lastFour"},
        {BalanceTextRole, "balanceText"},
        {BalanceMinorRole, "balanceMinor"},
        {IsNegativeRole, "isNegative"},
        {InitialBalanceRole, "initialBalance"},
        {TransactionCountRole, "transactionCount"},
    };
}

QVariant AccountListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const AccountSummary &s = m_rows.at(index.row());
    const Account &a = s.account;
    switch (role) {
    case IdRole: return qlonglong(a.id);
    case BankNameRole: return a.bankName;
    case NameRole: return a.name;
    case TitleRole:
        return a.lastFour.isEmpty() ? a.name : QStringLiteral("%1 ••%2").arg(a.name, a.lastFour);
    case CurrencyRole: return a.currency();
    case CardNetworkRole: return toKey(a.cardNetwork);
    case CardNetworkLabelRole: return a.cardNetwork == CardNetwork::None ? QString() : displayName(a.cardNetwork);
    case LastFourRole: return a.lastFour;
    case BalanceTextRole: return s.balance.format(m_settings.locale());
    case BalanceMinorRole: return qlonglong(s.balance.minorUnits());
    case IsNegativeRole: return s.balance.isNegative();
    case InitialBalanceRole: return a.initialBalance.toDecimalString();
    case TransactionCountRole: return s.transactionCount;
    default: return {};
    }
}

void AccountListModel::reload()
{
    const int before = count();
    const auto list = m_accounts.summaries(false);
    beginResetModel();
    m_rows = list ? list.value() : QList<AccountSummary>{};
    endResetModel();
    if (before != count())
        emit countChanged();
}

QVariantMap AccountListModel::get(int row) const
{
    QVariantMap map;
    if (row < 0 || row >= m_rows.size())
        return map;
    const QHash<int, QByteArray> names = roleNames();
    for (auto it = names.cbegin(); it != names.cend(); ++it)
        map.insert(QString::fromLatin1(it.value()), data(index(row, 0), it.key()));
    return map;
}

int AccountListModel::indexOfId(qint64 id) const
{
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).account.id == id)
            return i;
    }
    return -1;
}

} // namespace pf
