#include "database/Migrations.h"

#include "categorization/StarterRules.h"
#include "database/SqlUtil.h"
#include "models/Category.h"

#include <QDateTime>
#include <QSqlQuery>

namespace pf::schema {

namespace {

// Monetary values are INTEGER minor units (never REAL). Timestamps are INTEGER
// seconds since the Unix epoch (UTC). Foreign keys are enforced per connection.
const char *const kV1Statements[] = {
    R"(CREATE TABLE accounts (
        id                    INTEGER PRIMARY KEY AUTOINCREMENT,
        bank_name             TEXT    NOT NULL CHECK (length(trim(bank_name)) > 0),
        name                  TEXT    NOT NULL CHECK (length(trim(name)) > 0),
        currency              TEXT    NOT NULL CHECK (length(currency) = 3),
        card_network          TEXT    NOT NULL DEFAULT '',
        last_four             TEXT    CHECK (last_four IS NULL OR
                                             (length(last_four) = 4 AND last_four NOT GLOB '*[^0-9]*')),
        initial_balance_minor INTEGER NOT NULL DEFAULT 0,
        archived              INTEGER NOT NULL DEFAULT 0 CHECK (archived IN (0, 1)),
        sort_order            INTEGER NOT NULL DEFAULT 0,
        created_at            INTEGER NOT NULL
    ))",

    R"(CREATE TABLE categories (
        id         INTEGER PRIMARY KEY AUTOINCREMENT,
        key        TEXT    UNIQUE,
        name       TEXT    NOT NULL DEFAULT '',
        icon       TEXT    NOT NULL DEFAULT '',
        color      TEXT    NOT NULL DEFAULT '#8E8E93',
        kind       TEXT    NOT NULL DEFAULT 'any' CHECK (kind IN ('any', 'income', 'expense')),
        is_system  INTEGER NOT NULL DEFAULT 0 CHECK (is_system IN (0, 1)),
        sort_order INTEGER NOT NULL DEFAULT 0
    ))",

    R"(CREATE TABLE import_history (
        id            INTEGER PRIMARY KEY AUTOINCREMENT,
        source        TEXT    NOT NULL,
        file_name     TEXT    NOT NULL DEFAULT '',
        checksum      TEXT    NOT NULL DEFAULT '',
        account_id    INTEGER REFERENCES accounts(id) ON DELETE SET NULL,
        parser_id     TEXT    NOT NULL DEFAULT '',
        total_rows    INTEGER NOT NULL DEFAULT 0,
        imported_rows INTEGER NOT NULL DEFAULT 0,
        skipped_rows  INTEGER NOT NULL DEFAULT 0,
        imported_at   INTEGER NOT NULL
    ))",

    R"(CREATE TABLE transactions (
        id           INTEGER PRIMARY KEY AUTOINCREMENT,
        account_id   INTEGER NOT NULL REFERENCES accounts(id)   ON DELETE CASCADE,
        category_id  INTEGER          REFERENCES categories(id) ON DELETE SET NULL,
        import_id    INTEGER          REFERENCES import_history(id) ON DELETE SET NULL,
        type         TEXT    NOT NULL CHECK (type IN ('income', 'expense')),
        amount_minor INTEGER NOT NULL CHECK (amount_minor > 0),
        currency     TEXT    NOT NULL CHECK (length(currency) = 3),
        occurred_at  INTEGER NOT NULL,
        merchant     TEXT    NOT NULL DEFAULT '',
        description  TEXT    NOT NULL DEFAULT '',
        source       TEXT    NOT NULL DEFAULT 'manual',
        fingerprint  TEXT    NOT NULL,
        search_text  TEXT    NOT NULL DEFAULT '',
        created_at   INTEGER NOT NULL,
        updated_at   INTEGER NOT NULL
    ))",
    "CREATE INDEX idx_tx_account_time  ON transactions(account_id, occurred_at DESC)",
    "CREATE INDEX idx_tx_time          ON transactions(occurred_at DESC)",
    "CREATE INDEX idx_tx_category_time ON transactions(category_id, occurred_at)",
    "CREATE INDEX idx_tx_fingerprint   ON transactions(fingerprint)",
    "CREATE INDEX idx_tx_import        ON transactions(import_id)",

    R"(CREATE TABLE budgets (
        id          INTEGER PRIMARY KEY AUTOINCREMENT,
        category_id INTEGER REFERENCES categories(id) ON DELETE CASCADE,
        period      TEXT    NOT NULL DEFAULT '',
        limit_minor INTEGER NOT NULL CHECK (limit_minor > 0),
        currency    TEXT    NOT NULL CHECK (length(currency) = 3),
        created_at  INTEGER NOT NULL
    ))",
    // category_id may be NULL (overall budget); NULLs are distinct in plain
    // UNIQUE constraints, so uniqueness is enforced through an expression index.
    "CREATE UNIQUE INDEX idx_budget_unique ON budgets(COALESCE(category_id, 0), period, currency)",

    R"(CREATE TABLE categorization_rules (
        id          INTEGER PRIMARY KEY AUTOINCREMENT,
        category_id INTEGER NOT NULL REFERENCES categories(id) ON DELETE CASCADE,
        match_type  TEXT    NOT NULL DEFAULT 'contains' CHECK (match_type IN ('contains', 'exact', 'regex')),
        field       TEXT    NOT NULL DEFAULT 'any'      CHECK (field IN ('any', 'merchant', 'description')),
        pattern     TEXT    NOT NULL CHECK (length(pattern) > 0),
        priority    INTEGER NOT NULL DEFAULT 0,
        enabled     INTEGER NOT NULL DEFAULT 1 CHECK (enabled IN (0, 1)),
        is_system   INTEGER NOT NULL DEFAULT 0 CHECK (is_system IN (0, 1)),
        created_at  INTEGER NOT NULL
    ))",
    "CREATE INDEX idx_rules_priority ON categorization_rules(enabled, priority DESC)",

    R"(CREATE TABLE settings (
        key   TEXT PRIMARY KEY,
        value TEXT NOT NULL
    ))",
};

