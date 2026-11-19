#include "categorization/MerchantNormalizer.h"

#include <QRegularExpression>

namespace pf {

namespace {

using RE = QRegularExpression;
constexpr auto kOpts = RE::CaseInsensitiveOption | RE::UseUnicodePropertiesOption;

bool hasLowercaseLetter(const QString &s)
{
    for (const QChar c : s) {
        if (c.isLetter() && c.isLower())
            return true;
    }
    return false;
}

QString titleCaseLatin(const QString &word)
{
    // Short all-caps words (SAS, KFC, ATM) are acronyms; longer ones are title-cased.
    bool latin = false;
    for (const QChar c : word) {
        if (c.isLetter() && c.script() == QChar::Script_Latin)
            latin = true;
    }
    if (!latin || word.size() <= 3)
        return word;
    return word.left(1).toUpper() + word.mid(1).toLower();
}

} // namespace

NormalizedMerchant MerchantNormalizer::normalize(const QString &raw)
{
    QString s = raw.simplified();

    // Terminal / processor prefixes.
    static const RE prefix(QStringLiteral(R"(^(?:pos|ecom|e-com|purchase|payment|terminal|term|www\.)[ :*\-]*)"), kOpts);
    const bool hadPrefix = prefix.match(s).hasMatch();
    s.remove(prefix);
    if (hadPrefix) {
        // "POS 1234 CARREFOUR": the number after a processor prefix is the terminal id, not part of the name
        // ("POS 7-ELEVEN" keeps its 7: only digits followed by a space, colon or star count).
        static const RE terminalId(QStringLiteral(R"(^\d+[ :*]+(?=\S*\p{L}))"), RE::UseUnicodePropertiesOption);
        s.remove(terminalId);
    }
    s.replace(QLatin1Char('*'), QLatin1Char(' '));

    // Punctuation that is only noise; keep '.', '&', '-' inside names.
    static const RE noise(QStringLiteral(R"([|_\\/"“”«»(),;:!?#№]+)"));
    s.replace(noise, QStringLiteral(" "));
    s = s.simplified();

    // Legal forms as separate words.
    static const RE legal(
        QStringLiteral(R"((?<![\p{L}])(?:llc|cjsc|ojsc|jsc|ltd|co|ооо|зао|оао|ип|սպը|փբը|բբը)(?![\p{L}])\.?)"), kOpts);
    s.remove(legal);
    s = s.simplified();

    // Trailing city / country words and store or terminal numbers, repeatedly:
    // "ATM 1234 YEREVAN AM" -> "ATM".
    static const RE tail(
        QStringLiteral(R"((?:^|\s)(?:yerevan|gyumri|vanadzor|abovyan|armenia|arm|am|ереван|гюмри|ванадзор|армения|երևան|երեւան|գյումրի|վանաձոր|հայաստան|\d+|[nN]\d+)\s*$)"),
        kOpts);
    for (int guard = 0; guard < 6; ++guard) {
        const QString before = s;
        s.remove(tail);
        s = s.simplified();
        if (s == before)
            break;
    }

    // A name that is now empty or only digits means the original was just a number/city: keep the raw text.
    static const RE hasLetter(QStringLiteral(R"(\p{L})"), RE::UseUnicodePropertiesOption);
    if (!hasLetter.match(s).hasMatch())
        s = raw.simplified();

    NormalizedMerchant result;
    if (hasLowercaseLetter(s)) {
        result.display = s;
    } else {
        QStringList words = s.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        for (QString &w : words)
            w = titleCaseLatin(w);
        result.display = words.join(QLatin1Char(' '));
    }
    result.key = result.display.toLower().simplified();
    return result;
}

} // namespace pf
