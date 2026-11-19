#pragma once

#include "sms/TemplateSmsParser.h"

namespace pf::sms {

/// Ameriabank. SYNTHETIC multi-line layout (see IDBankSmsParser for why);
/// unfamiliar wording falls back to the generic extractor and goes to review.
class AmeriabankSmsParser : public TemplateSmsParser
{
public:
    AmeriabankSmsParser();
};

} // namespace pf::sms
