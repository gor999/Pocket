#pragma once

#include "core/Result.h"
#include "core/SettingsManager.h"
#include "database/TransactionRepository.h"

#include <QDate>
#include <QObject>
#include <QVariantMap>

namespace pf {

/// Read-only reporting over transactions. All sums are exact integer minor
/// units grouped by currency. (Phase 1 has the monthly summary; daily series,
/// category breakdowns and month-over-month comparison arrive in Phase 3.)
class AnalyticsService : public QObject
{
    Q_OBJECT

public:
    AnalyticsService(TransactionRepository &transactions, SettingsManager &settings, QObject *parent = nullptr);

    /// Start (inclusive) and end (exclusive) of a calendar month in local time.
    static QPair<QDateTime, QDateTime> monthRange(int year, int month);

    Result<CurrencyTotals> monthTotals(int year, int month, const QString &currency,
                                       std::optional<qint64> accountId = std::nullopt) const;

    /// Summary of the *current* calendar month in `currency`:
    /// {income, expense, net (formatted text), incomeMinor, expenseMinor, netMinor}.
    Q_INVOKABLE QVariantMap monthSummary(const QString &currency) const;

private:
    TransactionRepository &m_transactions;
    SettingsManager &m_settings;
};

} // namespace pf
