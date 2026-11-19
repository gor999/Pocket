#pragma once

#include "models/Enums.h"

#include <QList>
#include <QString>

namespace pf {

/// Spending/income category. Built-in categories have a stable `key`, are
/// translated by the UI (Armenian / Russian / English) and cannot be deleted.
/// Custom categories carry their own user-entered `name`.
struct Category
{
    qint64 id = 0;
    QString key; // empty for custom categories
    QString name; // custom name; unused for built-ins
    QString icon; // emoji
    QString color; // "#RRGGBB"
    CategoryKind kind = CategoryKind::Any;
    bool isSystem = false;
    int sortOrder = 0;

    /// Localized name for built-ins, `name` for custom categories.
    QString displayName() const;

    /// The built-in categories, in UI order. `cash` (ATM withdrawals) was added
    /// with schema v2; migration v2 reconciles existing databases with this list.
    static QList<Category> builtIn();
};

} // namespace pf
