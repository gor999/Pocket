#pragma once

#include "core/Result.h"
#include "database/DatabaseManager.h"
#include "models/Category.h"

#include <QList>

namespace pf {

class CategoryRepository
{
public:
    explicit CategoryRepository(const DatabaseManager &db) : m_db(db) {}

    Result<QList<Category>> all() const;
    Result<Category> byId(qint64 id) const;
    Result<Category> byKey(const QString &key) const;

    /// Adds a custom (non built-in) category.
    Result<Category> insertCustom(Category category) const;
    /// Renames/recolors a custom category. Built-ins are immutable.
    Status updateCustom(const Category &category) const;
    /// Deletes a custom category; its transactions become uncategorized.
    Status removeCustom(qint64 id) const;

private:
    const DatabaseManager &m_db;
};

} // namespace pf
