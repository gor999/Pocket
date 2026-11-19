#pragma once

#include <QString>

namespace pf {

enum class TransactionType { Expense, Income };

enum class TransactionSource {
    Manual,
    PastedNotification,
    CsvImport,
    DocumentImport,
    BankApi,
    BankSms, // imported automatically from a bank SMS (Shortcuts -> App Intent -> inbox)
};

enum class CardNetwork { None, Visa, Mastercard, Arca, AmericanExpress, Mir, UnionPay, Other };

enum class CategoryKind { Any, Income, Expense };

// Stable string keys persisted in SQLite. Never change an existing key.
QString toKey(TransactionType v);
QString toKey(TransactionSource v);
QString toKey(CardNetwork v);
QString toKey(CategoryKind v);

TransactionType transactionTypeFromKey(const QString &key, bool *ok = nullptr);
TransactionSource transactionSourceFromKey(const QString &key, bool *ok = nullptr);
CardNetwork cardNetworkFromKey(const QString &key, bool *ok = nullptr);
CategoryKind categoryKindFromKey(const QString &key, bool *ok = nullptr);

/// Human-readable, translatable label for a card network.
QString displayName(CardNetwork v);

} // namespace pf
