#include "core/Money.h"

#include "core/Currency.h"

#include <QStringList>
#include <QtNumeric>

namespace pf {

namespace {

quint64 pow10(int exponent)
{
    quint64 v = 1;
    for (int i = 0; i < exponent; ++i)
        v *= 10;
    return v;
}

quint64 magnitude(qint64 v)
{
    return v < 0 ? (~quint64(v) + 1) : quint64(v);
}

} // namespace

Money::Money(qint64 minorUnits, const QString &currency)
    : m_minor(minorUnits)
    , m_currency(Currency::normalize(currency))
{
}

int Money::exponent() const
{
    return Currency::exponent(m_currency);
}

void Money::requireSameCurrency(const Money &other) const
{
    if (m_currency != other.m_currency)
        qFatal("Money: currency mismatch (%s vs %s)", qPrintable(m_currency), qPrintable(other.m_currency));
}

Money Money::abs() const
{
    return Money(m_minor < 0 ? -m_minor : m_minor, m_currency);
}

Money Money::operator-() const
{
    return Money(-m_minor, m_currency);
}

Money Money::operator+(const Money &other) const
{
    requireSameCurrency(other);
    qint64 sum = 0;
    if (qAddOverflow(m_minor, other.m_minor, &sum))
        qFatal("Money: overflow in addition");
    return Money(sum, m_currency);
}

Money Money::operator-(const Money &other) const
{
    requireSameCurrency(other);
    qint64 diff = 0;
    if (qSubOverflow(m_minor, other.m_minor, &diff))
        qFatal("Money: overflow in subtraction");
    return Money(diff, m_currency);
}

Money &Money::operator+=(const Money &other)
{
    *this = *this + other;
    return *this;
}

Money &Money::operator-=(const Money &other)
{
    *this = *this - other;
    return *this;
}

std::strong_ordering Money::operator<=>(const Money &other) const
{
    requireSameCurrency(other);
    return m_minor <=> other.m_minor;
}

QString Money::toDecimalString() const
{
    const int exp = exponent();
    const quint64 p = pow10(exp);
    const quint64 mag = magnitude(m_minor);
    QString text = QString::number(mag / p);
    if (exp > 0)
        text += QLatin1Char('.') + QStringLiteral("%1").arg(mag % p, exp, 10, QLatin1Char('0'));
    if (m_minor < 0)
        text.prepend(QLatin1Char('-'));
    return text;
}

QString Money::format(const QLocale &locale, bool withSymbol) const
{
    if (!isValid())
        return {};

    const int exp = exponent();
    const quint64 p = pow10(exp);
    const quint64 mag = magnitude(m_minor);

    QString number = locale.toString(qulonglong(mag / p));
    if (exp > 0) {
        number += locale.decimalPoint()
            + QStringLiteral("%1").arg(mag % p, exp, 10, QLatin1Char('0'));
    }

    QString sign;
    if (m_minor < 0)
        sign = locale.negativeSign();

    if (!withSymbol)
        return sign + number;

    const QString symbol = Currency::symbol(m_currency);
    if (Currency::symbolBeforeAmount(m_currency)) {
        // "$1,234.56" / "-$12.50"; multi-letter symbols get a thin gap.
        const bool wordLike = symbol.size() > 1 && symbol.at(symbol.size() - 1).isLetter();
        return sign + symbol + (wordLike ? QStringLiteral(" ") : QString()) + number;
    }
    return sign + number + QStringLiteral(" ") + symbol;
}

std::optional<Money> Money::fromDecimalString(const QString &input, const QString &currency, bool *ambiguous)
{
    if (ambiguous)
        *ambiguous = false;

    const QString code = Currency::normalize(currency);
    if (!Currency::isSupported(code))
        return std::nullopt;
    const int exp = Currency::exponent(code);

    QString s = input.trimmed();
    bool negative = false;
    if (s.startsWith(QLatin1Char('-')) || s.startsWith(QChar(0x2212))) {
        negative = true;
        s.remove(0, 1);
    } else if (s.startsWith(QLatin1Char('+'))) {
        s.remove(0, 1);
    }
    s = s.trimmed();

    // Keep digits and the two candidate separators; drop grouping whitespace
    // and apostrophes (Swiss style); reject anything else.
    QString cleaned;
    cleaned.reserve(s.size());
    for (const QChar ch : s) {
        if ((ch >= QLatin1Char('0') && ch <= QLatin1Char('9')) || ch == QLatin1Char('.') || ch == QLatin1Char(',')) {
            cleaned += ch;
        } else if (ch.isSpace() || ch == QLatin1Char('\'') || ch == QChar(0x2019)) {
            continue;
        } else {
            return std::nullopt;
        }
    }
    if (cleaned.isEmpty())
        return std::nullopt;

    const int lastDot = cleaned.lastIndexOf(QLatin1Char('.'));
    const int lastComma = cleaned.lastIndexOf(QLatin1Char(','));
    QChar decimalSep; // null => no decimal part

    if (lastDot >= 0 && lastComma >= 0) {
        // Both kinds present: the later one is the decimal separator.
        decimalSep = lastDot > lastComma ? QLatin1Char('.') : QLatin1Char(',');
        const QChar thousands = decimalSep == QLatin1Char('.') ? QLatin1Char(',') : QLatin1Char('.');
        if (cleaned.count(decimalSep) != 1 || cleaned.lastIndexOf(thousands) > cleaned.indexOf(decimalSep))
            return std::nullopt;
        cleaned.remove(thousands);
    } else if (lastDot >= 0 || lastComma >= 0) {
        const QChar sep = lastDot >= 0 ? QLatin1Char('.') : QLatin1Char(',');
        const int occurrences = cleaned.count(sep);
        const int idx = cleaned.lastIndexOf(sep);
        const int digitsAfter = cleaned.size() - idx - 1;

        if (occurrences > 1) {
            // Only thousands separators: validate 1-3 leading digits then groups of 3.
            const QStringList groups = cleaned.split(sep);
            if (groups.first().isEmpty() || groups.first().size() > 3)
                return std::nullopt;
            for (int i = 1; i < groups.size(); ++i) {
                if (groups.at(i).size() != 3)
                    return std::nullopt;
            }
            cleaned.remove(sep);
        } else if (digitsAfter == 0) {
            cleaned.chop(1); // "12." -> 12
        } else if (digitsAfter <= exp) {
            decimalSep = sep; // "12.5", "12,50"
        } else if (digitsAfter == 3 && idx >= 1 && idx <= 3) {
            // "1,234": almost certainly a thousands separator, but not certain.
            cleaned.remove(sep);
            if (ambiguous)
                *ambiguous = true;
        } else {
            return std::nullopt; // more decimals than the currency allows
        }
    }

    QString integerPart = cleaned;
    QString fractionPart;
    if (!decimalSep.isNull()) {
        const int idx = cleaned.indexOf(decimalSep);
        integerPart = cleaned.left(idx);
        fractionPart = cleaned.mid(idx + 1);
        if (fractionPart.size() > exp)
            return std::nullopt;
    }
    if (integerPart.isEmpty() && fractionPart.isEmpty())
        return std::nullopt;
    while (fractionPart.size() < exp)
        fractionPart += QLatin1Char('0');

    qint64 minor = 0;
    for (const QChar ch : integerPart + fractionPart) {
        qint64 shifted = 0;
        qint64 next = 0;
        if (qMulOverflow(minor, qint64(10), &shifted) || qAddOverflow(shifted, qint64(ch.unicode() - '0'), &next))
            return std::nullopt;
        minor = next;
    }
    return Money(negative ? -minor : minor, code);
}

} // namespace pf
