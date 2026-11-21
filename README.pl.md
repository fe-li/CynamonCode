<p align="center">
  <img src="resources/icons/cynamoncode-128.png" width="128" height="128" alt="Logo CynamonCode">
</p>

<h1 align="center">CynamonCode</h1>

<p align="center">
  <strong>Lekki, natywny edytor tekstu i kodu dla Linuksa, Windows i macOS.</strong><br>
  Karty, kolorowanie składni i wygodne wyszukiwanie — bez podpowiedzi, bez sprawdzania kodu, bez rozpraszania.
</p>

<p align="center">
  <a href="https://github.com/fe-li/CynamonCode/releases/latest"><img src="https://img.shields.io/github/v/release/fe-li/CynamonCode?label=wersja" alt="Najnowsza wersja"></a>
  <a href="https://github.com/fe-li/CynamonCode/actions/workflows/ci.yml"><img src="https://github.com/fe-li/CynamonCode/actions/workflows/ci.yml/badge.svg" alt="Budowanie i testy"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/licencja-GPL--3.0--or--later-blue" alt="Licencja GPL-3.0-or-later"></a>
</p>

<p align="center"><a href="README.md">English</a> · <strong>Polski</strong></p>

<p align="center">
  <a href="https://github.com/fe-li/CynamonCode/releases/latest"><strong>⬇ Pobierz</strong></a> ·
  <a href="#instalacja">Instalacja</a> ·
  <a href="#funkcje">Funkcje</a> ·
  <a href="#budowanie-ze-źródeł">Budowanie</a>
</p>

| Motyw jasny z panelem plików | Motyw ciemny i dwa widoki |
|---|---|
| ![CynamonCode w motywie jasnym: kilka kart, plik Pythona z kolorowaniem składni i panel plików](.github/screenshots/light-theme.png) | ![CynamonCode w motywie ciemnym: dwa widoki obok siebie z plikami HTML i CSS](.github/screenshots/dark-theme.png) |

## Funkcje

- **Karty i dwa widoki** — przenoszenie karty do drugiego widoku albo klon tego samego dokumentu obok.
- **Kolorowanie składni 12 języków** — C/C++, Python, Java, JavaScript, HTML, CSS, PHP, SQL, JSON, XML, Bash i zwykły tekst; motyw jasny i ciemny.
- **Znajdź i zamień** — wielkość liter, całe wyrazy, wyrażenia regularne; także we wszystkich otwartych plikach. „Zamień wszystkie” cofa jedno Ctrl+Z.
- **Bezpieczne pliki** — atomowy zapis, zachowane końce linii i BOM, ostrzeżenie o zmianach dokonanych przez inny program; UTF-8 i na życzenie Windows-1250, bez cichych zmian bajtów.
- **Panel plików i sesja** — drzewo folderu z filtrem; po ponownym uruchomieniu wracają otwarte pliki i pozycja kursora.
- **Klawiatura** — konfigurowalny skrót każdego polecenia menu, Ctrl+Tab między kartami, F8 między widokami, F1 — pomoc.
- **Automatyczne wcięcia** — po `{` i znacznikach HTML; tabulatory lub spacje, tak jak w pliku.
- **Integracja z systemem** — wpis w menu, „Otwórz za pomocą” dla plików tekstowych i kodu; na Linuksie także domyślny program dla HTML, CSS, JS, PHP i innych typów (tylko dla Twojego konta, odwracalnie).
- **Interfejs po polsku**, pomoc w programie.

### Czego tu nie ma — celowo

Autouzupełniania i list podpowiedzi, podpowiedzi parametrów, sprawdzania składni i podkreślania „błędów”, LSP, uruchamiania i kompilowania plików, debuggera, asystentów AI, telemetrii i połączeń z siecią. CynamonCode koloruje składnię — piszesz Ty. To stała zasada projektu, pilnowana testami (`tests/gui/tst_editor_no_suggestions.cpp`).

## Instalacja

