#pragma once

#include "categorization/CategorizationService.h"
#include "core/SettingsManager.h"
#include "database/AccountRepository.h"
#include "database/CategoryRepository.h"
#include "database/DatabaseManager.h"
#include "database/SettingsRepository.h"
#include "database/SmsImportRepository.h"
#include "database/TransactionRepository.h"
#include "services/AccountService.h"
#include "services/TransactionService.h"
#include "sms/IBankTransactionReceiver.h"
#include "sms/ParserRegistry.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace pf {

/// Result of handing one SMS to SmsImportService::process().
struct ImportResult
{
    QString status; // sms::status::* or "test"
    qint64 smsImportId = 0;
    qint64 transactionId = 0;
    QString reviewReason;
    QString errorCode;
    QString message;
    bool alreadyProcessed = false; // same envelope id seen before
    /// True when the outcome is durably recorded (any status, including rejected).
    /// False only when storage failed: the receiver must then keep the source.
    bool handled = true;
};

/// The import pipeline for bank SMS:
///   sanitize -> (test / secret / size checks) -> identify bank -> parse ->
///   duplicate checks -> resolve account -> categorize -> store transaction ->
///   record the outcome in `sms_imports`.
/// Everything happens on the device; nothing is sent anywhere.
///
/// Also the QML facade for the "Bank SMS import" screens.
class SmsImportService : public QObject
{
    Q_OBJECT

public:
    // settings keys (table `settings`)
    static inline const QString kAutoCreateAccounts = QStringLiteral("sms.autoCreateAccounts"); // "1" (default) / "0"
    static inline const QString kTrustUrlChannel = QStringLiteral("sms.trustUrlChannel"); // "0" (default) / "1"
    static inline const QString kLastTestAt = QStringLiteral("sms.lastTestAt"); // epoch seconds
    static QString bankEnabledKey(const QString &bankId) { return QStringLiteral("sms.bank.%1.enabled").arg(bankId); }

    static constexpr int kReviewConfidenceFloor = 60; // parse confidence below this -> review
    static constexpr int kMessageDuplicateWindowSeconds = 120;
    static constexpr int kNearDuplicateWindowSeconds = 600;
    static constexpr int kStaleAfterDays = 30;
    static inline const QString kTestMarker = QStringLiteral("PF-TEST");

    SmsImportService(DatabaseManager &db, SmsImportRepository &imports, TransactionService &transactions,
                     TransactionRepository &transactionRepo, AccountRepository &accounts, AccountService &accountService,
                     CategoryRepository &categories, CategorizationService &categorization,
                     SettingsRepository &settingsRepo, SettingsManager &settings, sms::ParserRegistry registry,
                     QObject *parent = nullptr);

    void setReceiver(sms::IBankTransactionReceiver *receiver) { m_receiver = receiver; }

    /// The pipeline. Safe to call with hostile input.
    ImportResult process(const sms::IncomingSms &incoming);

    const sms::ParserRegistry &registry() const { return m_registry; }

    // --- QML facade -----------------------------------------------------------
    /// {level: "notSetUp"|"linkVerified"|"active"|"stale", lastMessageAt, lastImportAt, lastTestAt,
    ///  receiverName, receiverAvailable, inboxLocation, receiverDetail, pendingFiles,
    ///  importedCount, needsReviewCount, errorCount, duplicateCount, ignoredCount,
    ///  autoCreateAccounts, trustUrlChannel}
    Q_INVOKABLE QVariantMap summary() const;
    /// [{id, name, filterPhrases, enabled, receivedCount, importedCount, lastReceivedAt, seen}]
    Q_INVOKABLE QVariantList banks() const;
    /// Transactions waiting for the user (see itemMap for the fields).
    Q_INVOKABLE QVariantList reviewItems() const;
    /// Latest imported/needs-review/duplicate messages.
    Q_INVOKABLE QVariantList history(int limit) const;
    /// Messages that could not be imported (unrecognized, rejected).
    Q_INVOKABLE QVariantList errors(int limit) const;

    /// Accepts a review item. `categoryId` 0 keeps the transaction uncategorized; with
    /// `rememberMerchant` a user rule is created and applied to other waiting items.
    Q_INVOKABLE QVariantMap resolveReview(qint64 smsImportId, qint64 categoryId, bool rememberMerchant);
    /// Deletes the imported transaction (e.g. a real duplicate) and closes the review item.
    Q_INVOKABLE QVariantMap discardReview(qint64 smsImportId);
    Q_INVOKABLE QVariantMap dismissError(qint64 smsImportId);
    Q_INVOKABLE void clearHistory(); // removes duplicate/ignored/unrecognized/rejected rows (not review items)

    Q_INVOKABLE void setBankEnabled(const QString &bankId, bool enabled);
    Q_INVOKABLE void setAutoCreateAccounts(bool enabled);
    Q_INVOKABLE void setTrustUrlChannel(bool trust);

    /// Processes everything waiting in the receiver's inbox (call when the app becomes active).
    Q_INVOKABLE int ingestPending();

signals:
    /// Anything visible in the SMS screens or in the transactions changed.
    void changed();

private:
    struct Prepared;
    QVariantMap itemMap(const SmsImportRecord &r) const;
    bool bankEnabled(const QString &bankId) const;
    bool flag(const QString &key, bool fallback) const;
    ImportResult record(SmsImportRecord rec, const QString &status, const QString &errorCode, const QString &message);
    ImportResult fail(const QString &message);
    Result<Account> resolveAccount(const sms::ParsedBankSms &p, bool *created, bool *ambiguous);
    void applyUserRulesToPendingReviews();
    QString errorText(const QString &code, const QString &fallback) const;

    DatabaseManager &m_db;
    SmsImportRepository &m_imports;
    TransactionService &m_transactions;
    TransactionRepository &m_transactionRepo;
    AccountRepository &m_accounts;
    AccountService &m_accountService;
    CategoryRepository &m_categories;
    CategorizationService &m_categorization;
    SettingsRepository &m_settingsRepo;
    SettingsManager &m_settings;
    sms::ParserRegistry m_registry;
    sms::IBankTransactionReceiver *m_receiver = nullptr;
};

} // namespace pf
