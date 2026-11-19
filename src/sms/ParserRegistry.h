#pragma once

#include "sms/BankSmsParser.h"

#include <QList>

#include <memory>

namespace pf::sms {

/// The set of bank parsers and the logic that picks one for a message.
///
/// Selection order:
///  1. the `bankHint` the user set in the Shortcuts action (explicit, most reliable);
///  2. otherwise the parser with the best matchScore() (sender / bank name in the text),
///     if it reaches kMinScore;
///  3. otherwise the message is reported as coming from an unknown bank.
class ParserRegistry
{
public:
    static constexpr int kMinScore = 50;

    static ParserRegistry withBuiltIns();

    void add(std::unique_ptr<BankSmsParser> parser);
    QList<const BankSmsParser *> parsers() const;
    const BankSmsParser *find(const QString &id) const;

    struct Identification
    {
        const BankSmsParser *parser = nullptr;
        int score = 0;
        bool fromHint = false;
    };
    Identification identify(const IncomingSms &sms) const;

    /// identify() + parse(). An unidentifiable message yields errorcode::UnknownBank.
    ParseOutcome parse(const IncomingSms &sms) const;

private:
    QList<std::shared_ptr<BankSmsParser>> m_parsers;
};

} // namespace pf::sms
