#include "sms/SmsTypes.h"

namespace pf::sms {

QString languageKey(Language l)
{
    switch (l) {
    case Language::English: return QStringLiteral("en");
    case Language::Russian: return QStringLiteral("ru");
    case Language::Armenian: return QStringLiteral("hy");
    case Language::Unknown: break;
    }
    return {};
}

} // namespace pf::sms
