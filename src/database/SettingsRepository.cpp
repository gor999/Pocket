#include "database/SettingsRepository.h"

#include "database/SqlUtil.h"

#include <QSqlQuery>

namespace pf {

QString SettingsRepository::value(const QString &key, const QString &fallback) const
{
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral("SELECT value FROM settings WHERE key = ?"));
    q.addBindValue(key);
    if (q.exec() && q.next())
        return q.value(0).toString();
    return fallback;
}

Status SettingsRepository::setValue(const QString &key, const QString &value) const
{
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral("INSERT INTO settings (key, value) VALUES (?, ?) "
                             "ON CONFLICT(key) DO UPDATE SET value = excluded.value"));
    q.addBindValue(key);
    q.addBindValue(value);
    if (!q.exec())
        return sql::failed(q, QStringLiteral("Save setting"));
    return Status::ok();
}

} // namespace pf
