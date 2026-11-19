#pragma once

#include "sms/BankSmsParser.h"

#include <QList>
#include <QRegularExpression>

namespace pf::sms {

/// Identity of a bank for matching purposes.
struct BankProfile
{
    QString id;
    QString name;
    QStringList filterPhrases;
    QStringList brandKeywords; // lower-case words that identify the bank inside a message
    QStringList senderIds; // lower-case sender IDs, if known
};

/// One message layout compiled to a regular expression with named groups:
/// kind, amount, cur, merchant, card, dt, bal, balcur (all optional except amount/cur).
struct SmsTemplate
{
    QString id;
    Language language = Language::Unknown;
    QRegularExpression regex;
};

/// Builds templates from a readable layout, e.g.
///   "{BANK}: {KIND} {AMOUNT} {CUR}, {MERCHANT}, {CARDWORD} {CARD}, {DT}[[, {BALWORD}: {BAL}]]"
/// Placeholders: {BANK} {KIND} {AMOUNT} {CUR} {MERCHANT} {REST} {CARDWORD} {CARD} {DT}
/// {BALWORD} {BAL} {SEP} {EOL}. "[[ ... ]]" marks an optional part. Spaces match any
/// run of blanks, "\n" matches a line break with surrounding blanks.
/// One template is produced per supported language (en, ru, hy).
QList<SmsTemplate> buildTemplates(const QString &idPrefix, const QString &layout, const BankProfile &bank);

/// Base for parsers that know a bank's message layouts. Order of work:
///   1. try the bank's templates (confidence ~90);
///   2. otherwise the bank-agnostic extractor (confidence ~60, usually ends up in review);
/// so an unfamiliar wording degrades to "needs review" instead of being lost.
class TemplateSmsParser : public BankSmsParser
{
public:
    TemplateSmsParser(BankProfile profile, QList<SmsTemplate> templates);

    QString id() const override { return m_profile.id; }
    QString bankName() const override { return m_profile.name; }
    QStringList filterPhrases() const override { return m_profile.filterPhrases; }
    int matchScore(const IncomingSms &sms) const override;
    ParseOutcome parse(const IncomingSms &sms) const override;

    const QList<SmsTemplate> &templates() const { return m_templates; }

private:
    ParseOutcome parseGeneric(const QString &text, const QDateTime &received) const;
    std::optional<ParsedBankSms> parseTemplate(const SmsTemplate &t, const QString &text, const QDateTime &received) const;
    void finish(ParsedBankSms &p, bool fromTemplate) const;

    BankProfile m_profile;
    QList<SmsTemplate> m_templates;
};

/// Applies the penalties/bonuses that turn "what was found" into a 0..100 confidence.
int scoreConfidence(const ParsedBankSms &p, bool fromTemplate);

} // namespace pf::sms
