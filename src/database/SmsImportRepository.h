#pragma once

#include "core/Money.h"
#include "core/Result.h"
#include "database/DatabaseManager.h"

#include <QDateTime>
#include <QList>
#include <QStringList>

#include <optional>

namespace pf {

/// One row of `sms_imports`: what happened to one SMS handed over by Shortcuts.
struct SmsImportRecord
{
    qint64 id = 0;
    QString externalId;
    QString channel;
    QString sender;
    QString bankId;
    QString parserId;
    QString status; // sms::status::*
    QString reviewReason; // sms::review::* (status needs_review)
    QString errorCode; // sms::errorcode::*
    QString errorMessage;
    QString body; // sanitized; empty for ignored/secret messages
    QString bodyHash;
    int confidence = 0;
    std::optional<qint64> transactionId;
    std::optional<qint64> suggestedCategoryId;
    int suggestionConfidence = 0;
    std::optional<Money> balance;
    QDateTime receivedAt;
    QDateTime importedAt;
};

struct BankImportStats
{
    QString bankId;
    int received = 0; // every row for the bank
    int imported = 0; // imported + needs_review
    QDateTime lastReceivedAt;
};

class SmsImportRepository
{
public:
    explicit SmsImportRepository(const DatabaseManager &db) : m_db(db) {}

    Result<SmsImportRecord> insert(SmsImportRecord record) const;
    /// Rewrites every mutable column of the row with record.id.
    Status update(const SmsImportRecord &record) const;
    Status remove(qint64 id) const;
    /// Deletes all rows with one of the statuses (e.g. clearing the error list).
    Status removeWithStatuses(const QStringList &statuses) const;

    Result<SmsImportRecord> byId(qint64 id) const;
    Result<std::optional<SmsImportRecord>> byExternalId(const QString &externalId) const;
    /// Rows with the same body hash received within +/- windowSeconds of `around`.
    Result<QList<SmsImportRecord>> byBodyHash(const QString &hash, const QDateTime &around, int windowSeconds) const;

    /// Newest first. An empty `statuses` list means all statuses.
    Result<QList<SmsImportRecord>> list(const QStringList &statuses, int limit, int offset = 0) const;
    Result<int> count(const QStringList &statuses) const;

    Result<QList<BankImportStats>> bankStats() const;
    /// Most recent received_at among rows that came through a real channel (not the manual test).
    Result<QDateTime> lastReceivedAt() const;
    /// Most recent imported_at among rows with status imported / needs_review.
    Result<QDateTime> lastImportedAt() const;

private:
    const DatabaseManager &m_db;
};

} // namespace pf
