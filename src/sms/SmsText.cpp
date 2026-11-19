#include "sms/SmsText.h"

#include "core/Currency.h"

#include <QRegularExpression>

namespace pf::sms::text {

namespace {

using RE = QRegularExpression;
constexpr auto kUni = RE::UseUnicodePropertiesOption;
constexpr auto kCi = RE::CaseInsensitiveOption;

// ---- currencies ----------------------------------------------------------
// Words are matched case-insensitively; bare 3-letter codes only in capitals
// and only if the app supports the currency ("ATM", "POS" are not currencies).
const char *const kCurrencyAlternation =
    "(?i:AMD|֏|դրամ\\w*|դր\\.?|драм\\w*|dram\\w*|USD|\\$|доллар\\w*|долл\\.?|դոլար\\w*|EUR|€|евро|եվրո|"
    "RUB|RUR|₽|руб\\w*|ռուբլ\\w*|GEL|₾|лари|GBP|£)|[A-Z]{3}";

const RE &currencyAfter()
{
    // Optional spaces, then a currency token that is not part of a longer word.
    static const RE re(QStringLiteral("^[ ]{0,2}(%1)(?![\\p{L}])").arg(QString::fromUtf8(kCurrencyAlternation)), kUni);
    return re;
}

const RE &currencyBefore()
{
    static const RE re(QStringLiteral("(?<![\\p{L}])(%1)[ ]{0,2}$").arg(QString::fromUtf8(kCurrencyAlternation)), kUni);
    return re;
}

// A number with optional thousands grouping and 1-2 decimals. It never starts
// or ends inside a larger digit run, and never swallows "15.11.2026".
const RE &numberRe()
{
    static const RE re(QStringLiteral(R"((?<![\d.,])(?:\d{1,3}(?:[ ,.]\d{3})+|\d+)(?:[.,]\d{1,2})?(?![\d]|[.,]\d))"));
    return re;
}

bool isKnownCode(const QString &code)
{
    return !code.isEmpty() && Currency::isSupported(code);
}

} // namespace

QString currencyPattern()
{
    return QString::fromUtf8(kCurrencyAlternation);
}

QString numberPattern()
{
    return QStringLiteral(R"((?:\d{1,3}(?:[ ,.]\d{3})+|\d+)(?:[.,]\d{1,2})?)");
}

QString currencyFromToken(const QString &tokenIn)
{
    QString token = tokenIn.trimmed();
    if (token.isEmpty())
        return {};
    const QString lower = token.toLower();
    struct Entry { const char16_t *prefix; const char *code; };
    static const Entry prefixes[] = {
        {u"֏", "AMD"}, {u"դր", "AMD"}, {u"драм", "AMD"}, {u"dram", "AMD"}, {u"amd", "AMD"},
        {u"$", "USD"}, {u"доллар", "USD"}, {u"долл", "USD"}, {u"դոլար", "USD"}, {u"usd", "USD"},
        {u"€", "EUR"}, {u"евро", "EUR"}, {u"եվրո", "EUR"}, {u"eur", "EUR"},
        {u"₽", "RUB"}, {u"руб", "RUB"}, {u"ռուբլ", "RUB"}, {u"rub", "RUB"}, {u"rur", "RUB"},
        {u"₾", "GEL"}, {u"лари", "GEL"}, {u"gel", "GEL"},
        {u"£", "GBP"}, {u"gbp", "GBP"},
    };
    for (const Entry &e : prefixes) {
        if (lower.startsWith(QString::fromUtf16(e.prefix)))
            return QString::fromLatin1(e.code);
    }
    // Any other upper-case ISO code the app supports (CHF, AED, ...).
    if (token.size() == 3 && token == token.toUpper() && isKnownCode(token))
        return token;
    return {};
}

QList<MoneyMention> findMoneyMentions(const QString &text, bool requireCurrency)
{
    static const RE balanceLabel(
        QStringLiteral(R"(balance|bal\.|avail|available|остаток|остат\.|доступно|баланс|մնացորդ|մնաց|հասանելի|բալանս|հաշվեկշիռ)"),
        kCi | kUni);
    static const RE feeLabel(
        QStringLiteral(R"(\bfee\b|commission|комисси|միջնորդավճար|կոմիսի)"), kCi | kUni);
    static const RE amountLabel(
        QStringLiteral(R"(amount|\bsum\b|total|сумма|итого|գումար|ընդամենը)"), kCi | kUni);

    QList<MoneyMention> result;
    qsizetype previousEnd = 0;
    QRegularExpressionMatchIterator it = numberRe().globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        MoneyMention mention;
        mention.numberText = m.captured(0);
        mention.start = m.capturedStart(0);
        mention.end = m.capturedEnd(0);

        // Currency right after the number wins ("5,000 AMD"), else right before ("AMD 5,000", "$12").
        QString code;
        const QString after = text.mid(mention.end, 16);
        if (const auto a = currencyAfter().match(after); a.hasMatch()) {
            code = currencyFromToken(a.captured(1));
            if (!code.isEmpty())
                mention.end += a.capturedEnd(1);
        }
        if (code.isEmpty()) {
            const qsizetype from = qMax<qsizetype>(0, mention.start - 16);
            const QString before = text.mid(from, mention.start - from);
            if (const auto b = currencyBefore().match(before); b.hasMatch()) {
                code = currencyFromToken(b.captured(1));
                if (!code.isEmpty())
                    mention.start = from + b.capturedStart(1);
            }
        }
        mention.currency = code;

        // Sign directly in front of the number.
        const qsizetype signPos = mention.start - 1;
        if (signPos >= 0) {
            const QChar c = text.at(signPos);
            mention.signNegative = c == QLatin1Char('-') || c == QChar(0x2212) || c == QChar(0x2013);
            mention.signPositive = c == QLatin1Char('+');
        }

        // Label: the few words between the previous mention and this one.
        const qsizetype labelFrom = qMax<qsizetype>(previousEnd, mention.start - 32);
        QString label = text.mid(labelFrom, mention.start - labelFrom);
        const qsizetype lineBreak = label.lastIndexOf(QLatin1Char('\n'));
        const bool sameLine = lineBreak < 0;
        if (!sameLine)
            label = label.mid(lineBreak + 1); // a label belongs to its own line
        // Prefer the nearest label: look only at the tail so "Balance: ... Amount: X" is not confused.
        mention.isBalance = balanceLabel.match(label).hasMatch();
        mention.isFee = feeLabel.match(label).hasMatch();
        mention.isLabelledAmount = !mention.isBalance && !mention.isFee && amountLabel.match(label).hasMatch();

        if (!mention.currency.isEmpty() || !requireCurrency)
            result.append(mention);
        previousEnd = mention.end;
    }
    return result;
}

