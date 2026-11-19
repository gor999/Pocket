#include "database/CategoryRepository.h"

#include "database/SqlUtil.h"

#include <QSqlQuery>

namespace pf {

namespace {

const QString kColumns = QStringLiteral("id, key, name, icon, color, kind, is_system, sort_order");

Category readCategory(const QSqlQuery &q)
{
    Category c;
    c.id = q.value(0).toLongLong();
    c.key = q.value(1).toString();
    c.name = q.value(2).toString();
    c.icon = q.value(3).toString();
    c.color = q.value(4).toString();
    c.kind = categoryKindFromKey(q.value(5).toString());
    c.isSystem = q.value(6).toBool();
    c.sortOrder = q.value(7).toInt();
    return c;
}

} // namespace

Result<QList<Category>> CategoryRepository::all() const
{
    QSqlQuery q(m_db.connection());
    if (!q.exec(QStringLiteral("SELECT %1 FROM categories ORDER BY sort_order, id").arg(kColumns)))
        return sql::failed(q, QStringLiteral("List categories"));
    QList<Category> list;
    while (q.next())
        list << readCategory(q);
    return list;
}

Result<Category> CategoryRepository::byId(qint64 id) const
{
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral("SELECT %1 FROM categories WHERE id = ?").arg(kColumns));
    q.addBindValue(qlonglong(id));
    if (!q.exec())
        return sql::failed(q, QStringLiteral("Load category"));
    if (!q.next())
        return Result<Category>::failure(QStringLiteral("Category %1 does not exist").arg(id));
    return readCategory(q);
}

Result<Category> CategoryRepository::byKey(const QString &key) const
{
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral("SELECT %1 FROM categories WHERE key = ?").arg(kColumns));
    q.addBindValue(key);
    if (!q.exec())
        return sql::failed(q, QStringLiteral("Load category"));
    if (!q.next())
        return Result<Category>::failure(QStringLiteral("Category '%1' does not exist").arg(key));
    return readCategory(q);
}

Result<Category> CategoryRepository::insertCustom(Category c) const
{
    if (c.name.trimmed().isEmpty())
        return Result<Category>::failure(QStringLiteral("Category name is empty"));

    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral(
        "INSERT INTO categories (key, name, icon, color, kind, is_system, sort_order) "
        "VALUES (NULL, ?, ?, ?, ?, 0, COALESCE((SELECT MAX(sort_order) + 1 FROM categories), 0))"));
    q.addBindValue(c.name.trimmed());
    q.addBindValue(sql::text(c.icon));
    q.addBindValue(sql::text(c.color));
    q.addBindValue(toKey(c.kind));
    if (!q.exec())
        return sql::failed(q, QStringLiteral("Insert category"));
    c.id = q.lastInsertId().toLongLong();
    c.key.clear();
    c.isSystem = false;
    return c;
}

Status CategoryRepository::updateCustom(const Category &c) const
{
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral("UPDATE categories SET name = ?, icon = ?, color = ?, kind = ? "
                             "WHERE id = ? AND is_system = 0"));
    q.addBindValue(c.name.trimmed());
    q.addBindValue(sql::text(c.icon));
    q.addBindValue(sql::text(c.color));
    q.addBindValue(toKey(c.kind));
    q.addBindValue(qlonglong(c.id));
    if (!q.exec())
        return sql::failed(q, QStringLiteral("Update category"));
    if (q.numRowsAffected() == 0)
        return Status::error(QStringLiteral("Only custom categories can be edited"));
    return Status::ok();
}

Status CategoryRepository::removeCustom(qint64 id) const
{
    QSqlQuery q(m_db.connection());
    q.prepare(QStringLiteral("DELETE FROM categories WHERE id = ? AND is_system = 0"));
    q.addBindValue(qlonglong(id));
    if (!q.exec())
        return sql::failed(q, QStringLiteral("Delete category"));
    if (q.numRowsAffected() == 0)
        return Status::error(QStringLiteral("Only custom categories can be deleted"));
    return Status::ok();
}

} // namespace pf
