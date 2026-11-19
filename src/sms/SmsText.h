#pragma once

#include "sms/SmsTypes.h"

#include <QDateTime>
#include <QList>
#include <QString>

#include <optional>

namespace pf::sms::text {

/// A number found in an SMS, with the currency written next to it.
struct MoneyMention
{
    QString numberText; // as written, e.g. "1 234,50"
    QString currency; // ISO code, empty when no supported currency is adjacent
    qsizetype start = 0; // span of number + currency within the text
    qsizetype end = 0;
    bool isBalance = false; // preceded by "Balance", "Available", "Остаток", "Մնացորդ", ...
    bool isFee = false; // preceded by "Fee", "Commission", "Комиссия", ...
    bool isLabelledAmount = false; // preceded by "Amount", "Сумма", "Գումար", ...
    bool signNegative = false; // "-5000"
    bool signPositive = false; // "+5000"
};

/// All number+currency pairs in reading order. Numbers without an adjacent
/// currency are returned only when `requireCurrency` is false.
QList<MoneyMention> findMoneyMentions(const QString &text, bool requireCurrency = true);

/// Exact integer-minor-unit parse. `ambiguous` is set when the written form
/// could mean two things ("1,234" with a 2-digit-precision currency is not
/// ambiguous: only a thousands separator is valid; "1,23" vs "1.23" is a
/// decimal point either way).
std::optional<Money> parseAmount(const QString &numberText, const QString &currency, bool *ambiguous = nullptr);

/// Maps a token ("֏", "руб", "USD", "$") to an ISO code; empty if unknown.
QString currencyFromToken(const QString &token);

/// Regular-expression source matching any currency token this module understands
/// (use inside a pattern compiled with UseUnicodePropertiesOption).
QString currencyPattern();
/// Regular-expression source for one amount, with optional grouping and decimals.
QString numberPattern();

struct Direction
{
    bool found = false;
    TransactionType type = TransactionType::Expense;
    bool ambiguous = false; // both directions present with equal strength
    bool reversal = false;
    QString keyword; // the matched operation word as written
};
Direction detectDirection(const QString &text);

/// Last four digits of the card the SMS refers to ("*1234", "card 1234", ...).
QString findCardLastFour(const QString &text);

struct DateTimeMatch
{
    QDateTime value;
    bool hasDate = false;
    bool hasTime = false;
};
/// First date/time written in the text (day-first: dd.MM.yyyy, dd/MM/yy, ISO).
/// `reference` supplies the year for "dd.MM HH:mm" and the date for a bare time.
std::optional<DateTimeMatch> findDateTime(const QString &text, const QDateTime &reference);

/// Merchant / counterparty. Labelled fields first ("Merchant: ...", "Վայր: ..."),
/// then the text that follows the amount. Empty when nothing credible is found.
QString extractMerchant(const QString &text, qsizetype afterAmountPos);
/// Strips punctuation and rejects values that are really numbers, dates or keywords.
QString cleanMerchantCandidate(const QString &candidate);

Language detectLanguage(const QString &text);

/// Lower-cases and removes Armenian/Latin/Cyrillic diacritic noise for matching.
QString foldForMatching(const QString &text);

} // namespace pf::sms::text