std::optional<Money> parseAmount(const QString &numberText, const QString &currency, bool *ambiguous)
{
    if (ambiguous)
        *ambiguous = false;
    const QString code = Currency::normalize(currency);
    if (!Currency::isSupported(code))
        return std::nullopt;
    bool amb = false;
    const auto money = Money::fromDecimalString(numberText, code, &amb);
    if (amb && Currency::exponent(code) <= 2) {
        // "5,000" / "5.000" with a currency that has at most two decimals: the
        // separator can only be a thousands separator, because the three digits
        // after it do not fit the currency's precision. Money flags it as a
        // generic ambiguity; for bank messages it would route nearly every
        // whole-number amount to manual review, so it is resolved here. A leading
        // zero ("0,500") is still ambiguous: that cannot be a thousands group.
        static const RE grouped(QStringLiteral(R"(^[+-]?[1-9]\d{0,2}[.,]\d{3}$)"));
        if (grouped.match(numberText.trimmed()).hasMatch())
            amb = false;
    }
    if (ambiguous)
        *ambiguous = amb;
    return money;
}

// ---- direction -------------------------------------------------------------

namespace {

struct Word
{
    RE re;
    bool strong;
};

QList<Word> buildWords(const QStringList &strong, const QStringList &weak)
{
    QList<Word> list;
    for (const QString &s : strong)
        list.append({RE(s, kCi | kUni), true});
    for (const QString &s : weak)
        list.append({RE(s, kCi | kUni), false});
    return list;
}

const QList<Word> &expenseWords()
{
    static const QList<Word> w = buildWords(
        // strong
        {QStringLiteral(R"((?<![\p{L}])(withdraw\w*|debit(?:ed)?|spent|charged|paid|sent|outgoing|transfer\s+to)(?![\p{L}]))"),
         QStringLiteral(R"(списани\w*|списан\w*|снят\w*|расход\w*|отправлен\w*|исходящ\w*)"),
         QStringLiteral(R"(դուրսգր\w*|կանխիկաց\w*|ելք\w*|ծախս\w*|դեբետ\w*)")},
        // weak: the operation word of an ordinary payment
        {QStringLiteral(R"((?<![\p{L}])(purchase|payment|bought|pos)(?![\p{L}]))"),
         QStringLiteral(R"(покупк\w*|оплат\w*|платеж\w*|платёж\w*)"),
         QStringLiteral(R"(գնում\w*|վճարում\w*|վճար(?![\p{L}]))")});
    return w;
}

const QList<Word> &incomeWords()
{
    static const QList<Word> w = buildWords(
        {QStringLiteral(R"((?<![\p{L}])(credit(?:ed)?|received?|deposit(?:ed)?|refund(?:ed)?|salary|payroll|cashback|top[- ]?up|incoming|revers\w*|cancel\w*|transfer\s+from)(?![\p{L}]))"),
         QStringLiteral(R"(зачислени\w*|зачислен\w*|пополнени\w*|поступлени\w*|поступил\w*|получен\w*|возврат\w*|зарплат\w*|кэшбэк\w*|начислен\w*|входящ\w*|отмен\w*)"),
         QStringLiteral(R"(մուտք\w*|ստացում\w*|ստացվ\w*|հաշվեգր\w*|լրացում\w*|վերադարձ\w*|աշխատավարձ\w*|քեշբեք\w*|չեղարկ\w*)")},
        {});
    return w;
}

struct Hit
{
    bool found = false;
    bool strong = false;
    qsizetype pos = 0;
    QString text;
};

Hit firstHit(const QList<Word> &words, const QString &text)
{
    Hit best;
    for (const Word &w : words) {
        const auto m = w.re.match(text);
        if (!m.hasMatch())
            continue;
        // Strong words outrank weak ones; among equals the earlier one wins.
        const bool better = !best.found || (w.strong && !best.strong) ||
                            (w.strong == best.strong && m.capturedStart(0) < best.pos);
        if (better) {
            best.found = true;
            best.strong = w.strong;
            best.pos = m.capturedStart(0);
            best.text = m.captured(0);
        }
    }
    return best;
}

} // namespace

