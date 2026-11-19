#include "database/CategorizationRuleRepository.h"

#include "database/SqlUtil.h"

#include <QDateTime>
#include <QSqlQuery>

namespace pf {

namespace {

const QString kColumns =
    QStringLiteral("id, category_id, match_type, field, pattern, priority, enabled, is_system, confidence");

QString matchKey(CategorizationRule::MatchType t)
{
    switch (t) {
    case CategorizationRule::MatchType::Contains: return QStringLiteral("contains");
    case CategorizationRule::MatchType::Exact: return QStringLiteral("exact");
    case CategorizationRule::MatchType::Regex: return QStringLiteral("regex");
    }
    return QStringLiteral("contains");
}

CategorizationRule::MatchType matchFromKey(const QString &k)
{
    if (k == QLatin1String("exact"))
        return CategorizationRule::MatchType::Exact;
    if (k == QLatin1String("regex"))
        return CategorizationRule::MatchType::Regex;
    return CategorizationRule::MatchType::Contains;
}

QString fieldKey(CategorizationRule::Field f)
{
    switch (f) {
    case CategorizationRule::Field::Any: return QStringLiteral("any");
    case CategorizationRule::Field::Merchant: return QStringLiteral("merchant");
    case CategorizationRule::Field::Description: return QStringLiteral("description");
    }
    return QStringLiteral("any");
}

CategorizationRule::Field fieldFromKey(const QString &k)
{
    if (k == QLatin1String("merchant"))
        return CategorizationRule::Field::Merchant;
    if (k == QLatin1String("description"))
        return CategorizationRule::Field::Description;
    return CategorizationRule::Field::Any;
}

CategorizationRule readRule(const QSqlQuery &q)
{
    CategorizationRule r;
    r.id = q.value(0).toLongLong();
    r.categoryId = q.value(1).toLongLong();
    r.matchType = matchFromKey(q.value(2).toString());
    r.field = fieldFromKey(q.value(3).toString());
    r.pattern = q.value(4).toString();
    r.priority = q.value(5).toInt();
    r.enabled = q.value(6).toBool();
    r.isSystem = q.value(7).toBool();
    r.confidence = q.value(8).toInt();
    return r;
}

} // namespace

Result<QList<CategorizationRule>> CategorizationRuleRepository::all() const
{
    QSqlQuery q(m_db.connection());
    if (!q.exec(QStringLiteral("SELECT %1 FROM categorization_rules ORDER BY priority DESC, id").arg(kColumns)))
        return sql::failed(q, QStringLiteral("List categorization rules"));
    QList<CategorizationRule> list;
    while (q.next())
        list << readRule(q);
    return list;
}

Result<CategorizationRule> CategorizationRuleRepository::byId(qint64 id) const
{
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral("SELECT %1 FROM categorization_rules WHERE id = ?").arg(kColumns));
    q.addBindValue(qlonglong(id));
    if (!q.exec())
        return sql::failed(q, QStringLiteral("Load rule"));
    if (!q.next())
        return Result<CategorizationRule>::failure(QStringLiteral("Rule %1 does not exist").arg(id));
    return readRule(q);
}

Result<CategorizationRule> CategorizationRuleRepository::insert(CategorizationRule rule) const
{
    if (rule.pattern.trimmed().isEmpty())
        return Result<CategorizationRule>::failure(QStringLiteral("The rule pattern is empty"));
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral(
        "INSERT INTO categorization_rules (category_id, match_type, field, pattern, priority, enabled, is_system, "
        "created_at, confidence) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(qlonglong(rule.categoryId));
    q.addBindValue(matchKey(rule.matchType));
    q.addBindValue(fieldKey(rule.field));
    q.addBindValue(rule.pattern);
    q.addBindValue(rule.priority);
    q.addBindValue(rule.enabled ? 1 : 0);
    q.addBindValue(rule.isSystem ? 1 : 0);
    q.addBindValue(qlonglong(QDateTime::currentSecsSinceEpoch()));
    q.addBindValue(rule.confidence);
    if (!q.exec())
        return sql::failed(q, QStringLiteral("Insert rule"));
    rule.id = q.lastInsertId().toLongLong();
    return rule;
}

Status CategorizationRuleRepository::update(const CategorizationRule &rule) const
{
    const auto existing = byId(rule.id);
    if (!existing)
        return existing.status();
    if (existing.value().isSystem)
        return setEnabled(rule.id, rule.enabled);

    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral(
        "UPDATE categorization_rules SET category_id = ?, match_type = ?, field = ?, pattern = ?, priority = ?, "
        "enabled = ?, confidence = ? WHERE id = ?"));
    q.addBindValue(qlonglong(rule.categoryId));
    q.addBindValue(matchKey(rule.matchType));
    q.addBindValue(fieldKey(rule.field));
    q.addBindValue(rule.pattern);
    q.addBindValue(rule.priority);
    q.addBindValue(rule.enabled ? 1 : 0);
    q.addBindValue(rule.confidence);
    q.addBindValue(qlonglong(rule.id));
    if (!q.exec())
        return sql::failed(q, QStringLiteral("Update rule"));
    return Status::ok();
}

Status CategorizationRuleRepository::setEnabled(qint64 id, bool enabled) const
{
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral("UPDATE categorization_rules SET enabled = ? WHERE id = ?"));
    q.addBindValue(enabled ? 1 : 0);
    q.addBindValue(qlonglong(id));
    if (!q.exec())
        return sql::failed(q, QStringLiteral("Enable rule"));
    if (q.numRowsAffected() == 0)
        return Status::error(QStringLiteral("Rule %1 does not exist").arg(id));
    return Status::ok();
}

Status CategorizationRuleRepository::remove(qint64 id) const
{
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral("DELETE FROM categorization_rules WHERE id = ? AND is_system = 0"));
    q.addBindValue(qlonglong(id));
    if (!q.exec())
        return sql::failed(q, QStringLiteral("Delete rule"));
    if (q.numRowsAffected() == 0)
        return Status::error(QStringLiteral("Only your own rules can be deleted; disable built-in rules instead."));
    return Status::ok();
}

Result<std::optional<CategorizationRule>> CategorizationRuleRepository::findUserMerchantRule(const QString &pattern) const
{
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral(
        "SELECT %1 FROM categorization_rules WHERE is_system = 0 AND match_type = 'exact' AND field = 'merchant' "
        "AND pattern = ? LIMIT 1").arg(kColumns));
    q.addBindValue(pattern);
    if (!q.exec())
        return sql::failed(q, QStringLiteral("Find merchant rule"));
    if (!q.next())
        return std::optional<CategorizationRule>();
    return std::optional<CategorizationRule>(readRule(q));
}

} // namespace pf