Status applyV1(QSqlDatabase &db)
{
    for (const char *statement : kV1Statements) {
        QSqlQuery q(db);
        if (!q.exec(QString::fromUtf8(statement)))
            return sql::failed(q, QStringLiteral("Schema v1"));
    }

    QSqlQuery insert(db);
    insert.prepare(QStringLiteral(
        "INSERT INTO categories (key, name, icon, color, kind, is_system, sort_order) "
        "VALUES (?, '', ?, ?, ?, 1, ?)"));
    for (const Category &c : Category::builtIn()) {
        insert.addBindValue(c.key);
        insert.addBindValue(c.icon);
        insert.addBindValue(c.color);
        insert.addBindValue(toKey(c.kind));
        insert.addBindValue(c.sortOrder);
        if (!insert.exec())
            return sql::failed(insert, QStringLiteral("Seeding categories"));
    }
    return Status::ok();
}

// ---------------------------------------------------------------------------
// v2: bank-SMS import
//   * categorization_rules.confidence (how sure a rule match makes us)
//   * `cash` built-in category (ATM withdrawals) + a reconcile step so existing
//     databases end up with exactly Category::builtIn()
//   * starter categorization rules (hy / ru / en keywords), editable by the user
//   * sms_imports: one row per SMS handed over by the Shortcuts automation
// ---------------------------------------------------------------------------
const char *const kV2Statements[] = {
    "ALTER TABLE categorization_rules ADD COLUMN confidence INTEGER NOT NULL DEFAULT 80",

    // Every SMS that reaches the app leaves one row, whatever its fate. The body is
    // stored only after sanitizing (card numbers / CVV / PIN masked) and never for
    // messages recognised as one-time codes or other secrets (those keep body = '').
    R"(CREATE TABLE sms_imports (
        id                    INTEGER PRIMARY KEY AUTOINCREMENT,
        external_id           TEXT    NOT NULL DEFAULT '',
        channel               TEXT    NOT NULL DEFAULT '',
        sender                TEXT    NOT NULL DEFAULT '',
        bank_id               TEXT    NOT NULL DEFAULT '',
        parser_id             TEXT    NOT NULL DEFAULT '',
        status                TEXT    NOT NULL CHECK (status IN
                                  ('imported', 'needs_review', 'duplicate', 'ignored', 'unrecognized', 'rejected')),
        review_reason         TEXT    NOT NULL DEFAULT '',
        error_code            TEXT    NOT NULL DEFAULT '',
        error_message         TEXT    NOT NULL DEFAULT '',
        body                  TEXT    NOT NULL DEFAULT '',
        body_hash             TEXT    NOT NULL DEFAULT '',
        confidence            INTEGER NOT NULL DEFAULT 0,
        transaction_id        INTEGER REFERENCES transactions(id) ON DELETE SET NULL,
        suggested_category_id INTEGER REFERENCES categories(id)   ON DELETE SET NULL,
        suggestion_confidence INTEGER NOT NULL DEFAULT 0,
        balance_minor         INTEGER,
        balance_currency      TEXT    NOT NULL DEFAULT '',
        received_at           INTEGER NOT NULL,
        imported_at           INTEGER NOT NULL
    ))",
    // Envelope ids make re-ingesting the same file harmless.
    "CREATE UNIQUE INDEX idx_sms_external ON sms_imports(external_id) WHERE external_id <> ''",
    "CREATE INDEX idx_sms_status_time ON sms_imports(status, imported_at DESC)",
    "CREATE INDEX idx_sms_hash        ON sms_imports(body_hash)",
    "CREATE INDEX idx_sms_tx          ON sms_imports(transaction_id)",
};

