#pragma once

#include "core/Money.h"

#include <QDateTime>
#include <QString>

#include <optional>

namespace pf {

/// Monthly spending limit. `categoryId` empty => overall monthly budget.
/// `period` is "YYYY-MM" for one specific month or empty for the recurring
/// default that applies to every month without its own entry.
/// (Schema exists since v1; BudgetService arrives in Phase 3.)
struct Budget
{
    qint64 id = 0;
    std::optional<qint64> categoryId;
    QString period;
    Money limit;
    QDateTime createdAt;
};

} // namespace pf
