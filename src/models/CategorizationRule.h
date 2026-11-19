#pragma once

#include <QString>

namespace pf {

/// User-editable categorization rule. Rules are evaluated by priority
/// (highest first); the first enabled match decides the category.
/// Used by CategorizationService (bank-SMS import). `confidence` is how sure a
/// match makes us: below CategorizationService::kAutoAssignThreshold the
/// category is only suggested and the transaction goes to review.
struct CategorizationRule
{
    enum class MatchType { Contains, Exact, Regex };
    enum class Field { Any, Merchant, Description };

    qint64 id = 0;
    qint64 categoryId = 0;
    MatchType matchType = MatchType::Contains;
    Field field = Field::Any;
    QString pattern;
    int priority = 0;
    bool enabled = true;
    bool isSystem = false;
    int confidence = 80; // 0..100 (schema v2)
};

} // namespace pf
