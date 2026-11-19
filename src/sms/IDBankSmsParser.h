#pragma once

#include "sms/TemplateSmsParser.h"

namespace pf::sms {

/// IDBank. NOTE: the layouts below are SYNTHETIC. No real IDBank SMS was
/// available while this was written, so the templates model a typical
/// "operation, amount, place, card, time, balance" notification. Real messages
/// that do not match fall back to the generic extractor (-> review); add real
/// samples to tests/sms_samples and adjust the layouts.
class IDBankSmsParser : public TemplateSmsParser
{
public:
    IDBankSmsParser();
};

} // namespace pf::sms
