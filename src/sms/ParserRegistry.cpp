#include "sms/ParserRegistry.h"

#include "sms/ACBASmsParser.h"
#include "sms/AmeriabankSmsParser.h"
#include "sms/IDBankSmsParser.h"

namespace pf::sms {

ParserRegistry ParserRegistry::withBuiltIns()
{
    ParserRegistry r;
    r.add(std::make_unique<IDBankSmsParser>());
    r.add(std::make_unique<AmeriabankSmsParser>());
    r.add(std::make_unique<ACBASmsParser>());
    return r;
}

void ParserRegistry::add(std::unique_ptr<BankSmsParser> parser)
{
    m_parsers.append(std::shared_ptr<BankSmsParser>(std::move(parser)));
}

QList<const BankSmsParser *> ParserRegistry::parsers() const
{
    QList<const BankSmsParser *> list;
    for (const auto &p : m_parsers)
        list.append(p.get());
    return list;
}

const BankSmsParser *ParserRegistry::find(const QString &id) const
{
    for (const auto &p : m_parsers) {
        if (p->id() == id)
            return p.get();
    }
    return nullptr;
}

ParserRegistry::Identification ParserRegistry::identify(const IncomingSms &sms) const
{
    Identification best;
    const QString hint = sms.bankHint.trimmed().toLower();
    if (!hint.isEmpty() && hint != QLatin1String("auto")) {
        if (const BankSmsParser *p = find(hint)) {
            best.parser = p;
            best.score = 100;
            best.fromHint = true;
            return best;
        }
        // An unknown hint is ignored rather than trusted; fall through to detection.
    }
    for (const auto &p : m_parsers) {
        const int score = p->matchScore(sms);
        if (score > best.score) {
            best.parser = p.get();
            best.score = score;
        }
    }
    if (best.score < kMinScore)
        return {};
    return best;
}

ParseOutcome ParserRegistry::parse(const IncomingSms &sms) const
{
    const Identification id = identify(sms);
    if (!id.parser) {
        return ParseOutcome::failure(errorcode::UnknownBank,
                                     QObject::tr("The message does not come from a supported bank."));
    }
    return id.parser->parse(sms);
}

} // namespace pf::sms
