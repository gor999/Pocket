#pragma once

#include "core/SettingsManager.h"
#include "database/AccountRepository.h"

#include <QAbstractListModel>

namespace pf {

/// Active accounts with their live balances, for QML lists and pickers.
class AccountListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        BankNameRole,
        NameRole,
        TitleRole, // "Name ••1234"
        CurrencyRole,
        CardNetworkRole, // stable key, "" if none
        CardNetworkLabelRole,
        LastFourRole,
        BalanceTextRole,
        BalanceMinorRole,
        IsNegativeRole,
        InitialBalanceRole, // decimal text, for editing
        TransactionCountRole,
    };

    AccountListModel(AccountRepository &accounts, SettingsManager &settings, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return int(m_rows.size()); }

    Q_INVOKABLE void reload();
    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE int indexOfId(qint64 id) const;

signals:
    void countChanged();

private:
    AccountRepository &m_accounts;
    SettingsManager &m_settings;
    QList<AccountSummary> m_rows;
};

} // namespace pf
