#include "sms/ACBASmsParser.h"

namespace pf::sms {

namespace {

BankProfile profile()
{
    return {QStringLiteral("acba"),
            QStringLiteral("ACBA Bank"), // same spelling as AccountService::suggestedBanks()
            {QStringLiteral("ACBA")},
            {QStringLiteral("acba"), QStringLiteral("ակբա"), QStringLiteral("акба")},
            {QStringLiteral("acba")}};
}

QList<SmsTemplate> layouts(const BankProfile &p)
{
    QList<SmsTemplate> all;
    // ACBA: Card *1234 Purchase 12,500.00 AMD 15.11.2026 14:32 SAS SUPERMARKET Balance: 245,000.00 AMD
    // The variant without a place is tried first: otherwise a trailing "Balance: ..." would be read as the place.
    all += buildTemplates(QStringLiteral("noplace"),
                          QStringLiteral("[[{BANK}{SEP}]]{CARDWORD} {CARD} {KIND} {AMOUNT} {CUR} {DT}[[ {BALWORD}: {BAL}]]{EOL}"), p);
    all += buildTemplates(QStringLiteral("place"),
                          QStringLiteral("[[{BANK}{SEP}]]{CARDWORD} {CARD} {KIND} {AMOUNT} {CUR} {DT} {REST}[[ {BALWORD}: {BAL}]]{EOL}"), p);
    return all;
}

} // namespace

ACBASmsParser::ACBASmsParser()
    : TemplateSmsParser(profile(), layouts(profile()))
{
}

} // namespace pf::sms
