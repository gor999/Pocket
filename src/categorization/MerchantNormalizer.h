#pragma once

#include <QString>

namespace pf {

struct NormalizedMerchant
{
    QString display; // tidy name for the UI ("SAS Supermarket")
    QString key; // lower-case, single-spaced; what rules and user "remember" rules compare
};

/// Turns the text a POS terminal sends ("POS SAS SUPERMARKET N5 YEREVAN AM")
/// into a stable merchant name. Removes terminal prefixes, store numbers, legal
/// forms and trailing city/country words; keeps Armenian, Russian and Latin text.
class MerchantNormalizer
{
public:
    static NormalizedMerchant normalize(const QString &raw);
};

} // namespace pf
