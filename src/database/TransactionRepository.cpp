#include "database/TransactionRepository.h"

#include "database/SqlUtil.h"

#include <QSqlQuery>
#include <QStringList>

namespace pf {

namespace {

const QString kColumns = QStringLiteral(
    "t.id, t.account_id, t.category_id, t.import_id, t.type, t.amount_minor, t.currency, t.occurred_at, "
    "t.merchant, t.description, t.source, t.created_at, t.updated_at");

Transaction readTransaction(const QSqlQuery &q)
{
    Transaction t;
    t.id = q.value(0).toLongLong();
    t.accountId = q.value(1).toLongLong();
    t.categoryId = sql::optionalInt(q.value(2));
    t.importId = sql::optionalInt(q.value(3));
    t.type = transactionTypeFromKey(q.value(4).toString());
    t.amount = Money(q.value(5).toLongLong(), q.value(6).toString());
    t.occurredAt = sql::fromEpoch(q.value(7).toLongLong());
    t.merchant = q.value(8).toString();
    t.description = q.value(9).toString();
    t.source = transactionSourceFromKey(q.value(10).toString());
    t.createdAt = sql::fromEpoch(q.value(11).toLongLong());
    t.updatedAt = sql::fromEpoch(q.value(12).toLongLong());
    return t;
}

struct WhereClause
{
    QString sql;
    QVariantList binds;
};

WhereClause buildWhere(const TransactionFilter &f)
{
    QStringList clauses;
    QVariantList binds;

    if (f.accountId) {
        clauses << QStringLiteral("t.account_id = ?");
        binds << qlonglong(*f.accountId);
    }
    if (f.categoryId) {
        clauses << QStringLiteral("t.category_id = ?");
        binds << qlonglong(*f.categoryId);
    }
    if (f.uncategorizedOnly)
        clauses << QStringLiteral("t.category_id IS NULL");
    if (f.type) {
        clauses << QStringLiteral("t.type = ?");
        binds << toKey(*f.type);
    }
    if (f.from) {
        clauses << QStringLiteral("t.occurred_at >= ?");
        binds << qlonglong(sql::toEpoch(*f.from));
    }
    if (f.to) {
        clauses << QStringLiteral("t.occurred_at < ?");
        binds << qlonglong(sql::toEpoch(*f.to));
    }
    const QStringList words = Transaction::normalizeText(f.text).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString &word : words) {
        clauses << QStringLiteral("t.search_text LIKE ? ESCAPE '\\'");
        binds << QStringLiteral("%") + sql::escapeLike(word) + QStringLiteral("%");
    }

    WhereClause w;
    w.sql = clauses.isEmpty() ? QString() : QStringLiteral(" WHERE ") + clauses.join(QStringLiteral(" AND "));
    w.binds = binds;
    return w;
}

void bindAll(QSqlQuery &q, const QVariantList &binds)
{
    for (const QVariant &v : binds)
        q.addBindValue(v);
}

} // namespace

Result<Transaction> TransactionRepository::insert(Transaction t) const
{
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral(
        "INSERT INTO transactions (account_id, category_id, import_id, type, amount_minor, currency, "
        "occurred_at, merchant, description, source, fingerprint, search_text, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    t.createdAt = t.updatedAt = QDateTime::currentDateTime();
    t.merchant = t.merchant.trimmed();
    t.description = t.description.trimmed();

    q.addBindValue(qlonglong(t.accountId));
    q.addBindValue(sql::nullableInt(t.categoryId));
    q.addBindValue(sql::nullableInt(t.importId));
    q.addBindValue(toKey(t.type));
    q.addBindValue(qlonglong(t.amount.minorUnits()));
    q.addBindValue(t.amount.currency());
    q.addBindValue(qlonglong(sql::toEpoch(t.occurredAt)));
    q.addBindValue(sql::text(t.merchant));
    q.addBindValue(sql::text(t.description));
    q.addBindValue(toKey(t.source));
    q.addBindValue(t.fingerprint());
    q.addBindValue(t.searchText());
    q.addBindValue(qlonglong(sql::toEpoch(t.createdAt)));
    q.addBindValue(qlonglong(sql::toEpoch(t.updatedAt)));
    if (!q.exec())
        return sql::failed(q, QStringLiteral("Insert transaction"));
    t.id = q.lastInsertId().toLongLong();
    return t;
}

Status TransactionRepository::update(const Transaction &input) const
{
    Transaction t = input;
    t.merchant = t.merchant.trimmed();
    t.description = t.description.trimmed();
    t.updatedAt = QDateTime::currentDateTime();

    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral(
        "UPDATE transactions SET account_id = ?, category_id = ?, type = ?, amount_minor = ?, currency = ?, "
        "occurred_at = ?, merchant = ?, description = ?, fingerprint = ?, search_text = ?, updated_at = ? "
        "WHERE id = ?"));
    q.addBindValue(qlonglong(t.accountId));
    q.addBindValue(sql::nullableInt(t.categoryId));
    q.addBindValue(toKey(t.type));
    q.addBindValue(qlonglong(t.amount.minorUnits()));
    q.addBindValue(t.amount.currency());
    q.addBindValue(qlonglong(sql::toEpoch(t.occurredAt)));
    q.addBindValue(sql::text(t.merchant));
    q.addBindValue(sql::text(t.description));
    q.addBindValue(t.fingerprint());
    q.addBindValue(t.searchText());
    q.addBindValue(qlonglong(sql::toEpoch(t.updatedAt)));
    q.addBindValue(qlonglong(t.id));
    if (!q.exec())
        return sql::failed(q, QStringLiteral("Update transaction"));
    if (q.numRowsAffected() == 0)
        return Status::error(QStringLiteral("Transaction %1 does not exist").arg(t.id));
    return Status::ok();
}

