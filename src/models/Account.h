#pragma once

#include "core/Money.h"
#include "models/Enums.h"

#include <QDateTime>
#include <QString>

namespace pf {

/// A bank account, card or cash wallet.
///
/// SECURITY: only the last four digits of a card may ever be stored. Full card
/// numbers, CVV, PINs and banking passwords are never accepted (see
/// AccountService::validate and the CHECK constraint on accounts.last_four).
struct Account
{
    qint64 id = 0;
    QString bankName;
    QString name;
    CardNetwork cardNetwork = CardNetwork::None;
    QString lastFour; // empty or exactly 4 digits
    Money initialBalance; // also carries the account currency
    bool archived = false;
    int sortOrder = 0;
    QDateTime createdAt;

    QString currency() const { return initialBalance.currency(); }
};

/// Account plus values derived from its transactions.
struct AccountSummary
{
    Account account;
    Money balance; // initial balance + income - expenses (exact integer math)
    int transactionCount = 0;
};

} // namespace pf
