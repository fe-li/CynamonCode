# Historia zmian

Format oparty na [Keep a Changelog](https://keepachangelog.com/pl/1.1.0/). Wersje według [SemVer](https://semver.org/lang/pl/).

## [Niewydane]

### Dodano (E0, 2026-11-05)

- Repozytorium Git (`main`), `.gitignore`, `.gitattributes` (końce linii LF).
- `CMakeLists.txt` (C++20, Qt6 Core/Gui/Widgets, ostrzeżenia `-Wall -Wextra -Wpedantic`) i `CMakePresets.json` (Debug/Release).
- `cmake/FindQScintillaQt6.cmake`: wykrywanie QScintilla Qt6 po `Qsci/qsciscintilla.h` i `qscintilla2_qt6`, odmowa nagłówków Qt5, target `QScintilla::QScintilla`.
- `src/main.cpp` i `MainWindow` z tytułem „CynamonCode”, menu „Plik → Zakończ” (Ctrl+Q) i paskiem stanu.
- Smoke testy Qt Test/CTest: `tst_qscintilla_link` (linkowanie QScintilla Qt6, brak Qt5 w procesie) i `tst_mainwindow_smoke` (tytuł, pokazanie i zamknięcie okna).
- Skrypty `scripts/bootstrap-mint.sh`, `scripts/build.sh`, `scripts/test.sh`.

### Zmieniono

- Dokumentacja: ujednolicone rozbieżności między SPECYFIKACJA, TESTY, ETAPY i ARCHITEKTURA (rejestr w `docs/DECYZJE.md`).

### Weryfikacja

- Build i testy: **NIEZWERYFIKOWANE** — na maszynie roboczej (Windows 11, bez WSL, CMake, kompilatora i Qt) nie można było ich uruchomić. Sprawdzono tylko `bash -n` skryptów, poprawność JSON presetów oraz brak CR i BOM we wszystkich plikach.
- Przegląd kodu (subagent `recenzent-kodu`) zamiast kompilacji. Naprawiono: średnik w `REASON_FAILURE_MESSAGE` FindQScintillaQt6 (przerywałby każdą konfigurację); dodano `qt6-qpa-plugins` do zależności; preset testowy `debug-offscreen`; `tests/fixtures/** -text` w `.gitattributes`, by fixture'y CRLF/mixed nie były normalizowane.
