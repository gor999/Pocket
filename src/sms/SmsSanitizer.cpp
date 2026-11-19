#include "sms/SmsSanitizer.h"

#include <QRegularExpression>

namespace pf::sms {

namespace {

bool isStrippedChar(QChar c)
{
    const ushort u = c.unicode();
    if (u == '\n' || u == '\t')
        return false; // handled separately
    if (u < 0x20 || u == 0x7F)
        return true; // C0 controls + DEL
    if (u >= 0x80 && u <= 0x9F)
        return true; // C1 controls
    // Zero-width and bidirectional formatting characters (used to disguise text).
    if (u == 0x200B || u == 0x200C || u == 0x200D || u == 0x2060 || u == 0xFEFF)
        return true;
    if ((u >= 0x202A && u <= 0x202E) || (u >= 0x2066 && u <= 0x2069) || u == 0x200E || u == 0x200F)
        return true;
    return false;
}

bool isUnicodeSpace(QChar c)
{
    const ushort u = c.unicode();
    return u == 0x00A0 || u == 0x1680 || (u >= 0x2000 && u <= 0x200A) || u == 0x202F || u == 0x205F || u == 0x3000;
}

} // namespace

QString SmsSanitizer::clean(const QString &raw)
{
    QString text = raw;
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    QString out;
    out.reserve(qMin<qsizetype>(text.size(), kMaxBodyChars + 16));
    for (QChar c : text) {
        if (isStrippedChar(c))
            continue;
        if (c == QLatin1Char('\t') || isUnicodeSpace(c))
            c = QLatin1Char(' ');
        out.append(c);
    }

    // Collapse runs of spaces and blank lines, trim every line.
    static const QRegularExpression spaces(QStringLiteral("[ ]{2,}"));
    out.replace(spaces, QStringLiteral(" "));
    QStringList lines = out.split(QLatin1Char('\n'));
    for (QString &line : lines)
        line = line.trimmed();
    out = lines.join(QLatin1Char('\n'));
    static const QRegularExpression blankLines(QStringLiteral("\n{3,}"));
    out.replace(blankLines, QStringLiteral("\n\n"));
    return out.trimmed();
}

QString SmsSanitizer::maskSensitive(const QString &text)
{
    QString out = text;

    // 13-19 digits, optionally grouped by single spaces or dashes: a card number.
    static const QRegularExpression pan(QStringLiteral(R"((?<![\d.,])(?:\d[ -]?){12,18}\d(?![\d]|[.,]\d))"));
    QRegularExpressionMatchIterator it = pan.globalMatch(out);
    QList<std::pair<qsizetype, qsizetype>> spans;
    QStringList replacements;
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        QString digits = m.captured(0);
        digits.remove(QRegularExpression(QStringLiteral("[^\\d]")));
        if (digits.size() < 13 || digits.size() > 19)
            continue;
        spans.append({m.capturedStart(0), m.capturedLength(0)});
        replacements.append(QStringLiteral("**** **** **** ") + digits.right(4));
    }
    for (qsizetype i = spans.size() - 1; i >= 0; --i)
        out.replace(spans[i].first, spans[i].second, replacements[i]);

    static const QRegularExpression secretValue(
        QStringLiteral(R"(\b(cvv2?|cvc2?|pin|пин|ՊԻՆ)\b(\s*[:=]?\s*)\d{3,8})"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    out.replace(secretValue, QStringLiteral("\\1\\2***"));
    return out;
}

QString SmsSanitizer::sanitizeForStorage(const QString &raw)
{
    return maskSensitive(clean(raw));
}

bool SmsSanitizer::isSecretBearing(const QString &text)
{
    // Phrases for one-time codes and secrets in English, Russian and Armenian.
    // Deliberately narrow: "authorization code" appears in ordinary purchase
    // notifications, so a bare "code" must not match.
    static const QRegularExpression secret(
        QStringLiteral(
            R"((?<![\p{L}])(otp|one[- ]?time|verification\s+code|confirmation\s+code|security\s+code|passcode|password|3[- ]?d[- ]?secure|3ds|cvv2?|cvc2?|pin)(?![\p{L}])|)"
            R"(код\s+(подтверждения|безопасности|верификации|доступа)|одноразов|пароль|пин[- ]?код|)"
            R"(հաստատման\s+կոդ|միանգամյա|գաղտնաբառ|անվտանգության\s+կոդ|վավերացման\s+կոդ|ՊԻՆ)"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    return secret.match(text).hasMatch();
}

QString SmsSanitizer::cleanSender(const QString &raw)
{
    QString out;
    for (QChar c : raw) {
        if (isStrippedChar(c) || c == QLatin1Char('\n'))
            continue;
        out.append(isUnicodeSpace(c) ? QLatin1Char(' ') : c);
    }
    return out.simplified().left(kMaxSenderChars);
}

} // namespace pf::sms
