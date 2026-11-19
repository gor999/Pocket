#pragma once

#include "models/CategorizationRule.h"

#include <QList>

namespace pf {

/// Built-in categorization rules that ship with the app (stored as
/// `is_system = 1` rows by schema migration v2). They are *starter* keyword
/// lists in English, Russian and Armenian - not a merchant database. Users can
/// disable any of them and add their own; user rules always win.
struct StarterRule
{
    const char *categoryKey;
    CategorizationRule::MatchType matchType;
    const char *pattern; // lower-case for Contains, a regular expression for Regex
    int priority;
    int confidence;
};

const QList<StarterRule> &starterRules();

} // namespace pf
