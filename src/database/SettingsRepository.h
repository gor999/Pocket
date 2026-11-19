#pragma once

#include "core/Result.h"
#include "database/DatabaseManager.h"

#include <QString>

namespace pf {

/// Key/value storage in the `settings` table.
/// Never store secrets here; sensitive values belong in the iOS Keychain.
class SettingsRepository
{
public:
    explicit SettingsRepository(const DatabaseManager &db) : m_db(db) {}

    QString value(const QString &key, const QString &fallback = QString()) const;
    Status setValue(const QString &key, const QString &value) const;

private:
    const DatabaseManager &m_db;
};

} // namespace pf
