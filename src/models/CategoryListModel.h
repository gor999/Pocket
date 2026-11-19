#pragma once

#include "core/SettingsManager.h"
#include "database/CategoryRepository.h"

#include <QAbstractListModel>
#include <QVariantList>

namespace pf {

class CategoryListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role { IdRole = Qt::UserRole + 1, KeyRole, NameRole, IconRole, ColorRole, KindRole, IsSystemRole };

    CategoryListModel(CategoryRepository &categories, SettingsManager &settings, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return int(m_rows.size()); }

    Q_INVOKABLE void reload();
    Q_INVOKABLE QVariantMap get(int row) const;
    /// Categories suitable for "income" or "expense" (plus kind=any), as
    /// [{value (id), label, icon, color}] for chip selectors.
    Q_INVOKABLE QVariantList choicesFor(const QString &type) const;

    /// Row data by id (used by other models for display lookups).
    const QList<Category> &categories() const { return m_rows; }

signals:
    void countChanged();

private:
    CategoryRepository &m_categories;
    SettingsManager &m_settings;
    QList<Category> m_rows;
};

} // namespace pf
