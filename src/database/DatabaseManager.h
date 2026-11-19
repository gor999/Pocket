#pragma once

#include "core/Result.h"

#include <QSqlDatabase>
#include <QString>
#include <QThreadStorage>

#include <functional>

namespace pf {

/// Owns the SQLite database file: opens it, applies pragmas, runs schema
/// migrations and hands out one connection *per thread*.
///
/// QSqlDatabase connections must not cross threads, so connection() lazily
/// creates a connection for the calling thread. That lets heavy work (CSV
/// import, analytics) run in QtConcurrent / worker threads using the very same
/// repository classes as the UI thread. SQLite runs in WAL mode so readers do
/// not block the writer.
///
/// Lifetime: every thread that used connection() must have finished before the
/// manager is destroyed.
class DatabaseManager
{
public:
    explicit DatabaseManager(QString databasePath);
    ~DatabaseManager();

    DatabaseManager(const DatabaseManager &) = delete;
    DatabaseManager &operator=(const DatabaseManager &) = delete;

    /// Creates the directory/file if needed, opens the connection for the
    /// calling thread and migrates the schema to the latest version.
    Status open();

    const QString &path() const { return m_path; }

    /// Connection for the calling thread (created on first use).
    QSqlDatabase connection() const;

    /// Runs `work` inside BEGIN IMMEDIATE ... COMMIT; rolls back if it returns
    /// an error. Nested calls on the same thread join the outer transaction.
    Status transaction(const std::function<Status()> &work) const;

    int schemaVersion() const;

private:
    struct ConnectionHolder
    {
        QString name;
        int transactionDepth = 0;
        ConnectionHolder() = default;
        ConnectionHolder(const ConnectionHolder &) = delete;
        ConnectionHolder &operator=(const ConnectionHolder &) = delete;
        ~ConnectionHolder() { release(); }
        void release();
    };

    Status migrate();

    QString m_path;
    quint64 m_instanceId;
    mutable QThreadStorage<ConnectionHolder> m_connections;
};

} // namespace pf
