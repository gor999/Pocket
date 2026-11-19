#include "database/SmsImportRepository.h"

#include "database/SqlUtil.h"

#include <QSqlQuery>

namespace pf {

namespace {

const QString kColumns = QStringLiteral(
    "id, external_id, channel, sender, bank_id, parser_id, status, review_reason, error_code, error_message, body, "
    "body_hash, confidence, transaction_id, suggested_category_id, suggestion_confidence, balance_minor, "
    "balance_currency, received_at, imported_at");

SmsImportRecord readRecord(const QSqlQuery &q)
{
    SmsImportRecord r;
    r.id = q.value(0).toLongLong();
    r.externalId = q.value(1).toString();
    r.channel = q.value(2).toString();
    r.sender = q.value(3).toString();
    r.bankId = q.value(4).toString();
    r.parserId = q.value(5).toString();
    r.status = q.value(6).toString();
    r.reviewReason = q.value(7).toString();
    r.errorCode = q.value(8).toString();
    r.errorMessage = q.value(9).toString();
    r.body = q.value(10).toString();
    r.bodyHash = q.value(11).toString();
    r.confidence = q.value(12).toInt();
    r.transactionId = sql::optionalInt(q.value(13));
    r.suggestedCategoryId = sql::optionalInt(q.value(14));
    r.suggestionConfidence = q.value(15).toInt();
    if (!q.value(16).isNull() && !q.value(17).toString().isEmpty())
        r.balance = Money(q.value(16).toLongLong(), q.value(17).toString());
    r.receivedAt = sql::fromEpoch(q.value(18).toLongLong());
    r.importedAt = sql::fromEpoch(q.value(19).toLongLong());
    return r;
}

void bindRecord(QSqlQuery &q, const SmsImportRecord &r)
{
    q.addBindValue(sql::text(r.externalId));
    q.addBindValue(sql::text(r.channel));
    q.addBindValue(sql::text(r.sender));
    q.addBindValue(sql::text(r.bankId));
    q.addBindValue(sql::text(r.parserId));
    q.addBindValue(r.status);
    q.addBindValue(sql::text(r.reviewReason));
    q.addBindValue(sql::text(r.errorCode));
    q.addBindValue(sql::text(r.errorMessage));
    q.addBindValue(sql::text(r.body));
    q.addBindValue(sql::text(r.bodyHash));
    q.addBindValue(r.confidence);
    q.addBindValue(sql::nullableInt(r.transactionId));
    q.addBindValue(sql::nullableInt(r.suggestedCategoryId));
    q.addBindValue(r.suggestionConfidence);
    q.addBindValue(r.balance ? QVariant::fromValue<qlonglong>(r.balance->minorUnits())
                             : QVariant(QMetaType::fromType<qlonglong>()));
    q.addBindValue(sql::text(r.balance ? r.balance->currency() : QString())); // NOT NULL column: '' when there is no balance
    q.addBindValue(qlonglong(sql::toEpoch(r.receivedAt.isValid() ? r.receivedAt : QDateTime::currentDateTime())));
    q.addBindValue(qlonglong(sql::toEpoch(r.importedAt.isValid() ? r.importedAt : QDateTime::currentDateTime())));
}

QString statusClause(const QStringList &statuses)
{
    if (statuses.isEmpty())
        return {};
    QStringList marks;
    for (int i = 0; i < statuses.size(); ++i)
        marks << QStringLiteral("?");
    return QStringLiteral(" WHERE status IN (%1)").arg(marks.join(QStringLiteral(", ")));
}

} // namespace

Result<SmsImportRecord> SmsImportRepository::insert(SmsImportRecord r) const
{
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral(
        "INSERT INTO sms_imports (external_id, channel, sender, bank_id, parser_id, status, review_reason, error_code, "
        "error_message, body, body_hash, confidence, transaction_id, suggested_category_id, suggestion_confidence, "
        "balance_minor, balance_currency, received_at, imported_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    bindRecord(q, r);
    if (!q.exec())
        return sql::failed(q, QStringLiteral("Insert SMS import"));
    r.id = q.lastInsertId().toLongLong();
    return r;
}

Status SmsImportRepository::update(const SmsImportRecord &r) const
{
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral(
        "UPDATE sms_imports SET external_id = ?, channel = ?, sender = ?, bank_id = ?, parser_id = ?, status = ?, "
        "review_reason = ?, error_code = ?, error_message = ?, body = ?, body_hash = ?, confidence = ?, "
        "transaction_id = ?, suggested_category_id = ?, suggestion_confidence = ?, balance_minor = ?, "
        "balance_currency = ?, received_at = ?, imported_at = ? WHERE id = ?"));
    bindRecord(q, r);
    q.addBindValue(qlonglong(r.id));
    if (!q.exec())
        return sql::failed(q, QStringLiteral("Update SMS import"));
    return Status::ok();
}

Status SmsImportRepository::remove(qint64 id) const
{
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral("DELETE FROM sms_imports WHERE id = ?"));
    q.addBindValue(qlonglong(id));
    if (!q.exec())
        return sql::failed(q, QStringLiteral("Delete SMS import"));
    return Status::ok();
}

