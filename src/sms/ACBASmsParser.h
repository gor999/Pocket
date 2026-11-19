#pragma once

#include "sms/TemplateSmsParser.h"

namespace pf::sms {

/// ACBA Bank. SYNTHETIC single-line layout that follows the fields ACBA's
/// notifications are described as containing (date, time, amount, place,
/// optional resulting balance); the exact wording is NOT verified.
class ACBASmsParser : public TemplateSmsParser
{
public:
    ACBASmsParser();
};

} // namespace pf::sms
