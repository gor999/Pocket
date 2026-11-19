#include "sms/UrlImportHandler.h"

#include <QUrlQuery>

namespace pf::sms {

std::optional<IncomingSms> UrlImportHandler::parse(const QUrl &url)
{
    if (url.scheme().compare(QLatin1String(kScheme), Qt::CaseInsensitive) != 0)
        return std::nullopt;
    // personalfinance://import?... (host) or personalfinance:import?... / personalfinance:///import (path)
    const QString target = !url.host().isEmpty() ? url.host() : url.path().section(QLatin1Char('/'), -1);
    if (target.compare(QLatin1String("import"), Qt::CaseInsensitive) != 0)
        return std::nullopt;

    const QUrlQuery query(url);
    const QString body = query.queryItemValue(QStringLiteral("body"), QUrl::FullyDecoded);
    if (body.trimmed().isEmpty())
        return std::nullopt;

    IncomingSms sms;
    sms.channel = channel::UrlScheme;
    sms.body = body.left(kMaxUrlBodyChars + 1); // one extra char so the size check can reject it
    sms.sender = query.queryItemValue(QStringLiteral("sender"), QUrl::FullyDecoded);
    sms.bankHint = query.queryItemValue(QStringLiteral("bank"), QUrl::FullyDecoded);

    bool ok = false;
    const qint64 ts = query.queryItemValue(QStringLiteral("ts")).toLongLong(&ok);
    if (ok && ts > 946684800) // after 2000-01-01
        sms.receivedAt = QDateTime::fromSecsSinceEpoch(ts);
    return sms;
}

} // namespace pf::sms
