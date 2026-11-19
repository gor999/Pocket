#include "models/Transaction.h"

#include <QCryptographicHash>

namespace pf {

QString Transaction::normalizeText(const QString &text)
{
    return text.simplified().toLower();
}

QString Transaction::fingerprint() const
{
    const qint64 minuteBucket = occurredAt.toSecsSinceEpoch() / 60;
    const QString payload = QStringLiteral("%1|%2|%3|%4|%5")
                                .arg(accountId)
                                .arg(minuteBucket)
                                .arg(amount.minorUnits())
                                .arg(toKey(type), normalizeText(merchant));
    const QByteArray digest = QCryptographicHash::hash(payload.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(digest.toHex());
}

QString Transaction::searchText() const
{
    return normalizeText(merchant + QLatin1Char(' ') + description);
}

} // namespace pf