Status TransactionRepository::remove(qint64 id) const
{
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral("DELETE FROM transactions WHERE id = ?"));
    q.addBindValue(qlonglong(id));
    if (!q.exec())
        return sql::failed(q, QStringLiteral("Delete transaction"));
    return Status::ok();
}

Result<Transaction> TransactionRepository::byId(qint64 id) const
{
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral("SELECT %1 FROM transactions t WHERE t.id = ?").arg(kColumns));
    q.addBindValue(qlonglong(id));
    if (!q.exec())
        return sql::failed(q, QStringLiteral("Load transaction"));
    if (!q.next())
        return Result<Transaction>::failure(QStringLiteral("Transaction %1 does not exist").arg(id));
    return readTransaction(q);
}

Result<QList<Transaction>> TransactionRepository::query(const TransactionFilter &filter, int limit, int offset) const
{
    const WhereClause w = buildWhere(filter);
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral("SELECT %1 FROM transactions t%2 ORDER BY t.occurred_at DESC, t.id DESC LIMIT ? OFFSET ?")
                  .arg(kColumns, w.sql));
    bindAll(q, w.binds);
    q.addBindValue(limit);
    q.addBindValue(offset);
    if (!q.exec())
        return sql::failed(q, QStringLiteral("Query transactions"));

    QList<Transaction> list;
    while (q.next())
        list << readTransaction(q);
    return list;
}

Result<int> TransactionRepository::count(const TransactionFilter &filter) const
{
    const WhereClause w = buildWhere(filter);
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM transactions t%1").arg(w.sql));
    bindAll(q, w.binds);
    if (!q.exec() || !q.next())
        return sql::failed(q, QStringLiteral("Count transactions"));
    return q.value(0).toInt();
}

Result<QList<CurrencyTotals>> TransactionRepository::totals(const QDateTime &from, const QDateTime &to,
                                                            std::optional<qint64> accountId) const
{
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral(
        "SELECT currency, "
        "       COALESCE(SUM(CASE WHEN type = 'income'  THEN amount_minor END), 0), "
        "       COALESCE(SUM(CASE WHEN type = 'expense' THEN amount_minor END), 0) "
        "FROM transactions WHERE occurred_at >= ? AND occurred_at < ? AND (? IS NULL OR account_id = ?) "
        "GROUP BY currency ORDER BY currency"));
    q.addBindValue(qlonglong(sql::toEpoch(from)));
    q.addBindValue(qlonglong(sql::toEpoch(to)));
    q.addBindValue(sql::nullableInt(accountId));
    q.addBindValue(sql::nullableInt(accountId));
    if (!q.exec())
        return sql::failed(q, QStringLiteral("Period totals"));

    QList<CurrencyTotals> list;
    while (q.next()) {
        CurrencyTotals t;
        t.currency = q.value(0).toString();
        t.income = Money(q.value(1).toLongLong(), t.currency);
        t.expense = Money(q.value(2).toLongLong(), t.currency);
        list << t;
    }
    return list;
}

Result<QList<Transaction>> TransactionRepository::findPotentialDuplicates(const Transaction &candidate,
                                                                          int windowSeconds, qint64 excludeId) const
{
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral(
        "SELECT %1 FROM transactions t "
        "WHERE t.account_id = ? AND t.type = ? AND t.amount_minor = ? AND t.id <> ? "
        "  AND t.occurred_at BETWEEN ? AND ? "
        "ORDER BY t.occurred_at DESC")
                  .arg(kColumns));
    const qint64 at = sql::toEpoch(candidate.occurredAt);
    q.addBindValue(qlonglong(candidate.accountId));
    q.addBindValue(toKey(candidate.type));
    q.addBindValue(qlonglong(candidate.amount.minorUnits()));
    q.addBindValue(qlonglong(excludeId));
    q.addBindValue(qlonglong(at - windowSeconds));
    q.addBindValue(qlonglong(at + windowSeconds));
    if (!q.exec())
        return sql::failed(q, QStringLiteral("Find duplicates"));

    QList<Transaction> list;
    while (q.next())
        list << readTransaction(q);
    return list;
}

} // namespace pf
