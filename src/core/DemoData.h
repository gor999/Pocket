#pragma once

#include "core/ApplicationManager.h"

namespace pf {

/// Developer aid: fills an empty database with realistic sample accounts and
/// roughly 75 days of transactions (Armenian, Russian and English merchants)
/// so the UI can be previewed and screenshotted. Only invoked when the app is
/// started with PF_SEED_DEMO=1 in a PF_DEV_TOOLS build, and only on an empty
/// database. Deterministic: same data on every run.
class DemoData
{
public:
    static bool seed(ApplicationManager &app);

    /// Feeds a few SYNTHETIC bank SMS (invented layouts, not real bank formats) through the
    /// import pipeline so the "Bank SMS import" screens have something to show:
    /// auto-imported, needs-review, duplicate, ignored (OTP) and unrecognized messages.
    static void seedSms(ApplicationManager &app);
};

} // namespace pf
