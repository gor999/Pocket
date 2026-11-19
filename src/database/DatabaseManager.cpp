#include "database/DatabaseManager.h"

#include "database/Migrations.h"
#include "database/SqlUtil.h"

#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSqlError>
#include <QSqlQuery>

#include <atomic>

Q_LOGGING_CATEGORY(lcDatabase, "pf.database")

namespace pf {

namespace {
std::atomic<quint64> g_instanceCounter{0};
std::atomic<quint64> g_connectionCounter{0};
} // namespace

void DatabaseManager::ConnectionHolder::release()
{
    if (name.isEmpty())
        return;
    {
        QSqlDatabase db = QSqlDatabase::database(name, false);
        if (db.isValid() && db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase(name);
    name.clear();
    transactionDepth = 0;
}

DatabaseManager::DatabaseManager(QString databasePath)
    : m_path(std::move(databasePath))
    , m_instanceId(++g_instanceCounter)
{
}

DatabaseManager::~DatabaseManager()
{
    m_connections.localData().release();
}

QSqlDatabase DatabaseManager::connection() const
{
    ConnectionHolder &holder = m_connections.localData();
    if (!holder.name.isEmpty())
        return QSqlDatabase::database(holder.name, false);

    holder.name = QStringLiteral("pf_%1_%2").arg(m_instanceId).arg(++g_connectionCounter);
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), holder.name);
    db.setDatabaseName(m_path);
    db.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=5000"));
    if (!db.open()) {
        qCCritical(lcDatabase) << "Cannot open database" << m_path << db.lastError().text();
        return db;
    }

    QSqlQuery pragma(db);
    const char *const pragmas[] = {
        "PRAGMA foreign_keys = ON",
        "PRAGMA journal_mode = WAL",
        "PRAGMA synchronous = NORMAL",
        "PRAGMA temp_store = MEMORY",
    };
    for (const char *p : pragmas) {
        if (!pragma.exec(QLatin1String(p)))
            qCWarning(lcDatabase) << "Pragma failed:" << p << pragma.lastError().text();
        pragma.finish();
    }
    return db;
}

Status DatabaseManager::open()
{
    if (!QDir().mkpath(QFileInfo(m_path).absolutePath()))
        return Status::error(QStringLiteral("Cannot create database directory for %1").arg(m_path));

    QSqlDatabase db = connection();
    if (!db.isOpen())
        return Status::error(QStringLiteral("Cannot open database: %1").arg(db.lastError().text()));
    return migrate();
}

int DatabaseManager::schemaVersion() const
{
    QSqlQuery q(connection());
    if (q.exec(QStringLiteral("PRAGMA user_version")) && q.next())
        return q.value(0).toInt();
    return -1;
}

Status DatabaseManager::migrate()
{
    const int current = schemaVersion();
    if (current < 0)
        return Status::error(QStringLiteral("Cannot read database schema version"));

    const auto &all = schema::migrations();
    const int latest = all.constLast().version;
    if (current > latest) {
        return Status::error(QStringLiteral("Database was created by a newer version of the app (schema %1 > %2)")
                                 .arg(current)
                                 .arg(latest));
    }

    for (const schema::Migration &m : all) {
        if (m.version <= current)
            continue;
        const Status s = transaction([&]() -> Status {
            QSqlDatabase db = connection();
            if (const Status applied = m.apply(db); !applied)
                return applied;
            QSqlQuery v(db);
            if (!v.exec(QStringLiteral("PRAGMA user_version = %1").arg(m.version)))
                return sql::failed(v, QStringLiteral("Setting schema version"));
            return Status::ok();
        });
        if (!s)
            return Status::error(QStringLiteral("Migration to schema v%1 failed: %2").arg(m.version).arg(s.message()));
        qCInfo(lcDatabase) << "Migrated database to schema" << m.version;
    }
    return Status::ok();
}

Status DatabaseManager::transaction(const std::function<Status()> &work) const
{
    ConnectionHolder &holder = m_connections.localData();
    if (holder.transactionDepth > 0)
        return work(); // join the surrounding transaction

    QSqlDatabase db = connection();
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("BEGIN IMMEDIATE")))
        return sql::failed(q, QStringLiteral("BEGIN"));

    holder.transactionDepth = 1;
    const Status result = work();
    holder.transactionDepth = 0;

    if (result) {
        if (!q.exec(QStringLiteral("COMMIT"))) {
            const Status failure = sql::failed(q, QStringLiteral("COMMIT"));
            q.exec(QStringLiteral("ROLLBACK"));
            return failure;
        }
        return Status::ok();
    }
    q.exec(QStringLiteral("ROLLBACK"));
    return result;
}

} // namespace pf
