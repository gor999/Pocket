#include "services/SmsImportService.h"

#include "categorization/MerchantNormalizer.h"
#include "sms/SmsSanitizer.h"
#include "sms/SmsText.h"

#include <QCryptographicHash>
#include <QLoggingCategory>

namespace pf {

namespace {

Q_LOGGING_CATEGORY(lcImport, "pf.sms.import")

QString hashOf(const QString &text)
{
    const QString folded = text.toLower().simplified();
    return QString::fromLatin1(QCryptographicHash::hash(folded.toUtf8(), QCryptographicHash::Sha256).toHex());
}

/// "ACBA Bank" and "ACBA" are the same bank for account matching.
QString foldBank(QString s)
{
    s = s.toLower();
    s.remove(QLatin1Char(' '));
    if (s.endsWith(QLatin1String("bank")) && s.size() > 4)
        s.chop(4);
    return s;
}

QString safeChannel(const QString &c)
{
    if (c == sms::channel::ShortcutIntent || c == sms::channel::UrlScheme || c == sms::channel::DesktopFolder)
        return c;
    return QStringLiteral("other");
}

QString safeHint(const QString &hint)
{
    QString out;
    for (const QChar c : hint.trimmed().toLower()) {
        if (c.isLetterOrNumber() && c.unicode() < 128)
            out.append(c);
        else if (c == QLatin1Char('-') || c == QLatin1Char('_'))
            out.append(c);
    }
    return out.left(32);
}

bool categoryFits(const Category &c, TransactionType type)
{
    return c.kind == CategoryKind::Any || (c.kind == CategoryKind::Income) == (type == TransactionType::Income);
}

} // namespace

SmsImportService::SmsImportService(DatabaseManager &db, SmsImportRepository &imports, TransactionService &transactions,
                                   TransactionRepository &transactionRepo, AccountRepository &accounts,
                                   AccountService &accountService, CategoryRepository &categories,
                                   CategorizationService &categorization, SettingsRepository &settingsRepo,
                                   SettingsManager &settings, sms::ParserRegistry registry, QObject *parent)
    : QObject(parent)
    , m_db(db)
    , m_imports(imports)
    , m_transactions(transactions)
    , m_transactionRepo(transactionRepo)
    , m_accounts(accounts)
    , m_accountService(accountService)
    , m_categories(categories)
    , m_categorization(categorization)
    , m_settingsRepo(settingsRepo)
    , m_settings(settings)
    , m_registry(std::move(registry))
{
}

bool SmsImportService::flag(const QString &key, bool fallback) const
{
    const QString v = m_settingsRepo.value(key, fallback ? QStringLiteral("1") : QStringLiteral("0"));
    return v == QLatin1String("1");
}

bool SmsImportService::bankEnabled(const QString &bankId) const
{
    return flag(bankEnabledKey(bankId), true);
}

QString SmsImportService::errorText(const QString &code, const QString &fallback) const
{
    using namespace sms::errorcode;
    if (code == EmptyMessage) return tr("The message was empty.");
    if (code == TooLong) return tr("The message was too long to be a bank notification.");
    if (code == UnknownBank) return tr("The message is not from a supported bank.");
    if (code == NotATransaction) return tr("Not a transaction message.");
    if (code == NoAmount) return tr("No amount was found.");
    if (code == InvalidAmount) return tr("The amount could not be read.");
    if (code == UnsupportedCurrency) return tr("The currency is not supported.");
    if (code == NoDirection) return tr("Could not tell whether this is income or an expense.");
    if (code == BankDisabled) return tr("Import is switched off for this bank.");
    if (code == NoAccount) return tr("No matching account, and automatic account creation is off.");
    if (code == StorageError) return tr("The message could not be saved.");
    return fallback;
}

ImportResult SmsImportService::fail(const QString &message)
{
    qCWarning(lcImport) << "storage failure:" << message;
    ImportResult r;
    r.status = sms::status::Rejected;
    r.errorCode = sms::errorcode::StorageError;
    r.message = message;
    r.handled = false; // keep the source; try again later
    return r;
}

ImportResult SmsImportService::record(SmsImportRecord rec, const QString &status, const QString &errorCode,
                                      const QString &message)
{
    rec.status = status;
    rec.errorCode = errorCode;
    rec.errorMessage = message.isEmpty() ? errorText(errorCode, QString()) : message;
    const auto inserted = m_imports.insert(rec);
    if (!inserted)
        return fail(inserted.error());
    ImportResult r;
    r.status = status;
    r.smsImportId = inserted.value().id;
    r.transactionId = rec.transactionId.value_or(0);
    r.errorCode = errorCode;
    r.message = rec.errorMessage;
    r.reviewReason = rec.reviewReason;
    emit changed();
    return r;
}

Result<Account> SmsImportService::resolveAccount(const sms::ParsedBankSms &p, bool *created, bool *ambiguous)
{
    *created = false;
    *ambiguous = false;
    const auto summaries = m_accounts.summaries(true);
    if (!summaries)
        return summaries.status();

    const QString bank = foldBank(p.bankName);
    QList<Account> candidates;
    for (const AccountSummary &s : summaries.value()) {
        const Account &a = s.account;
        if (foldBank(a.bankName) == bank && a.currency() == p.amount.currency())
            candidates << a;
    }
    // Prefer active accounts, but an archived account that matches the card still wins over a new one.
    const auto pick = [&](bool requireCard) -> std::optional<Account> {
        for (const Account &a : std::as_const(candidates)) {
            if (requireCard && a.lastFour != p.cardLastFour)
                continue;
            if (!a.archived)
                return a;
        }
        for (const Account &a : std::as_const(candidates)) {
            if (requireCard && a.lastFour != p.cardLastFour)
                continue;
            return a;
        }
        return std::nullopt;
    };

    if (!p.cardLastFour.isEmpty()) {
        if (const auto a = pick(true))
            return *a;
    } else {
        QList<Account> active;
        for (const Account &a : std::as_const(candidates)) {
            if (!a.archived)
                active << a;
        }
        if (active.size() == 1)
            return active.first();
        if (active.size() > 1) {
            *ambiguous = true;
            return active.first();
        }
    }

    if (!flag(kAutoCreateAccounts, true))
        return Result<Account>::failure(errorText(sms::errorcode::NoAccount, QString()));

    Account a;
    a.bankName = p.bankName;
    a.name = p.cardLastFour.isEmpty() ? p.bankName : QStringLiteral("%1 ••%2").arg(p.bankName, p.cardLastFour);
    a.lastFour = p.cardLastFour;
    // If the SMS states the balance after this operation, start the account so that the
    // app's own balance matches it. Only trusted for a confident, template-based parse.
    qint64 initial = 0;
    if (p.balance && p.balance->currency() == p.amount.currency() && p.confidence >= 80) {
        const Money signedAmount = p.type == TransactionType::Income ? p.amount : -p.amount;
        initial = (*p.balance - signedAmount).minorUnits();
    }
    a.initialBalance = Money(initial, p.amount.currency());
    const auto createdAccount = m_accountService.create(a);
    if (!createdAccount)
        return createdAccount;
    *created = true;
    return createdAccount;
}

ImportResult SmsImportService::process(const sms::IncomingSms &incoming)
{
    using namespace sms;
    const QDateTime now = QDateTime::currentDateTime();

    SmsImportRecord rec;
    rec.externalId = incoming.externalId.trimmed().left(64);
    rec.channel = safeChannel(incoming.channel);
    rec.sender = SmsSanitizer::cleanSender(incoming.sender);
    rec.receivedAt = incoming.receivedAt.isValid() ? incoming.receivedAt : now;
    rec.importedAt = now;

    // 1) Same envelope delivered twice (crash before the file was removed, re-ingest): nothing to do.
    if (!rec.externalId.isEmpty()) {
        const auto previous = m_imports.byExternalId(rec.externalId);
        if (!previous)
            return fail(previous.error());
        if (previous.value()) {
            ImportResult r;
            r.status = previous.value()->status;
            r.smsImportId = previous.value()->id;
            r.alreadyProcessed = true;
            return r;
        }
    }

    // 2) Size and emptiness, before the text is looked at.
    if (incoming.body.size() > SmsSanitizer::kMaxBodyChars)
        return record(rec, status::Rejected, errorcode::TooLong, QString());
    const QString clean = SmsSanitizer::clean(incoming.body);
    if (clean.isEmpty())
        return record(rec, status::Rejected, errorcode::EmptyMessage, QString());

    // 3) The setup test: proves Shortcuts -> action -> inbox -> app works; creates no transaction.
    if (clean.startsWith(kTestMarker, Qt::CaseInsensitive)) {
        const auto s = m_settingsRepo.setValue(kLastTestAt, QString::number(QDateTime::currentSecsSinceEpoch()));
        Q_UNUSED(s)
        return record(rec, status::Ignored, QStringLiteral("test"), tr("Connection test received."));
    }

    // 4) One-time codes and other secrets: dropped, text never stored.
    if (SmsSanitizer::isSecretBearing(clean))
        return record(rec, status::Ignored, QStringLiteral("secret_ignored"), tr("Ignored: contains a one-time code or secret."));

    rec.body = SmsSanitizer::maskSensitive(clean);
    rec.bodyHash = hashOf(rec.body);

    IncomingSms msg;
    msg.body = clean;
    msg.sender = rec.sender;
    msg.receivedAt = rec.receivedAt;
    msg.channel = rec.channel;
    msg.bankHint = safeHint(incoming.bankHint);

    // 5) Which bank?
    const ParserRegistry::Identification ident = m_registry.identify(msg);
    if (!ident.parser) {
        const bool hasMoney = !text::findMoneyMentions(clean).isEmpty();
        if (!hasMoney)
            rec.body.clear();
        return record(rec, hasMoney ? status::Unrecognized : status::Ignored, errorcode::UnknownBank, QString());
    }
    rec.bankId = ident.parser->id();
    if (!bankEnabled(rec.bankId)) {
        rec.body.clear();
        return record(rec, status::Rejected, errorcode::BankDisabled, QString());
    }

    // 6) Parse.
    const ParseOutcome outcome = ident.parser->parse(msg);
    if (!outcome.ok()) {
        const bool notATransaction = outcome.errorCode == errorcode::NotATransaction;
        if (notATransaction)
            rec.body.clear(); // promotions / information: nothing worth keeping
        return record(rec, notATransaction ? status::Ignored : status::Unrecognized, outcome.errorCode,
                      outcome.errorMessage);
    }
    const ParsedBankSms p = *outcome.parsed;
    rec.parserId = p.parserId;
    rec.confidence = p.confidence;
    rec.balance = p.balance;

    // 7) Same text arrived moments ago (two automations matched, or a double trigger).
    {
        const auto same = m_imports.byBodyHash(rec.bodyHash, rec.receivedAt, kMessageDuplicateWindowSeconds);
        if (!same)
            return fail(same.error());
        for (const SmsImportRecord &previous : same.value()) {
            if (previous.transactionId) {
                rec.transactionId = previous.transactionId;
                return record(rec, status::Duplicate, QStringLiteral("same_message"),
                              tr("The same message was already imported."));
            }
        }
    }

    // 8) Account, category, transaction, and the log row, atomically.
    ImportResult result;
    QString failureCode;
    QString failureMessage;
    const Status stored = m_db.transaction([&]() -> Status {
        bool createdAccount = false;
        bool ambiguousAccount = false;
        const auto account = resolveAccount(p, &createdAccount, &ambiguousAccount);
        if (!account) {
            failureCode = errorcode::NoAccount;
            failureMessage = account.error();
            return account.status();
        }

        const NormalizedMerchant merchant = MerchantNormalizer::normalize(p.merchant);

        Transaction t;
        t.accountId = account.value().id;
        t.type = p.type;
        t.amount = p.amount;
        t.occurredAt = p.occurredAt;
        t.merchant = merchant.display;
        t.description = p.kindText;
        t.source = TransactionSource::BankSms;

        // Duplicates against existing transactions (from any source).
        const auto near = m_transactionRepo.findPotentialDuplicates(t, kNearDuplicateWindowSeconds);
        if (!near)
            return near.status();
        bool possibleDuplicate = false;
        for (const Transaction &other : near.value()) {
            if (other.fingerprint() == t.fingerprint()) {
                rec.transactionId = other.id;
                rec.status = status::Duplicate;
                rec.errorCode = QStringLiteral("same_transaction");
                rec.errorMessage = tr("A matching transaction already exists.");
                const auto inserted = m_imports.insert(rec);
                if (!inserted)
                    return inserted.status();
                result.status = status::Duplicate;
                result.smsImportId = inserted.value().id;
                result.transactionId = other.id;
                result.errorCode = rec.errorCode;
                result.message = rec.errorMessage;
                return Status::ok();
            }
            possibleDuplicate = true;
        }

        // Category: applied only when the rule is confident enough; otherwise suggested.
        const CategorySuggestion suggestion = m_categorization.classify(p.merchant, p.kindText, p.type);
        const bool autoCategory = suggestion.autoAssign(CategorizationService::kAutoAssignThreshold);
        if (autoCategory)
            t.categoryId = suggestion.categoryId;
        else if (suggestion.categoryId) {
            rec.suggestedCategoryId = suggestion.categoryId;
            rec.suggestionConfidence = suggestion.confidence;
        }

        // Anything that deserves a second look.
        QString reason;
        if (p.confidence < kReviewConfidenceFloor)
            reason = review::Parse;
        else if (ambiguousAccount)
            reason = review::Account;
        else if (possibleDuplicate)
            reason = review::PossibleDuplicate;
        else if (rec.channel == channel::UrlScheme && !flag(kTrustUrlChannel, false))
            reason = review::UntrustedChannel;
        else if (!autoCategory)
            reason = review::Category;

        const AddOutcome added = m_transactions.add(t, /*allowDuplicate=*/true);
        if (!added.saved) {
            failureCode = errorcode::StorageError;
            failureMessage = added.error;
            return Status::error(added.error);
        }

        rec.transactionId = added.transaction.id;
        rec.status = reason.isEmpty() ? status::Imported : status::NeedsReview;
        rec.reviewReason = reason;
        const auto inserted = m_imports.insert(rec);
        if (!inserted)
            return inserted.status();
        result.status = rec.status;
        result.smsImportId = inserted.value().id;
        result.transactionId = added.transaction.id;
        result.reviewReason = reason;
        return Status::ok();
    });

    if (!stored) {
        if (failureCode == errorcode::NoAccount) {
            rec.body.clear();
            return record(rec, status::Rejected, errorcode::NoAccount, failureMessage);
        }
        return fail(stored.message());
    }
    emit changed();
    return result;
}

// ---------------------------------------------------------------------------
// Review
// ---------------------------------------------------------------------------

QVariantMap SmsImportService::resolveReview(qint64 smsImportId, qint64 categoryId, bool rememberMerchant)
{
    const auto failure = [](const QString &m) {
        return QVariantMap{{QStringLiteral("ok"), false}, {QStringLiteral("error"), m}};
    };
    auto rec = m_imports.byId(smsImportId);
    if (!rec)
        return failure(rec.error());
    SmsImportRecord record = rec.value();
    if (record.status != sms::status::NeedsReview)
        return failure(tr("This item is not waiting for review."));

    QString merchant;
    if (record.transactionId) {
        auto tx = m_transactionRepo.byId(*record.transactionId);
        if (!tx)
            return failure(tr("The transaction no longer exists."));
        Transaction t = tx.value();
        merchant = t.merchant;
        if (categoryId > 0) {
            const auto category = m_categories.byId(categoryId);
            if (!category)
                return failure(tr("Choose a category."));
            if (!categoryFits(category.value(), t.type))
                return failure(tr("This category does not fit the transaction type."));
            t.categoryId = categoryId;
            if (const Status s = m_transactions.update(t); !s)
                return failure(s.message());
        }
    }

    record.status = sms::status::Imported;
    record.reviewReason.clear();
    record.suggestedCategoryId.reset();
    record.suggestionConfidence = 0;
    if (const Status s = m_imports.update(record); !s)
        return failure(s.message());

    if (rememberMerchant && categoryId > 0 && !merchant.isEmpty()) {
        const auto rule = m_categorization.rememberMerchant(merchant, categoryId);
        if (!rule)
            return failure(rule.error());
        applyUserRulesToPendingReviews();
    }
    emit changed();
    return {{QStringLiteral("ok"), true}};
}

void SmsImportService::applyUserRulesToPendingReviews()
{
    const auto pending = m_imports.list({sms::status::NeedsReview}, 500);
    if (!pending)
        return;
    for (SmsImportRecord record : pending.value()) {
        if (record.reviewReason != sms::review::Category || !record.transactionId)
            continue;
        auto tx = m_transactionRepo.byId(*record.transactionId);
        if (!tx || tx.value().categoryId)
            continue;
        Transaction t = tx.value();
        const CategorySuggestion s = m_categorization.classify(t.merchant, t.description, t.type);
        if (!s.autoAssign(CategorizationService::kAutoAssignThreshold))
            continue;
        t.categoryId = s.categoryId;
        if (!m_transactions.update(t))
            continue;
        record.status = sms::status::Imported;
        record.reviewReason.clear();
        record.suggestedCategoryId.reset();
        record.suggestionConfidence = 0;
        m_imports.update(record);
    }
}

QVariantMap SmsImportService::discardReview(qint64 smsImportId)
{
    const auto failure = [](const QString &m) {
        return QVariantMap{{QStringLiteral("ok"), false}, {QStringLiteral("error"), m}};
    };
    auto rec = m_imports.byId(smsImportId);
    if (!rec)
        return failure(rec.error());
    SmsImportRecord record = rec.value();
    if (record.transactionId) {
        if (const Status s = m_transactions.remove(*record.transactionId); !s)
            return failure(s.message());
        record.transactionId.reset();
    }
    record.status = sms::status::Duplicate;
    record.reviewReason.clear();
    record.errorCode = QStringLiteral("discarded");
    record.errorMessage = tr("Discarded by you.");
    if (const Status s = m_imports.update(record); !s)
        return failure(s.message());
    emit changed();
    return {{QStringLiteral("ok"), true}};
}

QVariantMap SmsImportService::dismissError(qint64 smsImportId)
{
    const Status s = m_imports.remove(smsImportId);
    if (!s)
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), s.message()}};
    emit changed();
    return {{QStringLiteral("ok"), true}};
}

