#pragma once

#include "core/Result.h"
#include "database/DatabaseManager.h"
#include "models/CategorizationRule.h"

#include <QList>

namespace pf {

/// SQLite persistence for `categorization_rules`.
class CategorizationRuleRepository
{
public:
    explicit CategorizationRuleRepository(const DatabaseManager &db) : m_db(db) {}

    /// Highest priority first; ties by id.
    Result<QList<CategorizationRule>> all() const;
    Result<CategorizationRule> byId(qint64 id) const;
    Result<CategorizationRule> insert(CategorizationRule rule) const;
    /// Changes category / pattern / priority / confidence / enabled. System
    /// rules can only be enabled or disabled.
    Status update(const CategorizationRule &rule) const;
    Status setEnabled(qint64 id, bool enabled) const;
    /// User rules only; system rules cannot be deleted (disable them instead).
    Status remove(qint64 id) const;

    /// The user's exact merchant rule for `pattern`, if one exists.
    Result<std::optional<CategorizationRule>> findUserMerchantRule(const QString &pattern) const;

private:
    const DatabaseManager &m_db;
};

} // namespace pf
