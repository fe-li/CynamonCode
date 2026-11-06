#include "services/SettingsService.h"

#include <QSettings>

#include <algorithm>

namespace cynamoncode {

namespace {

const QString FontFamilyKey = QStringLiteral("editor/fontFamily");
const QString FontSizeKey = QStringLiteral("editor/fontSize");
const QString WordWrapKey = QStringLiteral("editor/wordWrap");
const QString LineNumbersKey = QStringLiteral("editor/lineNumbers");
const QString WhitespaceKey = QStringLiteral("editor/showWhitespace");
const QString LineEndingsKey = QStringLiteral("editor/showLineEndings");
const QString ZoomKey = QStringLiteral("editor/zoom");
const QString ThemeKey = QStringLiteral("appearance/theme");
const QString GeometryKey = QStringLiteral("window/geometry");
const QString RestoreSessionKey = QStringLiteral("session/restore");

bool readBool(const QSettings &settings, const QString &key, bool fallback)
{
    const QVariant value = settings.value(key);
    if (!value.isValid())
        return fallback;
    const QString text = value.toString().trimmed().toLower();
    if (text == QLatin1String("true") || text == QLatin1String("1"))
        return true;
    if (text == QLatin1String("false") || text == QLatin1String("0"))
        return false;
    return fallback;
}

} // namespace

SettingsService::SettingsService(QSettings &settings)
    : m_settings(settings)
{
}

EditorSettings SettingsService::load() const
{
    const EditorSettings defaults;
    EditorSettings result;

    result.fontFamily = m_settings.value(FontFamilyKey).toString().trimmed();

    bool sizeOk = false;
    const int size = m_settings.value(FontSizeKey).toInt(&sizeOk);
    result.fontPointSize = (sizeOk && size >= 4 && size <= 96) ? size : defaults.fontPointSize;

    result.wordWrap = readBool(m_settings, WordWrapKey, defaults.wordWrap);
    result.lineNumbers = readBool(m_settings, LineNumbersKey, defaults.lineNumbers);
    result.showWhitespace = readBool(m_settings, WhitespaceKey, defaults.showWhitespace);
    result.showLineEndings = readBool(m_settings, LineEndingsKey, defaults.showLineEndings);

    bool zoomOk = false;
    const int zoom = m_settings.value(ZoomKey).toInt(&zoomOk);
    result.zoom = zoomOk ? std::clamp(zoom, MinZoom, MaxZoom) : defaults.zoom;

    const QString theme = m_settings.value(ThemeKey).toString();
    result.theme = (theme == QLatin1String("dark") || theme == QLatin1String("light"))
                       ? theme
                       : defaults.theme;
    result.restoreSession = readBool(m_settings, RestoreSessionKey, defaults.restoreSession);
    return result;
}

void SettingsService::save(const EditorSettings &settings)
{
    m_settings.setValue(FontFamilyKey, settings.fontFamily);
    m_settings.setValue(FontSizeKey, settings.fontPointSize);
    m_settings.setValue(WordWrapKey, settings.wordWrap);
    m_settings.setValue(LineNumbersKey, settings.lineNumbers);
    m_settings.setValue(WhitespaceKey, settings.showWhitespace);
    m_settings.setValue(LineEndingsKey, settings.showLineEndings);
    m_settings.setValue(ZoomKey, std::clamp(settings.zoom, MinZoom, MaxZoom));
    m_settings.setValue(ThemeKey, settings.theme);
    m_settings.setValue(RestoreSessionKey, settings.restoreSession);
    m_settings.sync();
}

QByteArray SettingsService::windowGeometry() const
{
    return m_settings.value(GeometryKey).toByteArray();
}

void SettingsService::saveWindowGeometry(const QByteArray &geometry)
{
    m_settings.setValue(GeometryKey, geometry);
    m_settings.sync();
}

} // namespace cynamoncode
