#include "categorization/CategorizationService.h"

#include "categorization/MerchantNormalizer.h"

#include <QHash>
#include <QLoggingCategory>

namespace pf {

namespace {
Q_LOGGING_CATEGORY(lcCategorization, "pf.categorization")

bool compatible(CategoryKind kind, TransactionType type)
{
    switch (kind) {
    case CategoryKind::Any: return true;
    case CategoryKind::Income: return type == TransactionType::Income;
    case CategoryKind::Expense: return type == TransactionType::Expense;
    }
    return true;
}
} // namespace

CategorizationService::CategorizationService(CategoryRepository &categories, CategorizationRuleRepository &rules,
                                             QObject *parent)
    : QObject(parent)
    , m_categories(categories)
    , m_rules(rules)
{
}

void CategorizationService::reload()
{
    m_loaded = false;
    m_compiled.clear();
}

void CategorizationService::ensureLoaded() const
{
    if (m_loaded)
        return;
    m_compiled.clear();

    QHash<qint64, CategoryKind> kinds;
    if (const auto cats = m_categories.all()) {
        for (const Category &c : cats.value())
            kinds.insert(c.id, c.kind);
    }
    if (const auto rules = m_rules.all()) {
        for (const CategorizationRule &r : rules.value()) {
            if (!r.enabled || !kinds.contains(r.categoryId))
                continue;
            Compiled c;
            c.rule = r;
            c.categoryKind = kinds.value(r.categoryId);
            if (r.matchType == CategorizationRule::MatchType::Regex) {
                c.regex = QRegularExpression(r.pattern, QRegularExpression::CaseInsensitiveOption |
                                                            QRegularExpression::UseUnicodePropertiesOption);
                if (!c.regex.isValid()) {
                    qCWarning(lcCategorization) << "ignoring invalid regex rule" << r.id << r.pattern;
                    continue;
                }
            } else {
                c.folded = r.pattern.toLower().simplified();
            }
            m_compiled.append(c);
        }
    }
    m_loaded = true;
}

CategorySuggestion CategorizationService::classify(const QString &merchant, const QString &kindText,
                                                   TransactionType type) const
{
    ensureLoaded();
    const NormalizedMerchant nm = MerchantNormalizer::normalize(merchant);
    const QString rawMerchant = merchant.toLower().simplified();
    const QString kind = kindText.toLower().simplified();

    for (const Compiled &c : m_compiled) {
        if (!compatible(c.categoryKind, type))
            continue;

        bool hit = false;
        const auto matchText = [&](const QString &haystack) {
            switch (c.rule.matchType) {
            case CategorizationRule::MatchType::Exact: return haystack == c.folded;
            case CategorizationRule::MatchType::Contains: return !c.folded.isEmpty() && haystack.contains(c.folded);
            case CategorizationRule::MatchType::Regex: return c.regex.match(haystack).hasMatch();
            }
            return false;
        };
        switch (c.rule.field) {
        case CategorizationRule::Field::Merchant:
            hit = matchText(nm.key) || (c.rule.matchType != CategorizationRule::MatchType::Exact && matchText(rawMerchant));
            break;
        case CategorizationRule::Field::Description:
            hit = matchText(kind);
            break;
        case CategorizationRule::Field::Any:
            hit = matchText(nm.key) || matchText(rawMerchant) || matchText(kind);
            break;
        }
        if (!hit)
            continue;

        CategorySuggestion s;
        s.categoryId = c.rule.categoryId;
        s.confidence = c.rule.confidence;
        s.ruleId = c.rule.id;
        s.isUserRule = !c.rule.isSystem;
        return s;
    }
    return {};
}

Result<CategorizationRule> CategorizationService::rememberMerchant(const QString &merchant, qint64 categoryId)
{
    const NormalizedMerchant nm = MerchantNormalizer::normalize(merchant);
    if (nm.key.isEmpty())
        return Result<CategorizationRule>::failure(tr("There is no merchant name to remember."));
    if (!m_categories.byId(categoryId))
        return Result<CategorizationRule>::failure(tr("Choose a category."));

    const auto existing = m_rules.findUserMerchantRule(nm.key);
    if (!existing)
        return existing.status();
    if (existing.value()) {
        CategorizationRule r = *existing.value();
        r.categoryId = categoryId;
        r.enabled = true;
        r.confidence = 100;
        if (const Status s = m_rules.update(r); !s)
            return s;
        reload();
        emit rulesChanged();
        return r;
    }

    CategorizationRule r;
    r.categoryId = categoryId;
    r.matchType = CategorizationRule::MatchType::Exact;
    r.field = CategorizationRule::Field::Merchant;
    r.pattern = nm.key;
    r.priority = kUserRulePriority;
    r.enabled = true;
    r.isSystem = false;
    r.confidence = 100;
    const auto inserted = m_rules.insert(r);
    if (inserted) {
        reload();
        emit rulesChanged();
    }
    return inserted;
}

QVariantList CategorizationService::rules() const
{
    QVariantList out;
    QHash<qint64, QString> names;
    if (const auto cats = m_categories.all()) {
        for (const Category &c : cats.value())
            names.insert(c.id, c.displayName());
    }
    const auto all = m_rules.all();
    if (!all)
        return out;
    for (const CategorizationRule &r : all.value()) {
        out << QVariantMap{
            {QStringLiteral("id"), qlonglong(r.id)},
            {QStringLiteral("pattern"), r.pattern},
            {QStringLiteral("matchType"), int(r.matchType)},
            {QStringLiteral("categoryId"), qlonglong(r.categoryId)},
            {QStringLiteral("categoryName"), names.value(r.categoryId)},
            {QStringLiteral("priority"), r.priority},
            {QStringLiteral("confidence"), r.confidence},
            {QStringLiteral("enabled"), r.enabled},
            {QStringLiteral("isSystem"), r.isSystem},
        };
    }
    return out;
}

QVariantMap CategorizationService::setRuleEnabled(qint64 ruleId, bool enabled)
{
    const Status s = m_rules.setEnabled(ruleId, enabled);
    if (!s)
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), s.message()}};
    reload();
    emit rulesChanged();
    return {{QStringLiteral("ok"), true}};
}

QVariantMap CategorizationService::deleteRule(qint64 ruleId)
{
    const Status s = m_rules.remove(ruleId);
    if (!s)
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), s.message()}};
    reload();
    emit rulesChanged();
    return {{QStringLiteral("ok"), true}};
}

} // namespace pf
