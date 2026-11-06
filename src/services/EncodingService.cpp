#include "services/EncodingService.h"

#include <utility>

namespace cynamoncode::EncodingService {

namespace {

const QByteArray &utf8Bom()
{
    static const QByteArray bom("\xEF\xBB\xBF", 3);
    return bom;
}

} // namespace

DecodeResult decodeUtf8(const QByteArray &bytes)
{
    DecodeResult result;

    if (bytes.contains('\0')) {
        result.error = DecodeError::ContainsNul;
        return result;
    }

    result.hasBom = bytes.startsWith(utf8Bom());
    const QByteArray payload = result.hasBom ? bytes.mid(utf8Bom().size()) : bytes;

    // fromUtf8 zamienia niepoprawne sekwencje na U+FFFD, więc o poprawności
    // rozstrzyga porównanie bajtów po ponownym zakodowaniu.
    QString text = QString::fromUtf8(payload);
    if (text.toUtf8() != payload) {
        result.error = DecodeError::InvalidUtf8;
        result.hasBom = false;
        return result;
    }

    result.text = std::move(text);
    result.lineEnding = detectLineEnding(payload);
    return result;
}

QByteArray encodeUtf8(const QString &text, bool withBom)
{
    QByteArray out;
    if (withBom)
        out = utf8Bom();
    out += text.toUtf8();
    return out;
}

LineEnding detectLineEnding(const QByteArray &bytes)
{
    bool hasLf = false;
    bool hasCrLf = false;
    bool hasLoneCr = false;

    const qsizetype size = bytes.size();
    for (qsizetype i = 0; i < size; ++i) {
        const char c = bytes.at(i);
        if (c == '\r') {
            if (i + 1 < size && bytes.at(i + 1) == '\n') {
                hasCrLf = true;
                ++i;
            } else {
                hasLoneCr = true;
            }
        } else if (c == '\n') {
            hasLf = true;
        }
    }

    if (hasLoneCr || (hasLf && hasCrLf))
        return LineEnding::Mixed;
    if (hasCrLf)
        return LineEnding::CRLF;
    if (hasLf)
        return LineEnding::LF;
    return LineEnding::None;
}

} // namespace cynamoncode::EncodingService
