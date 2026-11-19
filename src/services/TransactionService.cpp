#include "services/TransactionService.h"

#include "core/Currency.h"

namespace pf {

TransactionService::TransactionService(TransactionRepository &transactions, AccountRepository &accounts,
                                       QObject *parent)
    : QObject(parent)
    , m_transactions(transactions)
    , m_accounts(accounts)
{
}

Status TransactionService::validate(const Transaction &t) const
{
    if (!t.amount.isValid() || !t.amount.isPositive())
        return Status::error(tr("The amount must be greater than zero."));
    if (!t.occurredAt.isValid())
        return Status::error(tr("Choose a valid date."));
    if (t.occurredAt.date().year() < 2000 || t.occurredAt > QDateTime::currentDateTime().addDays(1))
        return Status::error(tr("The date is out of range."));

    const auto account = m_accounts.byId(t.accountId);
    if (!account)
        return Status::error(tr("Choose an account."));
    if (account.value().currency() != t.amount.currency()) {
        return Status::error(tr("The transaction currency (%1) must match the account currency (%2).")
                                 .arg(t.amount.currency(), account.value().currency()));
    }
    return Status::ok();
}

AddOutcome TransactionService::add(Transaction t, bool allowDuplicate)
{
    AddOutcome out;
    if (const Status v = validate(t); !v) {
        out.error = v.message();
        return out;
    }

    if (!allowDuplicate) {
        const auto dupes = m_transactions.findPotentialDuplicates(t, kDuplicateWindowSeconds);
        if (!dupes) {
            out.error = dupes.error();
            return out;
        }
        if (!dupes.value().isEmpty()) {
            out.duplicates = dupes.value();
            return out;
        }
    }

    const auto inserted = m_transactions.insert(t);
    if (!inserted) {
        out.error = inserted.error();
        return out;
    }
    out.saved = true;
    out.transaction = inserted.value();
    emit changed();
    return out;
}

Status TransactionService::update(const Transaction &t)
{
    if (const Status v = validate(t); !v)
        return v;
    const Status s = m_transactions.update(t);
    if (s)
        emit changed();
    return s;
}

Status TransactionService::remove(qint64 id)
{
    const Status s = m_transactions.remove(id);
    if (s)
        emit changed();
    return s;
}

QVariantMap TransactionService::save(const QVariantMap &f)
{
    const auto failure = [](const QString &msg) {
        return QVariantMap{{QStringLiteral("ok"), false}, {QStringLiteral("error"), msg}};
    };

    Transaction t;
    t.id = f.value(QStringLiteral("id")).toLongLong();
    t.accountId = f.value(QStringLiteral("accountId")).toLongLong();
    if (const qint64 cat = f.value(QStringLiteral("categoryId")).toLongLong(); cat > 0)
        t.categoryId = cat;
    bool typeOk = false;
    t.type = transactionTypeFromKey(f.value(QStringLiteral("type")).toString(), &typeOk);
    if (!typeOk)
        return failure(tr("Choose income or expense."));
    t.occurredAt = f.value(QStringLiteral("occurredAt")).toDateTime();
    t.merchant = f.value(QStringLiteral("merchant")).toString();
    t.description = f.value(QStringLiteral("description")).toString();

    const auto account = m_accounts.byId(t.accountId);
    if (!account)
        return failure(tr("Choose an account."));
    const auto amount = Money::fromDecimalString(f.value(QStringLiteral("amount")).toString(), account.value().currency());
    if (!amount)
        return failure(tr("Enter a valid amount."));
    t.amount = *amount;

    if (t.id == 0) {
        const AddOutcome out = add(t, f.value(QStringLiteral("force")).toBool());
        if (out.saved)
            return {{QStringLiteral("ok"), true}, {QStringLiteral("id"), qlonglong(out.transaction.id)}};
        if (!out.duplicates.isEmpty()) {
            return {{QStringLiteral("ok"), false},
                    {QStringLiteral("duplicate"), true},
                    {QStringLiteral("error"), tr("A similar transaction was added a few minutes apart.")}};
        }
        return failure(out.error);
    }

    // Editing keeps the original source/import link.
    if (const auto existing = m_transactions.byId(t.id); existing) {
        t.source = existing.value().source;
        t.importId = existing.value().importId;
    }
    const Status s = update(t);
    if (!s)
        return failure(s.message());
    return {{QStringLiteral("ok"), true}, {QStringLiteral("id"), qlonglong(t.id)}};
}

QVariantMap TransactionService::deleteTransaction(qint64 transactionId)
{
    const Status s = remove(transactionId);
    if (!s)
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), s.message()}};
    return {{QStringLiteral("ok"), true}};
}

} // namespace pf
