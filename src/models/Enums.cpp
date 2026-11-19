#include "models/Enums.h"

#include <QCoreApplication>

#include <iterator>

namespace pf {

namespace {

template<typename E>
struct KeyEntry
{
    E value;
    const char *key;
};

constexpr KeyEntry<TransactionType> kTypes[] = {
    {TransactionType::Expense, "expense"},
    {TransactionType::Income, "income"},
};

constexpr KeyEntry<TransactionSource> kSources[] = {
    {TransactionSource::Manual, "manual"},
    {TransactionSource::PastedNotification, "paste"},
    {TransactionSource::CsvImport, "csv"},
    {TransactionSource::DocumentImport, "document"},
    {TransactionSource::BankApi, "api"},
    {TransactionSource::BankSms, "bank_sms"},
};

constexpr KeyEntry<CardNetwork> kNetworks[] = {
    {CardNetwork::None, ""},
    {CardNetwork::Visa, "visa"},
    {CardNetwork::Mastercard, "mastercard"},
    {CardNetwork::Arca, "arca"},
    {CardNetwork::AmericanExpress, "amex"},
    {CardNetwork::Mir, "mir"},
    {CardNetwork::UnionPay, "unionpay"},
    {CardNetwork::Other, "other"},
};

constexpr KeyEntry<CategoryKind> kKinds[] = {
    {CategoryKind::Any, "any"},
    {CategoryKind::Income, "income"},
    {CategoryKind::Expense, "expense"},
};

template<typename E, size_t N>
QString keyOf(const KeyEntry<E> (&table)[N], E value)
{
    for (const auto &e : table) {
        if (e.value == value)
            return QString::fromLatin1(e.key);
    }
    return {};
}

template<typename E, size_t N>
E valueOf(const KeyEntry<E> (&table)[N], const QString &key, bool *ok)
{
    for (const auto &e : table) {
        if (key == QLatin1String(e.key)) {
            if (ok)
                *ok = true;
            return e.value;
        }
    }
    if (ok)
        *ok = false;
    return table[0].value;
}

} // namespace

QString toKey(TransactionType v) { return keyOf(kTypes, v); }
QString toKey(TransactionSource v) { return keyOf(kSources, v); }
QString toKey(CardNetwork v) { return keyOf(kNetworks, v); }
QString toKey(CategoryKind v) { return keyOf(kKinds, v); }

TransactionType transactionTypeFromKey(const QString &key, bool *ok) { return valueOf(kTypes, key, ok); }
TransactionSource transactionSourceFromKey(const QString &key, bool *ok) { return valueOf(kSources, key, ok); }
CardNetwork cardNetworkFromKey(const QString &key, bool *ok) { return valueOf(kNetworks, key, ok); }
CategoryKind categoryKindFromKey(const QString &key, bool *ok) { return valueOf(kKinds, key, ok); }

QString displayName(CardNetwork v)
{
    switch (v) {
    case CardNetwork::None: return QCoreApplication::translate("CardNetwork", "None");
    case CardNetwork::Visa: return QStringLiteral("Visa");
    case CardNetwork::Mastercard: return QStringLiteral("Mastercard");
    case CardNetwork::Arca: return QStringLiteral("ArCa");
    case CardNetwork::AmericanExpress: return QStringLiteral("American Express");
    case CardNetwork::Mir: return QStringLiteral("Mir");
    case CardNetwork::UnionPay: return QStringLiteral("UnionPay");
    case CardNetwork::Other: return QCoreApplication::translate("CardNetwork", "Other");
    }
    return {};
}

} // namespace pf
