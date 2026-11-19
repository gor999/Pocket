#include "services/AnalyticsService.h"

namespace pf {

AnalyticsService::AnalyticsService(TransactionRepository &transactions, SettingsManager &settings, QObject *parent)
    : QObject(parent)
    , m_transactions(transactions)
    , m_settings(settings)
{
}

QPair<QDateTime, QDateTime> AnalyticsService::monthRange(int year, int month)
{
    const QDate first(year, month, 1);
    return {QDateTime(first, QTime(0, 0)), QDateTime(first.addMonths(1), QTime(0, 0))};
}

Result<CurrencyTotals> AnalyticsService::monthTotals(int year, int month, const QString &currency,
                                                     std::optional<qint64> accountId) const
{
    const auto range = monthRange(year, month);
    const auto all = m_transactions.totals(range.first, range.second, accountId);
    if (!all)
        return all.status();
    for (const CurrencyTotals &t : all.value()) {
        if (t.currency == currency)
            return t;
    }
    CurrencyTotals empty;
    empty.currency = currency;
    empty.income = Money::zero(currency);
    empty.expense = Money::zero(currency);
    return empty;
}

QVariantMap AnalyticsService::monthSummary(const QString &currency) const
{
    const QDate today = QDate::currentDate();
    const auto totals = monthTotals(today.year(), today.month(), currency);
    const QLocale locale = m_settings.locale();

    Money income = Money::zero(currency);
    Money expense = Money::zero(currency);
    if (totals) {
        income = totals.value().income;
        expense = totals.value().expense;
    }
    const Money net = income - expense;
    return {
        {QStringLiteral("currency"), currency},
        {QStringLiteral("income"), income.format(locale)},
        {QStringLiteral("expense"), expense.format(locale)},
        {QStringLiteral("net"), net.format(locale)},
        {QStringLiteral("incomeMinor"), qlonglong(income.minorUnits())},
        {QStringLiteral("expenseMinor"), qlonglong(expense.minorUnits())},
        {QStringLiteral("netMinor"), qlonglong(net.minorUnits())},
    };
}

} // namespace pf
