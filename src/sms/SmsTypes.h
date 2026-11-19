#pragma once

#include "core/Money.h"
#include "models/Enums.h"

#include <QDateTime>
#include <QString>
#include <QStringList>

#include <optional>

namespace pf::sms {

/// Where an SMS came from. The channel decides how far it is trusted.
namespace channel {
inline const QString ShortcutIntent = QStringLiteral("shortcut_intent"); // App Intents extension -> App Group inbox
inline const QString UrlScheme = QStringLiteral("url_scheme"); // personalfinance://import?... (any app can send this)
inline const QString DesktopFolder = QStringLiteral("desktop_folder"); // development / tests
} // namespace channel

/// One SMS as handed over by a receiver. Nothing here is trusted yet:
/// SmsSanitizer and the parsers validate it.
struct IncomingSms
{
    QString body;
    QString sender; // optional; many bank sender IDs cannot be picked in Shortcuts
    QDateTime receivedAt; // when the Shortcut ran (invalid => "now")
    QString channel = channel::ShortcutIntent;
    QString externalId; // envelope UUID; makes ingestion idempotent
    QString bankHint; // parser id chosen in the Shortcuts action ("idbank"); empty/"auto" => detect from the text
};

enum class Language { Unknown, English, Russian, Armenian };
QString languageKey(Language l);

/// What a parser extracted from one SMS. Money is always an integer minor-unit
/// amount (never a float). `confidence` (0..100) is how sure the *parse* is;
/// it is separate from how sure the *category* is.
struct ParsedBankSms
{
    QString bankId; // "idbank", "ameriabank", "acba"
    QString bankName; // "IDBank"
    QString parserId; // which template/extractor produced this
    Language language = Language::Unknown;

    TransactionType type = TransactionType::Expense;
    Money amount; // positive
    QString kindText; // operation word as written ("Purchase", "Գնում", ...)
    QString merchant; // raw, as written in the SMS
    QString cardLastFour; // exactly 4 digits or empty
    QDateTime occurredAt; // always valid; see timeFromMessage
    bool timeFromMessage = false;
    std::optional<Money> balance; // balance after the operation, if the SMS states one
    bool isReversal = false;

    int confidence = 0;
    QStringList warnings; // machine-readable: "ambiguous_amount", "ambiguous_direction", "no_merchant", ...
};

/// Parser result: either `parsed` or an error code (never both).
struct ParseOutcome
{
    std::optional<ParsedBankSms> parsed;
    QString errorCode; // see errorcode::
    QString errorMessage;

    bool ok() const { return parsed.has_value(); }
    static ParseOutcome success(ParsedBankSms p)
    {
        ParseOutcome o;
        o.parsed = std::move(p);
        return o;
    }
    static ParseOutcome failure(QString code, QString message)
    {
        ParseOutcome o;
        o.errorCode = std::move(code);
        o.errorMessage = std::move(message);
        return o;
    }
};

namespace errorcode {
inline const QString EmptyMessage = QStringLiteral("empty_message");
inline const QString TooLong = QStringLiteral("too_long");
inline const QString UnknownBank = QStringLiteral("unknown_bank");
inline const QString NotATransaction = QStringLiteral("not_a_transaction");
inline const QString NoAmount = QStringLiteral("no_amount");
inline const QString InvalidAmount = QStringLiteral("invalid_amount");
inline const QString UnsupportedCurrency = QStringLiteral("unsupported_currency");
inline const QString NoDirection = QStringLiteral("no_direction");
inline const QString BankDisabled = QStringLiteral("bank_disabled");
inline const QString NoAccount = QStringLiteral("no_account");
inline const QString StorageError = QStringLiteral("storage_error");
} // namespace errorcode

/// Persisted status of an SMS in `sms_imports` (stable keys).
namespace status {
inline const QString Imported = QStringLiteral("imported");
inline const QString NeedsReview = QStringLiteral("needs_review");
inline const QString Duplicate = QStringLiteral("duplicate");
inline const QString Ignored = QStringLiteral("ignored"); // OTP / promo / test: nothing stored
inline const QString Unrecognized = QStringLiteral("unrecognized"); // looked like a bank message, could not parse
inline const QString Rejected = QStringLiteral("rejected"); // invalid envelope / size / disabled bank
} // namespace status

/// Why a transaction is waiting for the user.
namespace review {
inline const QString Category = QStringLiteral("category"); // category confidence too low
inline const QString Parse = QStringLiteral("parse"); // parse confidence too low or ambiguous
inline const QString PossibleDuplicate = QStringLiteral("possible_duplicate");
inline const QString UntrustedChannel = QStringLiteral("untrusted_channel");
inline const QString Account = QStringLiteral("account"); // several accounts fit; one was chosen
} // namespace review

} // namespace pf::sms
