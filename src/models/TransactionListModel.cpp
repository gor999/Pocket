#include "models/TransactionListModel.h"

#include <QCoreApplication>
#include <QMetaObject>

#include <algorithm>

namespace pf {

TransactionListModel::TransactionListModel(TransactionRepository &transactions, AccountRepository &accounts,
                                           CategoryRepository &categories, SettingsManager &settings,
                                           QObject *parent)
    : QAbstractListModel(parent)
    , m_transactions(transactions)
    , m_accounts(accounts)
    , m_categories(categories)
    , m_settings(settings)
{
    connect(&m_settings, &SettingsManager::languageChanged, this, &TransactionListModel::scheduleReload);
}

int TransactionListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QHash<int, QByteArray> TransactionListModel::roleNames() const
{
    return {
        {IdRole, "transactionId"},
        {AccountIdRole, "accountId"},
        {AccountNameRole, "accountName"},
        {CategoryIdRole, "categoryId"},
        {CategoryNameRole, "categoryName"},
        {CategoryIconRole, "categoryIcon"},
        {CategoryColorRole, "categoryColor"},
        {TypeRole, "type"},
        {IsIncomeRole, "isIncome"},
        {AmountTextRole, "amountText"},
        {SignedAmountTextRole, "signedAmountText"},
        {CurrencyRole, "currency"},
        {MerchantRole, "merchant"},
        {DescriptionRole, "description"},
        {TitleRole, "title"},
        {SubtitleRole, "subtitle"},
        {OccurredAtRole, "occurredAt"},
        {TimeTextRole, "timeText"},
        {DayKeyRole, "dayKey"},
    };
}

QVariant TransactionListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const Transaction &t = m_rows.at(index.row());

    const Category &category = t.categoryId && m_categoryLookup.contains(*t.categoryId)
        ? m_categoryLookup[*t.categoryId]
        : m_otherCategory;
    const QString accountName = m_accountLookup.contains(t.accountId) ? m_accountLookup[t.accountId].name : QString();
    const QLocale locale = m_settings.locale();
    const bool income = t.type == TransactionType::Income;

    switch (role) {
    case IdRole: return qlonglong(t.id);
    case AccountIdRole: return qlonglong(t.accountId);
    case AccountNameRole: return accountName;
    case CategoryIdRole: return qlonglong(t.categoryId.value_or(0));
    case CategoryNameRole: return category.displayName();
    case CategoryIconRole: return category.icon;
    case CategoryColorRole: return category.color;
    case TypeRole: return toKey(t.type);
    case IsIncomeRole: return income;
    case AmountTextRole: return t.amount.format(locale);
    case SignedAmountTextRole:
        return income ? QStringLiteral("+") + t.amount.format(locale) : t.signedAmount().format(locale);
    case CurrencyRole: return t.amount.currency();
    case MerchantRole: return t.merchant;
    case DescriptionRole: return t.description;
    case TitleRole:
        if (!t.merchant.isEmpty())
            return t.merchant;
        if (!t.description.isEmpty())
            return t.description;
        return category.displayName();
    case SubtitleRole: {
        const QString catName = category.displayName();
        return accountName.isEmpty() ? catName : catName + QStringLiteral(" · ") + accountName;
    }
    case OccurredAtRole: return t.occurredAt;
    case TimeTextRole: return locale.toString(t.occurredAt.time(), QLocale::ShortFormat);
    case DayKeyRole: return t.occurredAt.date().toString(Qt::ISODate);
    default: return {};
    }
}

TransactionFilter TransactionListModel::currentFilter() const
{
    TransactionFilter f;
    f.text = m_searchText;
    if (m_accountFilter > 0)
        f.accountId = m_accountFilter;
    if (m_categoryFilter > 0)
        f.categoryId = m_categoryFilter;
    if (!m_typeFilter.isEmpty()) {
        bool ok = false;
        const TransactionType type = transactionTypeFromKey(m_typeFilter, &ok);
        if (ok)
            f.type = type;
    }
    return f;
}

int TransactionListModel::effectiveTotal() const
{
    return m_maxRows > 0 ? std::min(m_total, m_maxRows) : m_total;
}

