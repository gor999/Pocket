#pragma once

#include "core/Result.h"
#include "database/SettingsRepository.h"

#include <QLocale>
#include <QObject>
#include <QStringList>
#include <QVariantList>

namespace pf {

/// Application preferences exposed to QML. Persisted in the `settings` table.
class SettingsManager : public QObject
{
    Q_OBJECT
    /// "system" | "light" | "dark"
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    /// "en" | "hy" | "ru"
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QString defaultCurrency READ defaultCurrency WRITE setDefaultCurrency NOTIFY defaultCurrencyChanged)
    Q_PROPERTY(bool effectiveDark READ effectiveDark NOTIFY effectiveDarkChanged)
    Q_PROPERTY(QString localeName READ localeName NOTIFY languageChanged)

public:
    explicit SettingsManager(SettingsRepository &repository, QObject *parent = nullptr);

    /// Loads persisted values (call after the database is open).
    Status load();

    QString theme() const { return m_theme; }
    QString language() const { return m_language; }
    QString defaultCurrency() const { return m_defaultCurrency; }
    bool effectiveDark() const;
    QString localeName() const;
    QLocale locale() const;

    void setTheme(const QString &theme);
    void setLanguage(const QString &language);
    void setDefaultCurrency(const QString &currency);

    /// Fed by the platform layer (OS light/dark appearance).
    void setSystemDark(bool dark);

    Q_INVOKABLE QVariantList languages() const; // [{code, name}]
    Q_INVOKABLE QStringList currencies() const;

signals:
    void themeChanged();
    void languageChanged();
    void defaultCurrencyChanged();
    void effectiveDarkChanged();

private:
    SettingsRepository &m_repository;
    QString m_theme = QStringLiteral("system");
    QString m_language = QStringLiteral("en"); // product default
    QString m_defaultCurrency = QStringLiteral("AMD");
    bool m_systemDark = false;
};

} // namespace pf
