#pragma once

#include "core/SettingsManager.h"
#include "database/AccountRepository.h"
#include "database/CategoryRepository.h"
#include "database/TransactionRepository.h"

#include <QAbstractListModel>
#include <QHash>

namespace pf {

/// Paged, filterable list of transactions for QML (newest first).
/// Pages of `pageSize` rows are fetched lazily through canFetchMore/fetchMore,
/// so a ListView over thousands of transactions stays responsive.
class TransactionListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY filterChanged)
    /// 0 = all accounts
    Q_PROPERTY(int accountFilter READ accountFilter WRITE setAccountFilter NOTIFY filterChanged)
    /// 0 = all categories
    Q_PROPERTY(int categoryFilter READ categoryFilter WRITE setCategoryFilter NOTIFY filterChanged)
    /// "" = all, "income", "expense"
    Q_PROPERTY(QString typeFilter READ typeFilter WRITE setTypeFilter NOTIFY filterChanged)
    Q_PROPERTY(int pageSize READ pageSize WRITE setPageSize NOTIFY pageSizeChanged)
    /// 0 = unlimited. Dashboard "recent" lists set e.g. 5 and never fetch more.
    Q_PROPERTY(int maxRows READ maxRows WRITE setMaxRows NOTIFY maxRowsChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    /// Number of rows matching the filter (not just those loaded).
    Q_PROPERTY(int totalCount READ totalCount NOTIFY countChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        AccountIdRole,
        AccountNameRole,
        CategoryIdRole,
        CategoryNameRole,
        CategoryIconRole,
        CategoryColorRole,
        TypeRole,
        IsIncomeRole,
        AmountTextRole, // unsigned, formatted
        SignedAmountTextRole, // "+..." / "-..."
        CurrencyRole,
        MerchantRole,
        DescriptionRole,
        TitleRole,
        SubtitleRole,
        OccurredAtRole,
        TimeTextRole,
        DayKeyRole, // yyyy-MM-dd (local), for ListView sections
    };

    TransactionListModel(TransactionRepository &transactions, AccountRepository &accounts,
                         CategoryRepository &categories, SettingsManager &settings, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    bool canFetchMore(const QModelIndex &parent) const override;
    void fetchMore(const QModelIndex &parent) override;

    QString searchText() const { return m_searchText; }
    int accountFilter() const { return m_accountFilter; }
    int categoryFilter() const { return m_categoryFilter; }
    QString typeFilter() const { return m_typeFilter; }
    int pageSize() const { return m_pageSize; }
    int maxRows() const { return m_maxRows; }
    int count() const { return int(m_rows.size()); }
    int totalCount() const { return m_total; }

    void setSearchText(const QString &text);
    void setAccountFilter(int id);
    void setCategoryFilter(int id);
    void setTypeFilter(const QString &type);
    void setPageSize(int size);
    void setMaxRows(int rows);

    /// Re-runs the query from the first page. Several triggers in the same
    /// event-loop turn are coalesced into one query.
    Q_INVOKABLE void reload();
    /// All fields of a row in a form the edit sheet can use directly.
    Q_INVOKABLE QVariantMap get(int row) const;

signals:
    void filterChanged();
    void pageSizeChanged();
    void maxRowsChanged();
    void countChanged();

private:
    void scheduleReload();
    void refreshLookups();
    TransactionFilter currentFilter() const;
    int effectiveTotal() const;

    TransactionRepository &m_transactions;
    AccountRepository &m_accounts;
    CategoryRepository &m_categories;
    SettingsManager &m_settings;

    QString m_searchText;
    int m_accountFilter = 0;
    int m_categoryFilter = 0;
    QString m_typeFilter;
    int m_pageSize = 40;
    int m_maxRows = 0;

    QList<Transaction> m_rows;
    int m_total = 0;
    bool m_reloadScheduled = false;

    QHash<qint64, Account> m_accountLookup;
    QHash<qint64, Category> m_categoryLookup;
    Category m_otherCategory; // shown for uncategorized transactions
};

} // namespace pf
