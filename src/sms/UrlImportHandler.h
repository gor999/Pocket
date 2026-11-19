#pragma once

#include "sms/SmsTypes.h"

#include <QUrl>

#include <optional>

namespace pf::sms {

/// Tier B hand-over: the Shortcut opens
///   personalfinance://import?body=<text>&sender=<id>&bank=<parser id>&ts=<unix seconds>
/// The app comes to the foreground (iOS cannot do this silently) and the message
/// is processed. Any app or web page can open such a URL, so these messages are
/// marked as an untrusted channel and are routed to review unless the user opted in.
class UrlImportHandler
{
public:
    static constexpr auto kScheme = "personalfinance";
    static constexpr int kMaxUrlBodyChars = 2000;

    /// nullopt when the URL is not an import URL at all or has no usable body.
    static std::optional<IncomingSms> parse(const QUrl &url);
};

} // namespace pf::sms
