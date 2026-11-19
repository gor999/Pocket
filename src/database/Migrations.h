#pragma once

#include "core/Result.h"

#include <QList>
#include <QSqlDatabase>

#include <functional>

namespace pf::schema {

struct Migration
{
    int version;
    std::function<Status(QSqlDatabase &)> apply;
};

/// Ordered list of schema migrations. Version N is stored in SQLite's
/// `PRAGMA user_version` after migration N has been applied. Never edit a
/// released migration: add a new one.
const QList<Migration> &migrations();

} // namespace pf::schema
