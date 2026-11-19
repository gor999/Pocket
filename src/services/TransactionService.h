#pragma once

#include "core/Result.h"
#include "database/AccountRepository.h"
#include "database/TransactionRepository.h"
#include "models/Transaction.h"

#include <QList>
#include <QObject>
#include <QVariantMap>

namespace pf {

/// Outcome of TransactionService::add.
struct AddOutcome
{
    bool saved = false;
    Transaction transaction; // valid when saved
    QList<Transaction> duplicates; // non-empty => not saved, awaiting user confirmation
    QString error;
};

/// Business rules for transactions: validation, duplicate detection and
/// change notification. Persistence is delegated to TransactionRepository.
class TransactionService : public QObject
{
    Q_OBJECT

public:
    /// Same account + type + amount within this window counts as a possible duplicate.
    static constexpr int kDuplicateWindowSeconds = 10 * 60;

    TransactionService(TransactionRepository &transactions, AccountRepository &accounts, QObject *parent = nullptr);

    /// Validates against the owning account (exists, same currency, positive
    /// amount, sane date).
    Status validate(const Transaction &t) const;

    /// Saves the transaction unless likely duplicates exist and `allowDuplicate`
    /// is false, in which case nothing is saved and `duplicates` is filled in.
    AddOutcome add(Transaction t, bool allowDuplicate = false);
    Status update(const Transaction &t);
    Status remove(qint64 id);

    // --- QML facade ------------------------------------------------------
    /// fields: id (0 = new), accountId, categoryId (0 = none), type ("income"|"expense"),
    /// amount (decimal text), occurredAt (Date), merchant, description, force (bool).
    /// Returns {ok, error, id, duplicate}.
    Q_INVOKABLE QVariantMap save(const QVariantMap &fields);
    Q_INVOKABLE QVariantMap deleteTransaction(qint64 transactionId);

signals:
    void changed();

private:
    TransactionRepository &m_transactions;
    AccountRepository &m_accounts;
};

} // namespace pf