Status SmsImportRepository::removeWithStatuses(const QStringList &statuses) const
{
    if (statuses.isEmpty())
        return Status::ok();
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral("DELETE FROM sms_imports") + statusClause(statuses));
    for (const QString &s : statuses)
        q.addBindValue(s);
    if (!q.exec())
        return sql::failed(q, QStringLiteral("Clear SMS imports"));
    return Status::ok();
}

Result<SmsImportRecord> SmsImportRepository::byId(qint64 id) const
{
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral("SELECT %1 FROM sms_imports WHERE id = ?").arg(kColumns));
    q.addBindValue(qlonglong(id));
    if (!q.exec())
        return sql::failed(q, QStringLiteral("Load SMS import"));
    if (!q.next())
        return Result<SmsImportRecord>::failure(QStringLiteral("SMS import %1 does not exist").arg(id));
    return readRecord(q);
}

Result<std::optional<SmsImportRecord>> SmsImportRepository::byExternalId(const QString &externalId) const
{
    if (externalId.isEmpty())
        return std::optional<SmsImportRecord>();
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral("SELECT %1 FROM sms_imports WHERE external_id = ? LIMIT 1").arg(kColumns));
    q.addBindValue(externalId);
    if (!q.exec())
        return sql::failed(q, QStringLiteral("Find SMS import"));
    if (!q.next())
        return std::optional<SmsImportRecord>();
    return std::optional<SmsImportRecord>(readRecord(q));
}

Result<QList<SmsImportRecord>> SmsImportRepository::byBodyHash(const QString &hash, const QDateTime &around,
                                                               int windowSeconds) const
{
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral(
        "SELECT %1 FROM sms_imports WHERE body_hash = ? AND received_at BETWEEN ? AND ? ORDER BY id DESC").arg(kColumns));
    q.addBindValue(hash);
    q.addBindValue(qlonglong(sql::toEpoch(around) - windowSeconds));
    q.addBindValue(qlonglong(sql::toEpoch(around) + windowSeconds));
    if (!q.exec())
        return sql::failed(q, QStringLiteral("Find SMS by hash"));
    QList<SmsImportRecord> list;
    while (q.next())
        list << readRecord(q);
    return list;
}

Result<QList<SmsImportRecord>> SmsImportRepository::list(const QStringList &statuses, int limit, int offset) const
{
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral("SELECT %1 FROM sms_imports%2 ORDER BY imported_at DESC, id DESC LIMIT ? OFFSET ?")
                  .arg(kColumns, statusClause(statuses)));
    for (const QString &s : statuses)
        q.addBindValue(s);
    q.addBindValue(limit);
    q.addBindValue(offset);
    if (!q.exec())
        return sql::failed(q, QStringLiteral("List SMS imports"));
    QList<SmsImportRecord> list;
    while (q.next())
        list << readRecord(q);
    return list;
}

Result<int> SmsImportRepository::count(const QStringList &statuses) const
{
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM sms_imports") + statusClause(statuses));
    for (const QString &s : statuses)
        q.addBindValue(s);
    if (!q.exec() || !q.next())
        return sql::failed(q, QStringLiteral("Count SMS imports"));
    return q.value(0).toInt();
}

Result<QList<BankImportStats>> SmsImportRepository::bankStats() const
{
    QSqlQuery q(m_db.connection());
    if (!q.exec(QStringLiteral(
            "SELECT bank_id, COUNT(*), SUM(CASE WHEN status IN ('imported', 'needs_review') THEN 1 ELSE 0 END), "
            "MAX(received_at) FROM sms_imports WHERE bank_id <> '' AND error_code <> 'test' GROUP BY bank_id")))
        return sql::failed(q, QStringLiteral("Bank statistics"));
    QList<BankImportStats> list;
    while (q.next()) {
        BankImportStats s;
        s.bankId = q.value(0).toString();
        s.received = q.value(1).toInt();
        s.imported = q.value(2).toInt();
        s.lastReceivedAt = sql::fromEpoch(q.value(3).toLongLong());
        list << s;
    }
    return list;
}

Result<QDateTime> SmsImportRepository::lastReceivedAt() const
{
    QSqlQuery q(m_db.connection());
    if (!q.exec(QStringLiteral("SELECT MAX(received_at) FROM sms_imports WHERE error_code <> 'test'")))
        return sql::failed(q, QStringLiteral("Last received"));
    if (!q.next() || q.value(0).isNull())
        return QDateTime();
    return sql::fromEpoch(q.value(0).toLongLong());
}

Result<QDateTime> SmsImportRepository::lastImportedAt() const
{
    QSqlQuery q(m_db.connection());
    if (!q.exec(QStringLiteral("SELECT MAX(imported_at) FROM sms_imports WHERE status IN ('imported', 'needs_review')")))
        return sql::failed(q, QStringLiteral("Last import"));
    if (!q.next() || q.value(0).isNull())
        return QDateTime();
    return sql::fromEpoch(q.value(0).toLongLong());
}

} // namespace pf
