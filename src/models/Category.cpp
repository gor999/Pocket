#include "models/Category.h"

#include <QCoreApplication>

namespace pf {

namespace {

struct BuiltIn
{
    const char *key;
    const char *source; // untranslated text, picked up by lupdate via QT_TRANSLATE_NOOP
    const char *icon;
    const char *color;
    CategoryKind kind;
};

// clang-format off
constexpr BuiltIn kBuiltIn[] = {
    {"groceries",      QT_TRANSLATE_NOOP("Category", "Groceries"),      "\U0001F6D2", "#34C759", CategoryKind::Expense},
    {"restaurants",    QT_TRANSLATE_NOOP("Category", "Restaurants"),    "\U0001F37D️", "#FF9F0A", CategoryKind::Expense},
    {"transportation", QT_TRANSLATE_NOOP("Category", "Transportation"), "\U0001F697", "#0A84FF", CategoryKind::Expense},
    {"shopping",       QT_TRANSLATE_NOOP("Category", "Shopping"),       "\U0001F6CD️", "#BF5AF2", CategoryKind::Expense},
    {"healthcare",     QT_TRANSLATE_NOOP("Category", "Healthcare"),     "\U0001F3E5", "#FF453A", CategoryKind::Expense},
    {"utilities",      QT_TRANSLATE_NOOP("Category", "Utilities"),      "\U0001F4A1", "#64D2FF", CategoryKind::Expense},
    {"entertainment",  QT_TRANSLATE_NOOP("Category", "Entertainment"),  "\U0001F3AC", "#FF375F", CategoryKind::Expense},
    {"salary",         QT_TRANSLATE_NOOP("Category", "Salary"),         "\U0001F4BC", "#30D158", CategoryKind::Income},
    {"transfers",      QT_TRANSLATE_NOOP("Category", "Transfers"),      "\U0001F501", "#8E8E93", CategoryKind::Any},
    {"cash",           QT_TRANSLATE_NOOP("Category", "Cash"),           "\U0001F4B5", "#8FBF3F", CategoryKind::Expense},
    {"other",          QT_TRANSLATE_NOOP("Category", "Other"),          "\U0001F4E6", "#A2845E", CategoryKind::Any},
};
// clang-format on

} // namespace

QString Category::displayName() const
{
    if (!isSystem || key.isEmpty())
        return name;
    for (const BuiltIn &b : kBuiltIn) {
        if (key == QLatin1String(b.key))
            return QCoreApplication::translate("Category", b.source);
    }
    return name.isEmpty() ? key : name;
}

QList<Category> Category::builtIn()
{
    QList<Category> list;
    int order = 0;
    for (const BuiltIn &b : kBuiltIn) {
        Category c;
        c.key = QString::fromLatin1(b.key);
        c.icon = QString::fromUtf8(b.icon);
        c.color = QString::fromLatin1(b.color);
        c.kind = b.kind;
        c.isSystem = true;
        c.sortOrder = order++;
        list << c;
    }
    return list;
}

} // namespace pf
