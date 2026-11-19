#include "core/SettingsManager.h"

#include "core/Currency.h"

namespace pf {

namespace {
const QString kThemeKey = QStringLiteral("ui.theme");
const QString kLanguageKey = QStringLiteral("ui.language");
const QString kCurrencyKey = QStringLiteral("ui.defaultCurrency");

bool isValidTheme(const QString &t)
{
    return t == QLatin1String("system") || t == QLatin1String("light") || t == QLatin1String("dark");
}

bool isValidLanguage(const QString &l)
{
    return l == QLatin1String("en") || l == QLatin1String("hy") || l == QLatin1String("ru");
}
} // namespace

SettingsManager::SettingsManager(SettingsRepository &repository, QObject *parent)
    : QObject(parent)
    , m_repository(repository)
{
}

Status SettingsManager::load()
{
    const QString theme = m_repository.value(kThemeKey, m_theme);
    const QString language = m_repository.value(kLanguageKey, m_language);
    const QString currency = Currency::normalize(m_repository.value(kCurrencyKey, m_defaultCurrency));

    const bool darkBefore = effectiveDark();
    m_theme = isValidTheme(theme) ? theme : QStringLiteral("system");
    m_language = isValidLanguage(language) ? language : QStringLiteral("en");
    m_defaultCurrency = Currency::isSupported(currency) ? currency : QStringLiteral("AMD");

    emit themeChanged();
    emit languageChanged();
    emit defaultCurrencyChanged();
    if (darkBefore != effectiveDark())
        emit effectiveDarkChanged();
    return Status::ok();
}

bool SettingsManager::effectiveDark() const
{
    if (m_theme == QLatin1String("dark"))
        return true;
    if (m_theme == QLatin1String("light"))
        return false;
    return m_systemDark;
}

QString SettingsManager::localeName() const
{
    if (m_language == QLatin1String("hy"))
        return QStringLiteral("hy_AM");
    if (m_language == QLatin1String("ru"))
        return QStringLiteral("ru_RU");
    return QStringLiteral("en_US");
}

QLocale SettingsManager::locale() const
{
    return QLocale(localeName());
}

void SettingsManager::setTheme(const QString &theme)
{
    if (!isValidTheme(theme) || theme == m_theme)
        return;
    const bool darkBefore = effectiveDark();
    m_theme = theme;
    m_repository.setValue(kThemeKey, theme);
    emit themeChanged();
    if (darkBefore != effectiveDark())
        emit effectiveDarkChanged();
}

void SettingsManager::setLanguage(const QString &language)
{
    if (!isValidLanguage(language) || language == m_language)
        return;
    m_language = language;
    m_repository.setValue(kLanguageKey, language);
    emit languageChanged();
}

void SettingsManager::setDefaultCurrency(const QString &currency)
{
    const QString code = Currency::normalize(currency);
    if (!Currency::isSupported(code) || code == m_defaultCurrency)
        return;
    m_defaultCurrency = code;
    m_repository.setValue(kCurrencyKey, code);
    emit defaultCurrencyChanged();
}

void SettingsManager::setSystemDark(bool dark)
{
    if (dark == m_systemDark)
        return;
    const bool darkBefore = effectiveDark();
    m_systemDark = dark;
    if (darkBefore != effectiveDark())
        emit effectiveDarkChanged();
}

QVariantList SettingsManager::languages() const
{
    // Native names are intentionally not translated.
    return {
        QVariantMap{{QStringLiteral("code"), QStringLiteral("en")}, {QStringLiteral("name"), QStringLiteral("English")}},
        QVariantMap{{QStringLiteral("code"), QStringLiteral("hy")}, {QStringLiteral("name"), QStringLiteral("Հայերեն")}},
        QVariantMap{{QStringLiteral("code"), QStringLiteral("ru")}, {QStringLiteral("name"), QStringLiteral("Русский")}},
    };
}

QStringList SettingsManager::currencies() const
{
    return Currency::supportedCodes();
}

} // namespace pf
