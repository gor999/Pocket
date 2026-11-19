#pragma once

#include "core/Result.h"
#include "database/CategorizationRuleRepository.h"
#include "database/CategoryRepository.h"
#include "models/Enums.h"

#include <QObject>
#include <QRegularExpression>
#include <QVariantList>
#include <QVariantMap>

#include <optional>

namespace pf {

/// What the classifier concluded for one transaction.
struct CategorySuggestion
{
    std::optional<qint64> categoryId; // empty: nothing matched
    int confidence = 0; // 0..100
    qint64 ruleId = 0;
    bool isUserRule = false;

    /// True if the category may be applied without asking the user.
    bool autoAssign(int threshold) const { return categoryId.has_value() && confidence >= threshold; }
};

/// Rule-based, fully on-device categorization (no network, no AI service).
///
/// Order of evaluation: highest rule priority first; user rules (priority 1000)
/// therefore always beat the built-in starter rules. A rule whose category does
/// not fit the transaction direction (e.g. "Salary" on an expense) is skipped.
/// The *confidence* of the matching rule decides whether the category is
/// applied or only suggested for review.
class CategorizationService : public QObject
{
    Q_OBJECT

public:
    /// Below this confidence a category is suggested, never applied silently.
    static constexpr int kAutoAssignThreshold = 70;
    static constexpr int kUserRulePriority = 1000;

    CategorizationService(CategoryRepository &categories, CategorizationRuleRepository &rules, QObject *parent = nullptr);

    /// `merchant` is the raw text from the SMS; `kindText` the operation word
    /// ("Purchase", "Salary", "Կանխիկացում").
    CategorySuggestion classify(const QString &merchant, const QString &kindText, TransactionType type) const;

    /// "Always categorize this merchant like that": stores (or updates) an exact
    /// merchant rule owned by the user.
    Result<CategorizationRule> rememberMerchant(const QString &merchant, qint64 categoryId);

    /// Drops the compiled-rule cache (call after rules were changed elsewhere).
    void reload();

    // --- QML facade -----------------------------------------------------------
    /// [{id, pattern, matchType, categoryId, categoryName, priority, confidence, enabled, isSystem}]
    Q_INVOKABLE QVariantList rules() const;
    Q_INVOKABLE QVariantMap setRuleEnabled(qint64 ruleId, bool enabled);
    Q_INVOKABLE QVariantMap deleteRule(qint64 ruleId);

signals:
    void rulesChanged();

private:
    struct Compiled
    {
        CategorizationRule rule;
        CategoryKind categoryKind = CategoryKind::Any;
        QString folded; // lower-cased pattern for Contains/Exact
        QRegularExpression regex; // for Regex
    };
    void ensureLoaded() const;

    CategoryRepository &m_categories;
    CategorizationRuleRepository &m_rules;
    mutable bool m_loaded = false;
    mutable QList<Compiled> m_compiled;
};

} // namespace pf
