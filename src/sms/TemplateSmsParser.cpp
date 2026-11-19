#include "sms/TemplateSmsParser.h"

#include "sms/SmsSanitizer.h"
#include "sms/SmsText.h"

#include <QLoggingCategory>

namespace pf::sms {

namespace {

Q_LOGGING_CATEGORY(lcParser, "pf.sms.parser")

using RE = QRegularExpression;
constexpr auto kOpts = RE::CaseInsensitiveOption | RE::UseUnicodePropertiesOption;

struct Vocabulary
{
    Language language;
    QString key; // for template ids
    QString card;
    QString balance;
};

const QList<Vocabulary> &vocabularies()
{
    static const QList<Vocabulary> v = {
        {Language::English, QStringLiteral("en"), QStringLiteral(R"(card)"),
         QStringLiteral(R"((?:available\s+)?balance|avail\.?|bal\.?)")},
        {Language::Russian, QStringLiteral("ru"), QStringLiteral(R"(карт[аыуе]?)"),
         QStringLiteral(R"(остаток(?:\s+на\s+счете)?|баланс|доступно)")},
        {Language::Armenian, QStringLiteral("hy"), QStringLiteral(R"(քարտ\w*)"),
         QStringLiteral(R"(հասանելի(?:\s+մնացորդ)?|մնացորդ|բալանս)")},
    };
    return v;
}

QString dateTimePattern()
{
    return QStringLiteral(R"((?<dt>\d{1,2}[./-]\d{1,2}[./-]\d{2,4}(?:[ ,T]+\d{1,2}:\d{2}(?::\d{2})?)?))");
}

QString placeholder(const QString &name, const Vocabulary &v, const BankProfile &bank)
{
    if (name == QLatin1String("BANK")) {
        QStringList words;
        for (const QString &w : bank.brandKeywords)
            words << RE::escape(w);
        return QStringLiteral("(?:%1)").arg(words.join(QLatin1Char('|')));
    }
    if (name == QLatin1String("KIND"))
        return QStringLiteral(R"((?<kind>[\p{L}][\p{L}\-/ ]{1,30}?))");
    if (name == QLatin1String("AMOUNT"))
        return QStringLiteral("(?<amount>%1)").arg(text::numberPattern());
    if (name == QLatin1String("CUR"))
        return QStringLiteral("(?<cur>%1)").arg(text::currencyPattern());
    if (name == QLatin1String("MERCHANT"))
        return QStringLiteral(R"((?<merchant>[^\n,;]+?))");
    if (name == QLatin1String("REST"))
        return QStringLiteral(R"((?<merchant>[^\n]+?))");
    if (name == QLatin1String("CARDWORD"))
        return QStringLiteral("(?:%1)").arg(v.card);
    if (name == QLatin1String("CARD"))
        return QStringLiteral(R"([*•xX#]*[ -]?(?<card>\d{4}))");
    if (name == QLatin1String("DT"))
        return dateTimePattern();
    if (name == QLatin1String("BALWORD"))
        return QStringLiteral("(?:%1)").arg(v.balance);
    if (name == QLatin1String("BAL"))
        return QStringLiteral("(?<bal>%1)[ ]*(?<balcur>%2)?").arg(text::numberPattern(), text::currencyPattern());
    if (name == QLatin1String("SEP"))
        return QStringLiteral(R"([ ]*[:,\-]?[ ]*)");
    if (name == QLatin1String("EOL"))
        return QStringLiteral(R"([ ]*(?:\n|\z))");
    qCWarning(lcParser) << "unknown template placeholder" << name;
    return {};
}

QString literal(const QString &text)
{
    QString out;
    for (const QChar c : text) {
        if (c == QLatin1Char(' '))
            out += QStringLiteral("[ ]*");
        else if (c == QLatin1Char('\n'))
            out += QStringLiteral("[ ]*\\n[ ]*");
        else
            out += RE::escape(QString(c));
    }
    return out;
}

} // namespace

QList<SmsTemplate> buildTemplates(const QString &idPrefix, const QString &layout, const BankProfile &bank)
{
    QList<SmsTemplate> templates;
    for (const Vocabulary &v : vocabularies()) {
        QString pattern;
        qsizetype i = 0;
        QString pendingLiteral;
        const auto flush = [&] {
            pattern += literal(pendingLiteral);
            pendingLiteral.clear();
        };
        while (i < layout.size()) {
            if (layout.mid(i, 2) == QLatin1String("[[")) {
                flush();
                pattern += QStringLiteral("(?:");
                i += 2;
            } else if (layout.mid(i, 2) == QLatin1String("]]")) {
                flush();
                pattern += QStringLiteral(")?");
                i += 2;
            } else if (layout.at(i) == QLatin1Char('{')) {
                const qsizetype close = layout.indexOf(QLatin1Char('}'), i);
                if (close < 0) {
                    qCWarning(lcParser) << "unterminated placeholder in layout" << layout;
                    break;
                }
                flush();
                pattern += placeholder(layout.mid(i + 1, close - i - 1), v, bank);
                i = close + 1;
            } else {
                pendingLiteral += layout.at(i);
                ++i;
            }
        }
        flush();

        SmsTemplate t;
        t.id = QStringLiteral("%1-%2").arg(idPrefix, v.key);
        t.language = v.language;
        t.regex = RE(pattern, kOpts);
        if (!t.regex.isValid()) {
            qCWarning(lcParser) << "invalid template" << t.id << t.regex.errorString() << pattern;
            continue;
        }
        templates.append(t);
    }
    return templates;
}

/// A single card operation of a billion or more (in major units) is far more likely a
/// mis-read number than a real purchase: such a message is sent to review, never trusted.
bool isImplausibleAmount(const Money &amount)
{
    qint64 perMajor = 1;
    for (int i = 0; i < amount.exponent(); ++i)
        perMajor *= 10;
    return amount.minorUnits() / perMajor >= 1'000'000'000;
}

int scoreConfidence(const ParsedBankSms &p, bool fromTemplate)
{
    int score = fromTemplate ? 92 : 62;
    if (p.warnings.contains(QStringLiteral("ambiguous_amount")))
        score -= 35;
    if (p.warnings.contains(QStringLiteral("ambiguous_direction")))
        score -= 30;
    if (p.merchant.isEmpty())
        score -= 10;
    if (p.cardLastFour.isEmpty())
        score -= 5;
    if (!p.timeFromMessage)
        score -= 5;
    if (p.warnings.contains(QStringLiteral("time_in_future")))
        score -= 15;
    if (p.warnings.contains(QStringLiteral("implausible_amount")))
        score -= 40;
    return qBound(0, score, 100);
}

TemplateSmsParser::TemplateSmsParser(BankProfile profile, QList<SmsTemplate> templates)
    : m_profile(std::move(profile))
    , m_templates(std::move(templates))
{
}

int TemplateSmsParser::matchScore(const IncomingSms &sms) const
{
    const QString sender = text::foldForMatching(sms.sender);
    for (const QString &id : m_profile.senderIds) {
        if (!id.isEmpty() && sender == id)
            return 100;
    }
    const QString body = text::foldForMatching(sms.body);
    for (const QString &w : m_profile.brandKeywords) {
        if (!w.isEmpty() && (body.contains(w) || (!sender.isEmpty() && sender.contains(w))))
            return 90;
    }
    return 0;
}

void TemplateSmsParser::finish(ParsedBankSms &p, bool fromTemplate) const
{
    p.bankId = m_profile.id;
    p.bankName = m_profile.name;
    p.confidence = scoreConfidence(p, fromTemplate);
}

std::optional<ParsedBankSms> TemplateSmsParser::parseTemplate(const SmsTemplate &t, const QString &text,
                                                              const QDateTime &received) const
{
    const QRegularExpressionMatch m = t.regex.match(text);
    if (!m.hasMatch())
        return std::nullopt;

    QString kind = m.captured(QStringLiteral("kind")).trimmed();
    // "Purchase -500.00 AMD": the sign belongs to the amount, not to the operation word.
    while (!kind.isEmpty() && (kind.back().isSpace() || QStringLiteral(":-+\u2013\u2212").contains(kind.back())))
        kind.chop(1);
    text::Direction direction;
    if (!kind.isEmpty())
        direction = text::detectDirection(kind);
    if (!direction.found)
        return std::nullopt; // wording we do not know: let the generic extractor have a go

    const QString currency = text::currencyFromToken(m.captured(QStringLiteral("cur")));
    bool ambiguous = false;
    const auto amount = text::parseAmount(m.captured(QStringLiteral("amount")), currency, &ambiguous);
    if (!amount || !amount->isPositive())
        return std::nullopt;

    ParsedBankSms p;
    p.parserId = QStringLiteral("%1/%2").arg(m_profile.id, t.id);
    p.language = t.language;
    p.type = direction.type;
    p.isReversal = direction.reversal;
    p.kindText = kind;
    p.amount = *amount;
    if (ambiguous)
        p.warnings << QStringLiteral("ambiguous_amount");
    if (isImplausibleAmount(*amount))
        p.warnings << QStringLiteral("implausible_amount");
    if (direction.ambiguous)
        p.warnings << QStringLiteral("ambiguous_direction");
    p.merchant = text::cleanMerchantCandidate(m.captured(QStringLiteral("merchant")));
    if (p.merchant.isEmpty())
        p.warnings << QStringLiteral("no_merchant");
    p.cardLastFour = m.captured(QStringLiteral("card"));

    const QString dt = m.captured(QStringLiteral("dt"));
    if (const auto when = dt.isEmpty() ? std::nullopt : text::findDateTime(dt, received); when && when->hasDate) {
        p.occurredAt = when->value;
        p.timeFromMessage = true;
    } else {
        p.occurredAt = received;
    }

    const QString bal = m.captured(QStringLiteral("bal"));
    if (!bal.isEmpty()) {
        const QString balCur = text::currencyFromToken(m.captured(QStringLiteral("balcur")));
        bool balAmbiguous = false;
        if (const auto b = text::parseAmount(bal, balCur.isEmpty() ? currency : balCur, &balAmbiguous); b && !balAmbiguous)
            p.balance = *b;
    }
    finish(p, true);
    return p;
}

ParseOutcome TemplateSmsParser::parseGeneric(const QString &body, const QDateTime &received) const
{
    const QList<text::MoneyMention> mentions = text::findMoneyMentions(body);

    // Pick the operation amount: skip balances and fees; prefer a labelled "Amount".
    const text::MoneyMention *chosen = nullptr;
    for (const auto &m : mentions) {
        if (m.isBalance || m.isFee)
            continue;
        if (m.isLabelledAmount) {
            chosen = &m;
            break;
        }
        if (!chosen)
            chosen = &m;
    }
    if (!chosen) {
        // Information and promotions have no operation word. A message that does have one and also
        // contains numbers is probably a real transaction in a format (or currency) we cannot read:
        // report it as a failure so that it shows up in the errors list instead of vanishing.
        bool hasNumber = false;
        for (const auto &n : text::findMoneyMentions(body, /*requireCurrency=*/false))
            hasNumber = hasNumber || (!n.isBalance && !n.isFee);
        if (hasNumber && text::detectDirection(body).found) {
            return ParseOutcome::failure(errorcode::NoAmount,
                                         QObject::tr("An operation was found, but no amount in a supported currency."));
        }
        return ParseOutcome::failure(errorcode::NotATransaction,
                                     QObject::tr("No amount with a currency was found in the message."));
    }

    bool ambiguous = false;
    const auto amount = text::parseAmount(chosen->numberText, chosen->currency, &ambiguous);
    if (!amount)
        return ParseOutcome::failure(errorcode::InvalidAmount, QObject::tr("The amount could not be read."));
    if (!amount->isPositive())
        return ParseOutcome::failure(errorcode::InvalidAmount, QObject::tr("The amount must be greater than zero."));

    text::Direction direction = text::detectDirection(body);
    if (!direction.found) {
        if (chosen->signNegative) {
            direction.found = true;
            direction.type = TransactionType::Expense;
        } else if (chosen->signPositive) {
            direction.found = true;
            direction.type = TransactionType::Income;
        }
    }
    if (!direction.found) {
        return ParseOutcome::failure(errorcode::NoDirection,
                                     QObject::tr("Could not tell whether this is income or an expense."));
    }

    ParsedBankSms p;
    p.parserId = QStringLiteral("%1/generic").arg(m_profile.id);
    p.language = text::detectLanguage(body);
    p.type = direction.type;
    p.isReversal = direction.reversal;
    p.kindText = direction.keyword;
    p.amount = *amount;
    if (ambiguous)
        p.warnings << QStringLiteral("ambiguous_amount");
    if (isImplausibleAmount(*amount))
        p.warnings << QStringLiteral("implausible_amount");
    if (direction.ambiguous)
        p.warnings << QStringLiteral("ambiguous_direction");
    p.merchant = text::extractMerchant(body, chosen->end);
    if (p.merchant.isEmpty())
        p.warnings << QStringLiteral("no_merchant");
    p.cardLastFour = text::findCardLastFour(body);

    if (const auto when = text::findDateTime(body, received); when && when->hasDate) {
        p.occurredAt = when->value;
        p.timeFromMessage = true;
    } else if (when) {
        p.occurredAt = when->value; // time of day only
        p.timeFromMessage = true;
    } else {
        p.occurredAt = received;
    }

    for (const auto &m : mentions) {
        if (m.isBalance) {
            bool balAmbiguous = false;
            if (const auto b = text::parseAmount(m.numberText, m.currency, &balAmbiguous); b && !balAmbiguous) {
                p.balance = *b;
                break;
            }
        }
    }
    finish(p, false);
    return ParseOutcome::success(std::move(p));
}

ParseOutcome TemplateSmsParser::parse(const IncomingSms &sms) const
{
    const QString body = SmsSanitizer::clean(sms.body);
    if (body.isEmpty())
        return ParseOutcome::failure(errorcode::EmptyMessage, QObject::tr("The message is empty."));
    const QDateTime received = sms.receivedAt.isValid() ? sms.receivedAt : QDateTime::currentDateTime();

    ParseOutcome outcome;
    for (const SmsTemplate &t : m_templates) {
        if (auto p = parseTemplate(t, body, received)) {
            outcome = ParseOutcome::success(std::move(*p));
            break;
        }
    }
    if (!outcome.ok())
        outcome = parseGeneric(body, received);

    if (outcome.ok()) {
        ParsedBankSms &p = *outcome.parsed;
        p.bankId = m_profile.id;
        p.bankName = m_profile.name;
        // A date from the future means we mis-read it (or the phone clock is off).
        if (p.occurredAt > received.addDays(1)) {
            p.occurredAt = received;
            p.timeFromMessage = false;
            p.warnings << QStringLiteral("time_in_future");
            p.confidence = scoreConfidence(p, p.parserId.endsWith(QLatin1String("generic")) ? false : true);
        }
    }
    return outcome;
}

} // namespace pf::sms
