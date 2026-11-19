#pragma once

#include "core/Money.h"
#include "models/Enums.h"

#include <QDateTime>
#include <QString>

#include <optional>

namespace pf {

/// The standardized transaction every source (manual entry, pasted bank
/// notification, CSV row, future bank API) is converted into.
struct Transaction
{
    qint64 id = 0;
    qint64 accountId = 0;
    std::optional<qint64> categoryId; // empty => uncategorized (displayed as "Other")
    TransactionType type = TransactionType::Expense;
    Money amount; // always positive; direction comes from `type`
    QDateTime occurredAt;
    QString merchant;
    QString description;
    TransactionSource source = TransactionSource::Manual;
    std::optional<qint64> importId;
    QDateTime createdAt;
    QDateTime updatedAt;

    /// Amount with sign applied: income positive, expense negative.
    Money signedAmount() const { return type == TransactionType::Income ? amount : -amount; }

    /// SHA-256 over account, minute-resolution time, amount, type and the
    /// normalized merchant. Equal fingerprints => almost certainly the same
    /// real-world payment (used for duplicate detection).
    QString fingerprint() const;

    /// Lower-cased merchant + description. Stored in the database so search is
    /// case-insensitive for Armenian and Cyrillic text (SQLite's LIKE only
    /// folds ASCII).
    QString searchText() const;

    static QString normalizeText(const QString &text);
};

} // namespace pf