void SmsImportService::clearHistory()
{
    m_imports.removeWithStatuses({sms::status::Duplicate, sms::status::Ignored, sms::status::Unrecognized,
                                  sms::status::Rejected});
    emit changed();
}

void SmsImportService::setBankEnabled(const QString &bankId, bool enabled)
{
    m_settingsRepo.setValue(bankEnabledKey(bankId), enabled ? QStringLiteral("1") : QStringLiteral("0"));
    emit changed();
}

void SmsImportService::setAutoCreateAccounts(bool enabled)
{
    m_settingsRepo.setValue(kAutoCreateAccounts, enabled ? QStringLiteral("1") : QStringLiteral("0"));
    emit changed();
}

void SmsImportService::setTrustUrlChannel(bool trust)
{
    m_settingsRepo.setValue(kTrustUrlChannel, trust ? QStringLiteral("1") : QStringLiteral("0"));
    emit changed();
}

int SmsImportService::ingestPending()
{
    return m_receiver ? m_receiver->processPending() : 0;
}

// ---------------------------------------------------------------------------
// QML facade
// ---------------------------------------------------------------------------

QVariantMap SmsImportService::itemMap(const SmsImportRecord &r) const
{
    QVariantMap m{
        {QStringLiteral("id"), qlonglong(r.id)},
        {QStringLiteral("status"), r.status},
        {QStringLiteral("reviewReason"), r.reviewReason},
        {QStringLiteral("bankId"), r.bankId},
        {QStringLiteral("channel"), r.channel},
        {QStringLiteral("confidence"), r.confidence},
        {QStringLiteral("receivedAt"), r.receivedAt},
        {QStringLiteral("importedAt"), r.importedAt},
        {QStringLiteral("errorCode"), r.errorCode},
        {QStringLiteral("errorMessage"), r.errorMessage},
        {QStringLiteral("snippet"), r.body.simplified().left(140)},
        {QStringLiteral("transactionId"), qlonglong(r.transactionId.value_or(0))},
        {QStringLiteral("suggestionConfidence"), r.suggestionConfidence},
    };

    QString bankName = r.bankId;
    if (const sms::BankSmsParser *p = m_registry.find(r.bankId))
        bankName = p->bankName();
    m.insert(QStringLiteral("bankName"), bankName);

    const auto describeCategory = [&](const QString &prefix, std::optional<qint64> id) {
        m.insert(prefix + QStringLiteral("Id"), qlonglong(id.value_or(0)));
        m.insert(prefix + QStringLiteral("Name"), QString());
        m.insert(prefix + QStringLiteral("Icon"), QString());
        m.insert(prefix + QStringLiteral("Color"), QString());
        if (!id)
            return;
        if (const auto c = m_categories.byId(*id)) {
            m.insert(prefix + QStringLiteral("Name"), c.value().displayName());
            m.insert(prefix + QStringLiteral("Icon"), c.value().icon);
            m.insert(prefix + QStringLiteral("Color"), c.value().color);
        }
    };

    std::optional<qint64> categoryId;
    m.insert(QStringLiteral("merchant"), QString());
    m.insert(QStringLiteral("type"), QString());
    m.insert(QStringLiteral("amountText"), QString());
    m.insert(QStringLiteral("occurredAt"), QDateTime());
    if (r.transactionId) {
        if (const auto tx = m_transactionRepo.byId(*r.transactionId)) {
            const Transaction &t = tx.value();
            categoryId = t.categoryId;
            m.insert(QStringLiteral("merchant"), t.merchant);
            m.insert(QStringLiteral("description"), t.description);
            m.insert(QStringLiteral("type"), toKey(t.type));
            m.insert(QStringLiteral("amountText"), t.amount.format(m_settings.locale()));
            m.insert(QStringLiteral("occurredAt"), t.occurredAt);
        }
    }
    describeCategory(QStringLiteral("category"), categoryId);
    describeCategory(QStringLiteral("suggested"), r.suggestedCategoryId);
    return m;
}