Direction detectDirection(const QString &textIn)
{
    // "credit card" / "debit card" name the product, not the direction.
    static const RE products(
        QStringLiteral(R"((credit|debit)\s+card|(кредитн|дебетов)\w*\s+карт\w*|վարկային\s+քարտ\w*|դեբետային\s+քարտ\w*)"),
        kCi | kUni);
    QString text = textIn;
    text.replace(products, QStringLiteral(" "));

    const Hit expense = firstHit(expenseWords(), text);
    const Hit income = firstHit(incomeWords(), text);

    Direction d;
    if (!expense.found && !income.found)
        return d;
    d.found = true;
    if (expense.found && !income.found) {
        d.type = TransactionType::Expense;
        d.keyword = expense.text;
    } else if (income.found && !expense.found) {
        d.type = TransactionType::Income;
        d.keyword = income.text;
    } else if (income.strong != expense.strong) {
        // "Payment received": the strong word decides, and it is not really ambiguous.
        const bool incomeWins = income.strong;
        d.type = incomeWins ? TransactionType::Income : TransactionType::Expense;
        d.keyword = incomeWins ? income.text : expense.text;
    } else {
        d.ambiguous = true;
        const bool incomeFirst = income.pos < expense.pos;
        d.type = incomeFirst ? TransactionType::Income : TransactionType::Expense;
        d.keyword = incomeFirst ? income.text : expense.text;
    }

    static const RE reversal(QStringLiteral(R"(revers\w*|cancel\w*|отмен\w*|չեղարկ\w*)"), kCi | kUni);
    d.reversal = d.type == TransactionType::Income && reversal.match(text).hasMatch();
    return d;
}

// ---- card ----------------------------------------------------------------

QString findCardLastFour(const QString &text)
{
    static const RE labelled(
        QStringLiteral(R"((?:card|карт[аыуе]?|քարտ\w*|ending(?:\s+in)?|ends\s+with|оканчива\w+|заканчива\w+|№|#)[^\d\n]{0,10}?(?:[*•xX]+[ -]?)*(\d{4})(?!\d))"),
        kCi | kUni);
    static const RE masked(QStringLiteral(R"([*•]{2,}[ -]?(\d{4})(?!\d))"));
    static const RE star(QStringLiteral(R"(\*\s?(\d{4})(?!\d))"));
    for (const RE *re : {&labelled, &masked, &star}) {
        const auto m = re->match(text);
        if (m.hasMatch())
            return m.captured(1);
    }
    return {};
}

// ---- date / time -----------------------------------------------------------

namespace {

bool validDate(int y, int mo, int d)
{
    return QDate::isValid(y, mo, d);
}

int fullYear(int y)
{
    return y < 100 ? 2000 + y : y;
}

} // namespace

