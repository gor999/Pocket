#include "models/CategoryListModel.h"

namespace pf {

CategoryListModel::CategoryListModel(CategoryRepository &categories, SettingsManager &settings, QObject *parent)
    : QAbstractListModel(parent)
    , m_categories(categories)
    , m_settings(settings)
{
    // Built-in category names are translated; refresh when the language changes.
    connect(&m_settings, &SettingsManager::languageChanged, this, [this] {
        if (!m_rows.isEmpty())
            emit dataChanged(index(0), index(int(m_rows.size()) - 1), {NameRole});
    });
}

int CategoryListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QHash<int, QByteArray> CategoryListModel::roleNames() const
{
    return {
        {IdRole, "categoryId"}, {KeyRole, "key"},   {NameRole, "name"},       {IconRole, "icon"},
        {ColorRole, "color"},   {KindRole, "kind"}, {IsSystemRole, "isSystem"},
    };
}

QVariant CategoryListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const Category &c = m_rows.at(index.row());
    switch (role) {
    case IdRole: return qlonglong(c.id);
    case KeyRole: return c.key;
    case NameRole: return c.displayName();
    case IconRole: return c.icon;
    case ColorRole: return c.color;
    case KindRole: return toKey(c.kind);
    case IsSystemRole: return c.isSystem;
    default: return {};
    }
}

void CategoryListModel::reload()
{
    const int before = count();
    const auto list = m_categories.all();
    beginResetModel();
    m_rows = list ? list.value() : QList<Category>{};
    endResetModel();
    if (before != count())
        emit countChanged();
}

QVariantMap CategoryListModel::get(int row) const
{
    QVariantMap map;
    if (row < 0 || row >= m_rows.size())
        return map;
    const QHash<int, QByteArray> names = roleNames();
    for (auto it = names.cbegin(); it != names.cend(); ++it)
        map.insert(QString::fromLatin1(it.value()), data(index(row, 0), it.key()));
    return map;
}

QVariantList CategoryListModel::choicesFor(const QString &type) const
{
    QVariantList list;
    for (const Category &c : m_rows) {
        const QString kind = toKey(c.kind);
        if (kind != QLatin1String("any") && kind != type)
            continue;
        list << QVariantMap{
            {QStringLiteral("value"), qlonglong(c.id)},
            {QStringLiteral("label"), c.displayName()},
            {QStringLiteral("icon"), c.icon},
            {QStringLiteral("color"), c.color},
        };
    }
    return list;
}

} // namespace pf