QVariantList SmsImportService::reviewItems() const
{
    QVariantList out;
    const auto list = m_imports.list({sms::status::NeedsReview}, 200);
    if (!list)
        return out;
    for (const SmsImportRecord &r : list.value()) {
        if (r.transactionId) // a deleted transaction leaves nothing to review
            out << itemMap(r);
    }
    return out;
}

QVariantList SmsImportService::history(int limit) const
{
    QVariantList out;
    const auto list = m_imports.list({sms::status::Imported, sms::status::NeedsReview, sms::status::Duplicate},
                                     qBound(1, limit, 500));
    if (!list)
        return out;
    for (const SmsImportRecord &r : list.value())
        out << itemMap(r);
    return out;
}

QVariantList SmsImportService::errors(int limit) const
{
    QVariantList out;
    const auto list = m_imports.list({sms::status::Unrecognized, sms::status::Rejected}, qBound(1, limit, 500));
    if (!list)
        return out;
    for (const SmsImportRecord &r : list.value())
        out << itemMap(r);
    return out;
}

QVariantList SmsImportService::banks() const
{
    QHash<QString, BankImportStats> stats;
    if (const auto s = m_imports.bankStats()) {
        for (const BankImportStats &b : s.value())
            stats.insert(b.bankId, b);
    }
    QVariantList out;
    for (const sms::BankSmsParser *p : m_registry.parsers()) {
        const BankImportStats b = stats.value(p->id());
        out << QVariantMap{
            {QStringLiteral("id"), p->id()},
            {QStringLiteral("name"), p->bankName()},
            {QStringLiteral("filterPhrases"), p->filterPhrases()},
            {QStringLiteral("enabled"), bankEnabled(p->id())},
            {QStringLiteral("receivedCount"), b.received},
            {QStringLiteral("importedCount"), b.imported},
            {QStringLiteral("lastReceivedAt"), b.lastReceivedAt},
            {QStringLiteral("seen"), b.received > 0},
        };
    }
    return out;
}

