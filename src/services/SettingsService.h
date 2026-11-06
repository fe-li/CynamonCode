#pragma once

#include <QByteArray>
#include <QString>

class QSettings;

namespace cynamoncode {

// Ustawienia edytora trwałe między uruchomieniami (D029).
struct EditorSettings
{
    QString fontFamily;   // pusty = systemowa czcionka o stałej szerokości
    int fontPointSize = 0; // 0 = domyślny rozmiar systemowy
    bool wordWrap = false;
    bool lineNumbers = true;
    bool showWhitespace = false;
    bool showLineEndings = false;
    int zoom = 0;          // poziom powiększenia QScintilli (-10..20)
    QString theme = QStringLiteral("light"); // "light" / "dark"
    bool restoreSession = true; // D034

    friend bool operator==(const EditorSettings &, const EditorSettings &) = default;
};

// Odczyt i zapis przez QSettings; nieznane lub błędne wartości → domyślne.
// Nie posiada obiektu QSettings.
class SettingsService
{
public:
    explicit SettingsService(QSettings &settings);

    EditorSettings load() const;
    void save(const EditorSettings &settings);

    QByteArray windowGeometry() const;
    void saveWindowGeometry(const QByteArray &geometry);

    static constexpr int MinZoom = -10;
    static constexpr int MaxZoom = 20;

private:
    QSettings &m_settings;
};

} // namespace cynamoncode
