#pragma once

#include <QLocale>
#include <QString>
#include <QtGlobal>

#include <compare>
#include <optional>

namespace pf {

/// A monetary amount stored as an integer count of minor units (e.g. cents)
/// together with its ISO 4217 currency code.
///
/// No floating-point arithmetic is used anywhere: parsing, formatting, addition
/// and comparison are all exact integer operations. Arithmetic between two
/// different currencies is a programming error and aborts (we never mix
/// currencies; aggregation is always grouped per currency).
class Money
{
public:
    Money() = default; // invalid (no currency)
    Money(qint64 minorUnits, const QString &currency);

    static Money zero(const QString &currency) { return Money(0, currency); }

    /// Parses a human-entered decimal such as "1234.56", "1,234.56",
    /// "1 234,56" or "1.234,56". Returns nullopt if the text is not a valid
    /// amount for the currency's precision. `ambiguous` is set when the text
    /// could be read two ways (e.g. a lone "1,234": thousands separator or
    /// over-precise decimal); callers that must not guess (bank-message
    /// parsers) should send such results to manual confirmation.
    static std::optional<Money> fromDecimalString(const QString &text,
                                                  const QString &currency,
                                                  bool *ambiguous = nullptr);

    bool isValid() const { return !m_currency.isEmpty(); }
    qint64 minorUnits() const { return m_minor; }
    const QString &currency() const { return m_currency; }
    int exponent() const;

    bool isZero() const { return m_minor == 0; }
    bool isNegative() const { return m_minor < 0; }
    bool isPositive() const { return m_minor > 0; }

    Money abs() const;
    Money operator-() const;
    Money operator+(const Money &other) const;
    Money operator-(const Money &other) const;
    Money &operator+=(const Money &other);
    Money &operator-=(const Money &other);

    bool operator==(const Money &other) const = default;
    std::strong_ordering operator<=>(const Money &other) const;

    /// Canonical machine-readable form: "-1234.50" (dot decimal, no grouping).
    QString toDecimalString() const;

    /// Locale-aware display string, e.g. "1,234.56 ֏" or "-$12.50".
    QString format(const QLocale &locale = QLocale(), bool withSymbol = true) const;

private:
    void requireSameCurrency(const Money &other) const;

    qint64 m_minor = 0;
    QString m_currency;
};

} // namespace pf
