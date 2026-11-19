#pragma once

#include <QString>
#include <QtGlobal>

#include <optional>
#include <utility>

namespace pf {

/// Success/failure of an operation that produces no value.
/// The project does not use exceptions for expected failures (bad input,
/// database errors); those travel as Status / Result<T> values instead.
class Status
{
public:
    Status() = default; // success

    static Status ok() { return {}; }
    static Status error(QString message)
    {
        Status s;
        s.m_failed = true;
        s.m_message = std::move(message);
        return s;
    }

    bool isOk() const { return !m_failed; }
    explicit operator bool() const { return isOk(); }
    const QString &message() const { return m_message; }

private:
    QString m_message;
    bool m_failed = false;
};

/// A value or an error message.
template<typename T>
class Result
{
public:
    Result(T value) : m_value(std::move(value)) {}
    Result(Status failure) : m_status(std::move(failure)) {}

    static Result failure(QString message) { return Result(Status::error(std::move(message))); }

    bool isOk() const { return m_value.has_value(); }
    explicit operator bool() const { return isOk(); }

    T &value() { Q_ASSERT_X(m_value.has_value(), "Result::value", "accessing the value of a failed Result"); return *m_value; }
    const T &value() const { Q_ASSERT_X(m_value.has_value(), "Result::value", "accessing the value of a failed Result"); return *m_value; }
    T valueOr(T fallback) const { return m_value ? *m_value : std::move(fallback); }

    const QString &error() const { return m_status.message(); }
    Status status() const { return m_value ? Status::ok() : m_status; }

private:
    std::optional<T> m_value;
    Status m_status = Status::error(QStringLiteral("empty result"));
};

} // namespace pf