std::optional<DateTimeMatch> findDateTime(const QString &text, const QDateTime &reference)
{
    const QDateTime ref = reference.isValid() ? reference : QDateTime::currentDateTime();
    const QDate refDate = ref.date();

    static const RE iso(QStringLiteral(R"((?<!\d)(\d{4})-(\d{2})-(\d{2})(?:[ T]+(\d{1,2}):(\d{2})(?::(\d{2}))?)?(?!\d))"));
    static const RE dayFirst(QStringLiteral(R"((?<![\d.])(\d{1,2})[./-](\d{1,2})[./-](\d{4}|\d{2})(?!\d)(?:[ ,T]+(\d{1,2}):(\d{2})(?::(\d{2}))?)?)"));
    static const RE noYear(QStringLiteral(R"((?<![\d.])(\d{1,2})[./](\d{1,2})[ ,T]+(\d{1,2}):(\d{2})(?::(\d{2}))?)"));
    static const RE timeOnly(QStringLiteral(R"((?<![\d:])(\d{1,2}):(\d{2})(?::(\d{2}))?(?![\d:]))"));

    const auto make = [](int y, int mo, int d, int h, int mi, int s, bool hasTime) -> std::optional<DateTimeMatch> {
        if (!validDate(y, mo, d))
            return std::nullopt;
        if (h < 0 || h > 23 || mi < 0 || mi > 59 || s < 0 || s > 59)
            return std::nullopt;
        DateTimeMatch r;
        r.value = QDateTime(QDate(y, mo, d), QTime(h, mi, s));
        r.hasDate = true;
        r.hasTime = hasTime;
        return r;
    };

    // Earliest position wins when several formats are present.
    struct Candidate { qsizetype pos; DateTimeMatch match; };
    std::optional<Candidate> best;
    const auto offer = [&best](qsizetype pos, const std::optional<DateTimeMatch> &m) {
        if (m && (!best || pos < best->pos))
            best = Candidate{pos, *m};
    };

    {
        QRegularExpressionMatchIterator it = iso.globalMatch(text);
        while (it.hasNext()) {
            const auto m = it.next();
            const bool t = m.capturedLength(4) > 0;
            offer(m.capturedStart(0), make(m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt(),
                                           t ? m.captured(4).toInt() : 0, t ? m.captured(5).toInt() : 0,
                                           t ? m.captured(6).toInt() : 0, t));
        }
    }
    {
        QRegularExpressionMatchIterator it = dayFirst.globalMatch(text);
        while (it.hasNext()) {
            const auto m = it.next();
            const bool t = m.capturedLength(4) > 0;
            // Armenian/Russian/European bank messages are day-first.
            offer(m.capturedStart(0), make(fullYear(m.captured(3).toInt()), m.captured(2).toInt(), m.captured(1).toInt(),
                                           t ? m.captured(4).toInt() : 0, t ? m.captured(5).toInt() : 0,
                                           t ? m.captured(6).toInt() : 0, t));
        }
    }
    {
        QRegularExpressionMatchIterator it = noYear.globalMatch(text);
        while (it.hasNext()) {
            const auto m = it.next();
            // No year written: use the reference year (or the previous one if that would be in the future).
            int year = refDate.year();
            auto candidate = make(year, m.captured(2).toInt(), m.captured(1).toInt(), m.captured(3).toInt(),
                                  m.captured(4).toInt(), m.captured(5).toInt(), true);
            if (candidate && candidate->value > ref.addDays(1))
                candidate = make(year - 1, m.captured(2).toInt(), m.captured(1).toInt(), m.captured(3).toInt(),
                                 m.captured(4).toInt(), m.captured(5).toInt(), true);
            offer(m.capturedStart(0), candidate);
        }
    }
    if (best) {
        DateTimeMatch r = best->match;
        if (!r.hasTime) {
            // Date only: keep the time of day of the reference if it is the same day, else noon.
            r.value = QDateTime(r.value.date(), r.value.date() == refDate ? ref.time() : QTime(12, 0));
        }
        return r;
    }

    // A bare time ("14:32") on the reference date.
    const auto t = timeOnly.match(text);
    if (t.hasMatch()) {
        const int h = t.captured(1).toInt();
        const int mi = t.captured(2).toInt();
        const int s = t.captured(3).isEmpty() ? 0 : t.captured(3).toInt();
        if (h <= 23 && mi <= 59 && s <= 59) {
            DateTimeMatch r;
            r.value = QDateTime(refDate, QTime(h, mi, s));
            if (r.value > ref.addSecs(3600)) // "23:50" received just after midnight => yesterday
                r.value = r.value.addDays(-1);
            r.hasTime = true;
            return r;
        }
    }
    return std::nullopt;
}

