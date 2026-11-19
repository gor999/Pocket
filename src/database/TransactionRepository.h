#pragma once

#include "core/Result.h"
#include "database/DatabaseManager.h"
#include "models/Transaction.h"

#include <QDateTime>
#include <QList>

#include <optional>

namespace pf {

struct TransactionFilter
{
    QString text; // whitespace-separated words, ALL must match merchant/description
    std::optional<qint64> accountId;
    std::optional<qint64> categoryId;
    bool uncategorizedOnly = false;
    std::optional<TransactionType> type;
    std::optional<QDateTime> from; // inclusive
    std::optional<QDateTime> to; // exclusive
};

/// Income and expense totals for one currency over a period.
struct CurrencyTotals
{
    QString currency;
    Money income;
    Money expense;
    Money net() const { return income - expense; }
};

class TransactionRepository
{
public:
    explicit TransactionRepository(const DatabaseManager &db) : m_db(db) {}

    /// Inserts and returns the stored transaction (id and timestamps filled in).
    Result<Transaction> insert(Transaction t) const;
    Status update(const Transaction &t) const;
    Status remove(qint64 id) const;

    Result<Transaction> byId(qint64 id) const;
    /// Newest first (occurred_at DESC, id DESC).
    Result<QList<Transaction>> query(const TransactionFilter &filter, int limit, int offset) const;
    Result<int> count(const TransactionFilter &filter) const;

    /// Income/expense sums per currency in [from, to).
    Result<QList<CurrencyTotals>> totals(const QDateTime &from, const QDateTime &to,
                                         std::optional<qint64> accountId = std::nullopt) const;

    /// Transactions on the same account with the same type and amount within
    /// +/- `windowSeconds` of `occurredAt` (excluding `excludeId`).
    Result<QList<Transaction>> findPotentialDuplicates(const Transaction &candidate, int windowSeconds,
                                                       qint64 excludeId = 0) const;

private:
    const DatabaseManager &m_db;
};

} // namespace pf
