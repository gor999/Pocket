#pragma once

#include "sms/SmsTypes.h"

#include <QString>
#include <QStringList>

namespace pf::sms {

/// One bank's SMS format(s). To support another bank, implement this interface
/// (usually by deriving from TemplateSmsParser) and register it in
/// ParserRegistry::withBuiltIns(). Parsers are stateless and thread-safe.
class BankSmsParser
{
public:
    virtual ~BankSmsParser() = default;

    /// Stable id used in the database, in settings and by the Shortcuts action ("idbank").
    virtual QString id() const = 0;
    /// Name used for accounts created from this bank's messages ("IDBank").
    virtual QString bankName() const = 0;
    /// Phrases that appear in this bank's SMS; suggested to the user as the
    /// "Message Contains" filter of the Shortcuts automation.
    virtual QStringList filterPhrases() const = 0;

    /// 0..100: how likely is it that `sms` came from this bank (sender and text).
    virtual int matchScore(const IncomingSms &sms) const = 0;

    /// Extracts the transaction. Never throws; failure is reported in the outcome.
    virtual ParseOutcome parse(const IncomingSms &sms) const = 0;
};

} // namespace pf::sms
