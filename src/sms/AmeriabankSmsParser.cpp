#include "sms/AmeriabankSmsParser.h"

namespace pf::sms {

namespace {

BankProfile profile()
{
    return {QStringLiteral("ameriabank"),
            QStringLiteral("Ameriabank"),
            {QStringLiteral("Ameriabank")},
            {QStringLiteral("ameriabank"), QStringLiteral("ameria"), QStringLiteral("ամերիաբանկ"), QStringLiteral("америабанк")},
            {QStringLiteral("ameriabank")}};
}

QList<SmsTemplate> layouts(const BankProfile &p)
{
    QList<SmsTemplate> all;
    // Purchase: 12,500.00 AMD
    // SAS SUPERMARKET
    // Card *1234 15.11.2026 14:32
    // Balance: 245,000.00 AMD
    all += buildTemplates(QStringLiteral("place"),
                          QStringLiteral("[[{BANK}\n]]{KIND}: {AMOUNT} {CUR}\n{MERCHANT}\n{CARDWORD} {CARD} {DT}[[\n{BALWORD}: {BAL}]]"), p);
    all += buildTemplates(QStringLiteral("noplace"),
                          QStringLiteral("[[{BANK}\n]]{KIND}: {AMOUNT} {CUR}\n{CARDWORD} {CARD} {DT}[[\n{BALWORD}: {BAL}]]"), p);
    return all;
}

} // namespace

AmeriabankSmsParser::AmeriabankSmsParser()
    : TemplateSmsParser(profile(), layouts(profile()))
{
}

} // namespace pf::sms