QVariantMap SmsImportService::summary() const
{
    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime lastMessage = m_imports.lastReceivedAt().valueOr(QDateTime());
    const QDateTime lastImport = m_imports.lastImportedAt().valueOr(QDateTime());
    const qint64 testEpoch = m_settingsRepo.value(kLastTestAt).toLongLong();
    const QDateTime lastTest = testEpoch > 0 ? QDateTime::fromSecsSinceEpoch(testEpoch) : QDateTime();

    QString level = QStringLiteral("notSetUp");
    if (lastMessage.isValid())
        level = lastMessage.daysTo(now) > kStaleAfterDays ? QStringLiteral("stale") : QStringLiteral("active");
    else if (lastTest.isValid())
        level = QStringLiteral("linkVerified");

    const auto count = [this](const QStringList &statuses) { return m_imports.count(statuses).valueOr(0); };

    QVariantMap m{
        {QStringLiteral("level"), level},
        {QStringLiteral("lastMessageAt"), lastMessage},
        {QStringLiteral("lastImportAt"), lastImport},
        {QStringLiteral("lastTestAt"), lastTest},
        {QStringLiteral("importedCount"), count({sms::status::Imported, sms::status::NeedsReview})},
        {QStringLiteral("needsReviewCount"), count({sms::status::NeedsReview})},
        {QStringLiteral("errorCount"), count({sms::status::Unrecognized, sms::status::Rejected})},
        {QStringLiteral("duplicateCount"), count({sms::status::Duplicate})},
        {QStringLiteral("ignoredCount"), count({sms::status::Ignored})},
        {QStringLiteral("autoCreateAccounts"), flag(kAutoCreateAccounts, true)},
        {QStringLiteral("trustUrlChannel"), flag(kTrustUrlChannel, false)},
        {QStringLiteral("receiverName"), QString()},
        {QStringLiteral("receiverAvailable"), false},
        {QStringLiteral("inboxLocation"), QString()},
        {QStringLiteral("receiverDetail"), QString()},
        {QStringLiteral("pendingFiles"), 0},
    };
    if (m_receiver) {
        const sms::ReceiverStatus s = m_receiver->status();
        m.insert(QStringLiteral("receiverName"), m_receiver->displayName());
        m.insert(QStringLiteral("receiverAvailable"), s.available);
        m.insert(QStringLiteral("inboxLocation"), s.location);
        m.insert(QStringLiteral("receiverDetail"), s.detail);
        m.insert(QStringLiteral("pendingFiles"), s.pending);
    }
    return m;
}

} // namespace pf