// ---- merchant --------------------------------------------------------------

QString cleanMerchantCandidate(const QString &candidate)
{
    QString s = candidate.simplified();
    static const RE lead(QStringLiteral(R"(^[\s,;:\-–—@|/\\.]+)"));
    static const RE trail(QStringLiteral(R"([\s,;:\-–—@|/\\.]+$)"));
    s.remove(lead);
    s.remove(trail);

    // Leading prepositions: "at SHOP", "в SHOP".
    static const RE prep(QStringLiteral(R"(^(?:at|in|from|to|by|в|на|от|у)\s+)"), kCi | kUni);
    s.remove(prep);
    s.remove(lead);
    // Trailing connectors left over when the text was cut at a date: "NETFLIX.COM on".
    static const RE tailWord(QStringLiteral(R"(\s+(?:on|at|in|dated|from|от|в|на|числа)$)"), kCi | kUni);
    s.remove(tailWord);
    s.remove(trail);
    if (s.size() > 80)
        s = s.left(80).trimmed();

    // Reject values that are not names: only digits/punctuation, or just a currency/keyword.
    static const RE hasLetter(QStringLiteral(R"(\p{L})"), kUni);
    if (!hasLetter.match(s).hasMatch())
        return {};
    if (s.size() < 2)
        return {};
    if (!currencyFromToken(s).isEmpty())
        return {};
    return s;
}

QString extractMerchant(const QString &text, qsizetype afterAmountPos)
{
    // 1) Explicitly labelled field.
    static const RE labelled(
        QStringLiteral(R"((?:merchant|place|location|shop|store|terminal|recipient|beneficiary|counterparty|payee|sender|)"
                       R"(место|магазин|терминал|получатель|отправитель|контрагент|)"
                       R"(վայր|վաճառող|տեղ|ստացող|ուղարկող|կետ)\s*[:：]\s*([^\n]+))"),
        kCi | kUni);
    if (const auto m = labelled.match(text); m.hasMatch()) {
        QString v = m.captured(1);
        // Stop at the next labelled field on the same line.
        static const RE nextLabel(QStringLiteral(R"(\s*[,;]\s*(?:card|карта|քարտ|date|дата|ամսաթիվ|balance|остаток|մնացորդ|time)\b.*$)"),
                                  kCi | kUni);
        v.remove(nextLabel);
        const QString cleaned = cleanMerchantCandidate(v);
        if (!cleaned.isEmpty())
            return cleaned;
    }

    // 2) The text after the amount, up to the first date, time, card or balance marker.
    if (afterAmountPos < 0 || afterAmountPos >= text.size())
        return {};
    QString tail = text.mid(afterAmountPos);
    static const RE stop(
        QStringLiteral(R"(\n|(?<![\d.])\d{1,2}[./-]\d{1,2}[./-]\d{2,4}|(?<![\d:])\d{1,2}:\d{2}|)"
                       R"((?<![\p{L}])(?:card|карт[аыуе]?|քարտ\w*|balance|bal\.?|avail\w*|остаток|баланс|մնացորդ|հասանելի|fee|комисси\w*)(?![\p{L}])|)"
                       R"([*•]{1,}\s?\d{4}|[.;]\s)"),
        kCi | kUni);
    if (const auto m = stop.match(tail); m.hasMatch())
        tail = tail.left(m.capturedStart(0));
    return cleanMerchantCandidate(tail);
}

// ---- language ----------------------------------------------------------------

Language detectLanguage(const QString &text)
{
    int hy = 0, ru = 0, en = 0;
    for (const QChar c : text) {
        const ushort u = c.unicode();
        if (u >= 0x0531 && u <= 0x058F)
            ++hy;
        else if (u >= 0x0400 && u <= 0x04FF)
            ++ru;
        else if ((u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z'))
            ++en;
    }
    // Bank messages written in Armenian or Russian still contain Latin merchant
    // names and card words, so a script wins as soon as it has real presence.
    if (hy >= 6 && hy >= ru)
        return Language::Armenian;
    if (ru >= 6)
        return Language::Russian;
    if (en > 0)
        return Language::English;
    return Language::Unknown;
}

QString foldForMatching(const QString &text)
{
    return text.toLower().simplified();
}

} // namespace pf::sms::text