Status applyV2(QSqlDatabase &db)
{
    for (const char *statement : kV2Statements) {
        QSqlQuery q(db);
        if (!q.exec(QString::fromUtf8(statement)))
            return sql::failed(q, QStringLiteral("Schema v2"));
    }

    // Reconcile built-in categories: add the missing ones (cash) and keep the UI order.
    {
        QSqlQuery order(db);
        order.prepare(QStringLiteral("UPDATE categories SET sort_order = ? WHERE key = ?"));
        QSqlQuery insert(db);
        insert.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO categories (key, name, icon, color, kind, is_system, sort_order) "
            "VALUES (?, '', ?, ?, ?, 1, ?)"));
        for (const Category &c : Category::builtIn()) {
            order.addBindValue(c.sortOrder);
            order.addBindValue(c.key);
            if (!order.exec())
                return sql::failed(order, QStringLiteral("Reordering categories"));
            insert.addBindValue(c.key);
            insert.addBindValue(c.icon);
            insert.addBindValue(c.color);
            insert.addBindValue(toKey(c.kind));
            insert.addBindValue(c.sortOrder);
            if (!insert.exec())
                return sql::failed(insert, QStringLiteral("Seeding categories"));
        }
    }

    // Starter rules.
    QSqlQuery rule(db);
    rule.prepare(QStringLiteral(
        "INSERT INTO categorization_rules (category_id, match_type, field, pattern, priority, enabled, is_system, "
        "created_at, confidence) "
        "SELECT id, ?, 'any', ?, ?, 1, 1, ?, ? FROM categories WHERE key = ?"));
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    for (const StarterRule &r : starterRules()) {
        const bool regex = r.matchType == CategorizationRule::MatchType::Regex;
        rule.addBindValue(regex ? QStringLiteral("regex") : QStringLiteral("contains"));
        rule.addBindValue(QString::fromUtf8(r.pattern));
        rule.addBindValue(r.priority);
        rule.addBindValue(qlonglong(now));
        rule.addBindValue(r.confidence);
        rule.addBindValue(QString::fromLatin1(r.categoryKey));
        if (!rule.exec())
            return sql::failed(rule, QStringLiteral("Seeding categorization rules"));
    }
    return Status::ok();
}

} // namespace

const QList<Migration> &migrations()
{
    static const QList<Migration> list = {
        {1, applyV1},
        {2, applyV2},
    };
    return list;
}

} // namespace pf::schema
