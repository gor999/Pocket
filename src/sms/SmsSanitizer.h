#pragma once

#include <QString>

namespace pf::sms {

/// Input hygiene for text that arrives from outside the app. Every SMS passes
/// through here before it is parsed or stored.
///
/// SECURITY RULES enforced here:
///  * control / bidirectional-override characters are removed;
///  * the size is bounded (a hostile sender cannot make us store megabytes);
///  * anything that looks like a full card number is reduced to its last four
///    digits, and CVV/PIN values are masked, so they can never reach SQLite;
///  * messages that carry one-time codes or other secrets are recognised so the
///    caller can drop them *without storing their text*.
class SmsSanitizer
{
public:
    static constexpr int kMaxBodyChars = 2000; // ~ 3 concatenated Unicode SMS parts + slack
    static constexpr int kMaxSenderChars = 64;

    /// Removes control characters, normalizes line endings and spaces, trims.
    /// Idempotent.
    static QString clean(const QString &raw);

    /// clean() + maskSensitive(). This is the text that may be stored.
    static QString sanitizeForStorage(const QString &raw);

    /// Replaces 13-19 digit runs with "**** **** **** 1234" and masks CVV/PIN values.
    static QString maskSensitive(const QString &text);

    /// True for one-time-code / 3-D Secure / password / PIN / CVV messages.
    /// Such messages must be discarded without being stored.
    static bool isSecretBearing(const QString &text);

    /// Sender IDs are short labels; keep printable characters only.
    static QString cleanSender(const QString &raw);
};

} // namespace pf::sms