Wszystkie pliki są w [najnowszym wydaniu](https://github.com/fe-li/CynamonCode/releases/latest); sumy kontrolne w `SHA256SUMS`.

### Linux (Mint 22.x, Ubuntu 24.04 lub nowsze, 64-bit)

Pobierz `cynamoncode_1.1.0_amd64.deb`, kliknij dwukrotnie i wybierz „Zainstaluj pakiet” — albo:

```bash
sudo apt install ./cynamoncode_1.1.0_amd64.deb
```

`apt` doinstaluje brakujące biblioteki (Qt 6, QScintilla). Uruchom z menu **Akcesoria → CynamonCode** albo `cynamoncode plik.txt`. Odinstalowanie: `sudo apt remove cynamoncode`.

### Windows (10 i 11, 64-bit)

- **Instalator:** `CynamonCode-1.1.0-windows-x64.exe` — skrót w menu Start, „Otwórz za pomocą” dla plików tekstowych i kodu, deinstalator.
- **Wersja przenośna:** `CynamonCode-1.1.0-windows-x64.zip` — rozpakuj w dowolnym miejscu i uruchom `bin\cynamoncode.exe`.

Pliki nie mają podpisu cyfrowego, więc Windows SmartScreen może pokazać „System Windows ochronił ten komputer”: wybierz **Więcej informacji → Uruchom mimo to**.

### macOS (12 Monterey lub nowszy, Apple Silicon i Intel)

Otwórz `CynamonCode-1.1.0-macOS.dmg` i przeciągnij **CynamonCode** do **Programy**. Aplikacja nie jest podpisana przez Apple, więc pierwsze uruchomienie zostanie zablokowane: kliknij aplikację prawym przyciskiem → **Otwórz** → **Otwórz** (albo zezwól w Ustawieniach systemowych → Prywatność i ochrona).

Żaden pakiet nie zmienia domyślnych programów ani ustawień systemu.

## Budowanie ze źródeł

Wymagania (Linux Mint 22.x / Ubuntu 24.04): C++20, CMake ≥ 3.21, Ninja, Qt 6 (Core, Gui, Widgets, Test), QScintilla 2 dla Qt 6. Pakiety systemowe:

```bash
sudo apt install build-essential cmake ninja-build git pkg-config \
  qt6-base-dev qt6-tools-dev qt6-tools-dev-tools qt6-qpa-plugins \
  libqscintilla2-qt6-dev libgl-dev
```

Budowanie, testy i uruchomienie:

```bash
bash scripts/bootstrap-mint.sh          # sprawdza zależności; instaluje tylko z --install i po potwierdzeniu
bash scripts/build.sh                   # Debug → build/
bash scripts/test.sh                    # ctest (testy GUI bez ekranu: QT_QPA_PLATFORM=offscreen)
./build/cynamoncode
```

Na Windows i macOS: Qt 6.8 i QScintilla zbudowana ze źródeł (jak w `.github/workflows/ci.yml`), potem poniższe polecenia CMake z `-DCMAKE_PREFIX_PATH=<katalog Qt>`; na Windows w wierszu poleceń dewelopera Visual Studio.

To samo bez skryptów:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

Pakiety i archiwum źródeł (CPack: `.deb` na Linuksie, instalator NSIS i `.zip` na Windows, `.dmg` na macOS):

```bash
bash scripts/build.sh release
cpack --config build-release/CPackConfig.cmake -B build-release/pakiety
cpack --config build-release/CPackSourceConfig.cmake -B build-release/pakiety
```

Presety CMake: `debug` (katalog `build/`), `release` (`build-release/`), testowy `debug-offscreen`. Niestandardową instalację QScintilli wskażesz przez `-DQScintillaQt6_ROOT=<katalog>`.

### Typowe problemy

- **„Could not find the Qt platform plugin”** — brak `qt6-qpa-plugins`. Bez ekranu (SSH, CI) testy GUI uruchamiaj z `QT_QPA_PLATFORM=offscreen` (robi to `scripts/test.sh` i preset `debug-offscreen`).
- **„Failed to find required Qt component Gui” / WrapOpenGL** — zainstaluj `libgl-dev`.
- **„Could NOT find XKB”** — opcjonalna zależność Qt6Gui; budowanie działa bez niej (`libxkbcommon-dev` usuwa komunikat).
- **`Qsci/qsciscintilla.h` not found** — brak `libqscintilla2-qt6-dev`; projekt wymaga wariantu Qt 6, wariant Qt 5 nie jest używany.
- **Polskie napisy w oknach wyboru pliku i czcionki** — pakiet `qt6-translations-l10n` (pakiet `.deb` go zaleca).

## Struktura repozytorium

| Ścieżka | Przeznaczenie |
|---|---|
| `src/` | kod: `domain/`, `services/`, `controllers/` (bez GUI), `editor/`, `ui/`, `app/` |
| `tests/` | testy Qt Test/CTest: `unit/`, `gui/`, `packaging/` |
| `cmake/` | wykrywanie QScintilla Qt6, reguły instalacji, pakiety (CPack) |
| `packaging/` | Linux: `.desktop`, AppStream, `man`; zasoby Windows; `Info.plist` macOS |
| `resources/` | ikony i logo źródłowe |
| `scripts/` | sprawdzanie zależności, budowanie, testy, generowanie ikon |
| `.github/workflows/` | CI: budowanie, testy i pakiety na Linuksie, Windows i macOS |
| `CHANGELOG.md` | historia zmian |

## Licencja

Copyright © 2026 Kamil Feliszewski

CynamonCode jest wolnym oprogramowaniem: możesz je rozpowszechniać i modyfikować na warunkach **GNU General Public License w wersji 3 lub (według Twojego wyboru) dowolnej późniejszej** (SPDX: `GPL-3.0-or-later`). Program jest rozpowszechniany bez jakiejkolwiek gwarancji. Pełny tekst: [`LICENSE`](LICENSE).

Program korzysta z Qt 6 (LGPLv3) i QScintilla (GPLv3). Projekt nie jest związany z Linux Mint ani środowiskiem Cinnamon.
