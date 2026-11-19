#include "sms/IDBankSmsParser.h"

namespace pf::sms {

namespace {

BankProfile profile()
{
    return {QStringLiteral("idbank"),
            QStringLiteral("IDBank"),
            {QStringLiteral("IDBank")},
            // brand words (lower-case): Latin, Armenian, Russian spellings
            {QStringLiteral("idbank"), QStringLiteral("id bank"), QStringLiteral("այդի բանկ"), QStringLiteral("айди банк")},
            {QStringLiteral("idbank")}};
}

QList<SmsTemplate> layouts(const BankProfile &p)
{
    QList<SmsTemplate> all;
    // With a place:   IDBank: Purchase 12,500.00 AMD, SAS SUPERMARKET, Card *1234, 15.11.2026 14:32, Balance: 245,000.00 AMD
    all += buildTemplates(QStringLiteral("place"),
                          QStringLiteral("[[{BANK}{SEP}]]{KIND} {AMOUNT} {CUR}, {MERCHANT}, {CARDWORD} {CARD}, {DT}[[, {BALWORD}: {BAL}]]"), p);
    // Without a place: IDBank: Credit 500,000.00 AMD, Card *1234, 01.11.2026 10:00, Balance: ...
    all += buildTemplates(QStringLiteral("noplace"),
                          QStringLiteral("[[{BANK}{SEP}]]{KIND} {AMOUNT} {CUR}, {CARDWORD} {CARD}, {DT}[[, {BALWORD}: {BAL}]]"), p);
    return all;
}

} // namespace

IDBankSmsParser::IDBankSmsParser()
    : TemplateSmsParser(profile(), layouts(profile()))
{
}

} // namespace pf::sms
