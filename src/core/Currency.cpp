#include "core/Currency.h"

#include <iterator>

namespace pf {

namespace {

struct Entry
{
    const char *code;
    int exponent;
    const char16_t *symbol;
    bool symbolBefore;
};

// Armenian dram first, then the currencies most common on Armenian and
// international bank cards. Extend freely: nothing else hardcodes this list.
constexpr Entry kEntries[] = {
    {"AMD", 2, u"֏", false}, // ֏
    {"USD", 2, u"$", true},
    {"EUR", 2, u"€", true},
    {"RUB", 2, u"₽", false},
    {"GBP", 2, u"£", true},
    {"GEL", 2, u"₾", false},
    {"CHF", 2, u"CHF", false},
    {"AED", 2, u"AED", false},
    {"TRY", 2, u"₺", false},
    {"UAH", 2, u"₴", false},
    {"KZT", 2, u"₸", false},
    {"BYN", 2, u"Br", false},
    {"AZN", 2, u"₼", false},
    {"KGS", 2, u"KGS", false},
    {"UZS", 2, u"UZS", false},
    {"CAD", 2, u"CA$", true},
    {"AUD", 2, u"A$", true},
    {"CNY", 2, u"CN¥", true},
    {"JPY", 0, u"¥", true},
    {"KRW", 0, u"₩", true},
    {"INR", 2, u"₹", true},
    {"ILS", 2, u"₪", true},
    {"THB", 2, u"฿", true},
    {"SGD", 2, u"S$", true},
    {"PLN", 2, u"zł", false},
    {"CZK", 2, u"Kč", false},
    {"SEK", 2, u"kr", false},
    {"NOK", 2, u"kr", false},
    {"DKK", 2, u"kr", false},
    {"HUF", 2, u"Ft", false},
    {"RON", 2, u"RON", false},
    {"BGN", 2, u"BGN", false},
    {"IRR", 2, u"IRR", false},
};

const Entry *find(const QString &code)
{
    const QString normalized = Currency::normalize(code);
    for (const Entry &e : kEntries) {
        if (normalized == QLatin1String(e.code))
            return &e;
    }
    return nullptr;
}

} // namespace

QString Currency::normalize(const QString &code)
{
    return code.trimmed().toUpper();
}

bool Currency::isSupported(const QString &code)
{
    return find(code) != nullptr;
}

int Currency::exponent(const QString &code)
{
    const Entry *e = find(code);
    return e ? e->exponent : 2;
}

QString Currency::symbol(const QString &code)
{
    const Entry *e = find(code);
    return e ? QString::fromUtf16(e->symbol) : normalize(code);
}

bool Currency::symbolBeforeAmount(const QString &code)
{
    const Entry *e = find(code);
    return e ? e->symbolBefore : false;
}

QStringList Currency::supportedCodes()
{
    QStringList codes;
    codes.reserve(int(std::size(kEntries)));
    for (const Entry &e : kEntries)
        codes << QString::fromLatin1(e.code);
    return codes;
}

} // namespace pf
