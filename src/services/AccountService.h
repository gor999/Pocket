#pragma once

#include "core/Result.h"
#include "core/SettingsManager.h"
#include "database/AccountRepository.h"
#include "models/Account.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace pf {

/// Business rules for accounts. The typed C++ API (validate/create/update/...)
/// is used by tests and other services; the QVariantMap methods are the thin
/// facade QML calls.
class AccountService : public QObject
{
    Q_OBJECT

public:
    AccountService(AccountRepository &repository, SettingsManager &settings, QObject *parent = nullptr);

    /// Rules: names present, supported currency, last four = exactly 4 digits
    /// or empty (full card numbers are rejected).
    static Status validate(const Account &account);

    Result<Account> create(Account account);
    Status update(const Account &account);
    Status remove(qint64 id);
    Status setArchived(qint64 id, bool archived);

    // --- QML facade ------------------------------------------------------
    /// fields: id (0 = new), bankName, name, currency, cardNetwork, lastFour, initialBalance (text)
    /// Returns {ok, error, id}.
    Q_INVOKABLE QVariantMap save(const QVariantMap &fields);
    Q_INVOKABLE QVariantMap deleteAccount(qint64 accountId);
    Q_INVOKABLE QVariantMap archiveAccount(qint64 accountId, bool archived);
    /// Per-currency totals of all active accounts: [{currency, text, minor}]
    Q_INVOKABLE QVariantList totalsByCurrency() const;
    Q_INVOKABLE QVariantList cardNetworks() const; // [{value, label}]
    Q_INVOKABLE QStringList suggestedBanks() const;

signals:
    /// Emitted after any successful change to accounts (or their balances).
    void changed();

private:
    AccountRepository &m_repository;
    SettingsManager &m_settings;
};

} // namespace pf
