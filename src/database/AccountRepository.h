#pragma once

#include "core/Result.h"
#include "database/DatabaseManager.h"
#include "models/Account.h"

#include <QList>

namespace pf {

/// SQLite persistence for accounts. All balance math happens in SQL on integer
/// minor units, so results are exact.
class AccountRepository
{
public:
    explicit AccountRepository(const DatabaseManager &db) : m_db(db) {}

    Result<Account> insert(Account account) const;
    Status update(const Account &account) const;
    /// Deletes the account and (ON DELETE CASCADE) all of its transactions.
    Status remove(qint64 id) const;
    Status setArchived(qint64 id, bool archived) const;

    Result<Account> byId(qint64 id) const;
    Result<QList<AccountSummary>> summaries(bool includeArchived = false) const;
    Result<Money> balance(qint64 id) const;
    /// Sum of current balances per currency (never mixes currencies).
    Result<QList<Money>> totalsByCurrency(bool includeArchived = false) const;
    Result<int> transactionCount(qint64 accountId) const;

private:
    const DatabaseManager &m_db;
};

} // namespace pf
