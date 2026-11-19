#include "database/AccountRepository.h"

#include "database/SqlUtil.h"

#include <QSqlQuery>

namespace pf {

namespace {

constexpr const char *kBalanceExpr =
    "SUM(CASE type WHEN 'income' THEN amount_minor ELSE -amount_minor END)";

Account readAccount(const QSqlQuery &q)
{
    Account a;
    a.id = q.value(0).toLongLong();
    a.bankName = q.value(1).toString();
    a.name = q.value(2).toString();
    a.cardNetwork = cardNetworkFromKey(q.value(4).toString());
    a.lastFour = q.value(5).toString();
    a.initialBalance = Money(q.value(6).toLongLong(), q.value(3).toString());
    a.archived = q.value(7).toBool();
    a.sortOrder = q.value(8).toInt();
    a.createdAt = sql::fromEpoch(q.value(9).toLongLong());
    return a;
}

const QString kAccountColumns = QStringLiteral(
    "a.id, a.bank_name, a.name, a.currency, a.card_network, a.last_four, "
    "a.initial_balance_minor, a.archived, a.sort_order, a.created_at");

QVariant lastFourBind(const QString &lastFour)
{
    return lastFour.isEmpty() ? QVariant(QMetaType::fromType<QString>()) : QVariant(lastFour);
}

} // namespace

Result<Account> AccountRepository::insert(Account account) const
{
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral(
        "INSERT INTO accounts (bank_name, name, currency, card_network, last_four, initial_balance_minor, "
        "archived, sort_order, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, COALESCE((SELECT MAX(sort_order) + 1 FROM accounts), 0), ?)"));
    account.createdAt = QDateTime::currentDateTime();
    q.addBindValue(account.bankName.trimmed());
    q.addBindValue(account.name.trimmed());
    q.addBindValue(account.currency());
    q.addBindValue(toKey(account.cardNetwork));
    q.addBindValue(lastFourBind(account.lastFour));
    q.addBindValue(account.initialBalance.minorUnits());
    q.addBindValue(account.archived ? 1 : 0);
    q.addBindValue(sql::toEpoch(account.createdAt));
    if (!q.exec())
        return sql::failed(q, QStringLiteral("Insert account"));
    account.id = q.lastInsertId().toLongLong();
    return account;
}

Status AccountRepository::update(const Account &account) const
{
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral(
        "UPDATE accounts SET bank_name = ?, name = ?, currency = ?, card_network = ?, last_four = ?, "
        "initial_balance_minor = ?, archived = ? WHERE id = ?"));
    q.addBindValue(account.bankName.trimmed());
    q.addBindValue(account.name.trimmed());
    q.addBindValue(account.currency());
    q.addBindValue(toKey(account.cardNetwork));
    q.addBindValue(lastFourBind(account.lastFour));
    q.addBindValue(account.initialBalance.minorUnits());
    q.addBindValue(account.archived ? 1 : 0);
    q.addBindValue(account.id);
    if (!q.exec())
        return sql::failed(q, QStringLiteral("Update account"));
    if (q.numRowsAffected() == 0)
        return Status::error(QStringLiteral("Account %1 does not exist").arg(account.id));
    return Status::ok();
}

Status AccountRepository::remove(qint64 id) const
{
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral("DELETE FROM accounts WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec())
        return sql::failed(q, QStringLiteral("Delete account"));
    return Status::ok();
}

Status AccountRepository::setArchived(qint64 id, bool archived) const
{
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral("UPDATE accounts SET archived = ? WHERE id = ?"));
    q.addBindValue(archived ? 1 : 0);
    q.addBindValue(id);
    if (!q.exec())
        return sql::failed(q, QStringLiteral("Archive account"));
    return Status::ok();
}

Result<Account> AccountRepository::byId(qint64 id) const
{
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral("SELECT %1 FROM accounts a WHERE a.id = ?").arg(kAccountColumns));
    q.addBindValue(id);
    if (!q.exec())
        return sql::failed(q, QStringLiteral("Load account"));
    if (!q.next())
        return Result<Account>::failure(QStringLiteral("Account %1 does not exist").arg(id));
    return readAccount(q);
}

Result<QList<AccountSummary>> AccountRepository::summaries(bool includeArchived) const
{
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral(
        "SELECT %1, "
        "       a.initial_balance_minor + COALESCE(t.net, 0) AS balance_minor, "
        "       COALESCE(t.cnt, 0) AS tx_count "
        "FROM accounts a "
        "LEFT JOIN (SELECT account_id, %2 AS net, COUNT(*) AS cnt FROM transactions GROUP BY account_id) t "
        "       ON t.account_id = a.id "
        "WHERE (? = 1 OR a.archived = 0) "
        "ORDER BY a.sort_order, a.id")
                  .arg(kAccountColumns, QLatin1String(kBalanceExpr)));
    q.addBindValue(includeArchived ? 1 : 0);
    if (!q.exec())
        return sql::failed(q, QStringLiteral("List accounts"));

    QList<AccountSummary> list;
    while (q.next()) {
        AccountSummary s;
        s.account = readAccount(q);
        s.balance = Money(q.value(10).toLongLong(), s.account.currency());
        s.transactionCount = q.value(11).toInt();
        list << s;
    }
    return list;
}

Result<Money> AccountRepository::balance(qint64 id) const
{
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral(
        "SELECT a.currency, a.initial_balance_minor + COALESCE("
        "  (SELECT %1 FROM transactions WHERE account_id = a.id), 0) "
        "FROM accounts a WHERE a.id = ?")
                  .arg(QLatin1String(kBalanceExpr)));
    q.addBindValue(id);
    if (!q.exec())
        return sql::failed(q, QStringLiteral("Account balance"));
    if (!q.next())
        return Result<Money>::failure(QStringLiteral("Account %1 does not exist").arg(id));
    return Money(q.value(1).toLongLong(), q.value(0).toString());
}

Result<QList<Money>> AccountRepository::totalsByCurrency(bool includeArchived) const
{
    const auto list = summaries(includeArchived);
    if (!list)
        return list.status();

    QList<Money> totals;
    for (const AccountSummary &s : list.value()) {
        bool merged = false;
        for (Money &t : totals) {
            if (t.currency() == s.balance.currency()) {
                t += s.balance;
                merged = true;
                break;
            }
        }
        if (!merged)
            totals << s.balance;
    }
    return totals;
}

Result<int> AccountRepository::transactionCount(qint64 accountId) const
{
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM transactions WHERE account_id = ?"));
    q.addBindValue(accountId);
    if (!q.exec() || !q.next())
        return sql::failed(q, QStringLiteral("Count transactions"));
    return q.value(0).toInt();
}

} // namespace pf