void TransactionListModel::refreshLookups()
{
    m_accountLookup.clear();
    if (const auto accounts = m_accounts.summaries(true)) {
        for (const AccountSummary &s : accounts.value())
            m_accountLookup.insert(s.account.id, s.account);
    }
    m_categoryLookup.clear();
    if (const auto categories = m_categories.all()) {
        for (const Category &c : categories.value()) {
            m_categoryLookup.insert(c.id, c);
            if (c.key == QLatin1String("other"))
                m_otherCategory = c;
        }
    }
}

bool TransactionListModel::canFetchMore(const QModelIndex &parent) const
{
    return !parent.isValid() && m_rows.size() < effectiveTotal();
}

void TransactionListModel::fetchMore(const QModelIndex &parent)
{
    if (parent.isValid() || !canFetchMore(parent))
        return;
    const int remaining = effectiveTotal() - int(m_rows.size());
    const int limit = std::min(m_pageSize, remaining);
    const auto page = m_transactions.query(currentFilter(), limit, int(m_rows.size()));
    if (!page || page.value().isEmpty())
        return;
    const int first = int(m_rows.size());
    beginInsertRows({}, first, first + int(page.value().size()) - 1);
    m_rows += page.value();
    endInsertRows();
    emit countChanged();
}

void TransactionListModel::reload()
{
    m_reloadScheduled = false;
    refreshLookups();

    const TransactionFilter filter = currentFilter();
    const auto total = m_transactions.count(filter);
    m_total = total ? total.value() : 0;

    const int limit = m_maxRows > 0 ? std::min(m_pageSize, m_maxRows) : m_pageSize;
    const auto page = m_transactions.query(filter, limit, 0);

    beginResetModel();
    m_rows = page ? page.value() : QList<Transaction>{};
    endResetModel();
    emit countChanged();
}

void TransactionListModel::scheduleReload()
{
    if (m_reloadScheduled)
        return;
    m_reloadScheduled = true;
    QMetaObject::invokeMethod(this, &TransactionListModel::reload, Qt::QueuedConnection);
}

QVariantMap TransactionListModel::get(int row) const
{
    QVariantMap map;
    if (row < 0 || row >= m_rows.size())
        return map;
    const Transaction &t = m_rows.at(row);
    map.insert(QStringLiteral("id"), qlonglong(t.id));
    map.insert(QStringLiteral("accountId"), qlonglong(t.accountId));
    map.insert(QStringLiteral("categoryId"), qlonglong(t.categoryId.value_or(0)));
    map.insert(QStringLiteral("type"), toKey(t.type));
    map.insert(QStringLiteral("amount"), t.amount.toDecimalString());
    map.insert(QStringLiteral("currency"), t.amount.currency());
    map.insert(QStringLiteral("merchant"), t.merchant);
    map.insert(QStringLiteral("description"), t.description);
    map.insert(QStringLiteral("occurredAt"), t.occurredAt);
    return map;
}

void TransactionListModel::setSearchText(const QString &text)
{
    if (text == m_searchText)
        return;
    m_searchText = text;
    emit filterChanged();
    scheduleReload();
}

void TransactionListModel::setAccountFilter(int id)
{
    if (id == m_accountFilter)
        return;
    m_accountFilter = id;
    emit filterChanged();
    scheduleReload();
}

void TransactionListModel::setCategoryFilter(int id)
{
    if (id == m_categoryFilter)
        return;
    m_categoryFilter = id;
    emit filterChanged();
    scheduleReload();
}

void TransactionListModel::setTypeFilter(const QString &requested)
{
    // Only "income" / "expense" are filters; anything else (including "" and
    // "all") means no filter, so the stored value always matches what is applied.
    QString type;
    if (!requested.isEmpty()) {
        bool ok = false;
        transactionTypeFromKey(requested, &ok);
        if (ok)
            type = requested;
    }
    if (type == m_typeFilter)
        return;
    m_typeFilter = type;
    emit filterChanged();
    scheduleReload();
}

void TransactionListModel::setPageSize(int size)
{
    size = std::max(1, size);
    if (size == m_pageSize)
        return;
    m_pageSize = size;
    emit pageSizeChanged();
    scheduleReload();
}

void TransactionListModel::setMaxRows(int rows)
{
    rows = std::max(0, rows);
    if (rows == m_maxRows)
        return;
    m_maxRows = rows;
    emit maxRowsChanged();
    scheduleReload();
}

} // namespace pf
