#pragma once

#include "core/Result.h"

#include <QDateTime>
#include <QMetaType>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <optional>

namespace pf::sql {

inline Status failed(const QSqlQuery &q, const QString &context)
{
    return Status::error(context + QStringLiteral(": ") + q.lastError().text());
}

/// NULL-able integer bind value.
inline QVariant nullableInt(const std::optional<qint64> &v)
{
    return v ? QVariant::fromValue<qlonglong>(*v) : QVariant(QMetaType::fromType<qlonglong>());
}

inline std::optional<qint64> optionalInt(const QVariant &v)
{
    return v.isNull() ? std::nullopt : std::optional<qint64>(v.toLongLong());
}

/// A null QString binds as SQL NULL, which violates NOT NULL text columns.
/// Always bind text through this helper so "no text" is stored as ''.
inline QString text(const QString &s)
{
    return s.isNull() ? QStringLiteral("") : s;
}

inline qint64 toEpoch(const QDateTime &dt)
{
    return dt.toSecsSinceEpoch();
}

inline QDateTime fromEpoch(qint64 secs)
{
    return QDateTime::fromSecsSinceEpoch(secs);
}

/// Escapes %, _ and \ so user text can be used inside LIKE ... ESCAPE '\'.
inline QString escapeLike(const QString &text)
{
    QString out;
    out.reserve(text.size());
    for (const QChar c : text) {
        if (c == QLatin1Char('%') || c == QLatin1Char('_') || c == QLatin1Char('\\'))
            out += QLatin1Char('\\');
        out += c;
    }
    return out;
}

} // namespace pf::sql
