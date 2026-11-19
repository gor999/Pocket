#pragma once

#include <QString>
#include <QStringList>

namespace pf {

/// Registry of supported ISO 4217 currencies and their minor-unit precision.
/// Precision drives how many digits Money keeps after the decimal separator
/// (e.g. 2 for AMD/USD/EUR, 0 for JPY/KRW).
class Currency
{
public:
    static QString normalize(const QString &code); // trimmed, upper-case
    static bool isSupported(const QString &code);
    static int exponent(const QString &code); // minor-unit digits; 2 for unknown codes
    static QString symbol(const QString &code); // falls back to the code itself
    static bool symbolBeforeAmount(const QString &code);
    static QStringList supportedCodes();
};

} // namespace pf
