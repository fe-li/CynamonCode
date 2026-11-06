# Historia zmian

Format oparty na [Keep a Changelog](https://keepachangelog.com/pl/1.1.0/). Wersje według [SemVer](https://semver.org/lang/pl/).

## [Niewydane]

### Dodano (2026-11-06)

- Menu Pomoc → „O programie CynamonCode…”: logo, nazwa, wersja, autor (Kamil Feliszewski), strona www (www.feliszewski.pl), wersje Qt i QScintilla.
- Ikona aplikacji z logo CynamonCode (okno, pasek zadań) w rozmiarach 16–512 px.
- Wersja programu: 0.1.0.
- Test: `tst_about`.
- Logo źródłowe w `resources/icons/cynamoncode-zrodlo.png` i skrypt `scripts/generate-icons.py` odtwarzający ikony (D033).

### Zmieniono (2026-11-06)

- Konfigurowalne skróty klawiszowe przeniesione z E3 do E5 (D032).

### Dodano (E3, 2026-11-06)

- Okno „Znajdź i zamień” (Ctrl+F, Ctrl+H, F3, Shift+F3): wielkość liter, całe wyrazy, wyrażenia regularne z grupami w zamianie, zawijanie; „Zamień wszystkie” cofa się jednym Ctrl+Z.
- Pasek stanu: wiersz i kolumna, liczba zaznaczonych znaków, język, kodowanie, końce linii.
- Ustawienia zapamiętywane w `~/.config/CynamonCode/cynamoncode.conf`: czcionka, motyw, zawijanie wierszy, numery wierszy, białe znaki, końce linii, powiększenie, rozmiar i położenie okna.
- Wykrywanie zmian otwartych plików przez inne programy: pytanie o przeładowanie lub zachowanie zmian, informacja o usuniętym pliku, ostrzeżenie przed nadpisaniem cudzej zmiany przy zapisie.
- Testy: `tst_search`, `tst_settings`, `tst_disk_changes`, `tst_find_replace`, `tst_external_changes`, `tst_settings_window`; wspólne `tests/gui/ScriptedPrompts.h`.
- Decyzje D026–D030.

### Weryfikacja E3 (2026-11-06)

- Przegląd kodu: poprawione wykrywanie zmian tej samej długości w tej samej chwili, zdarzenia dysku w trakcie otwartych pytań, obserwacja odtworzonego pliku, zagłodzenie zgłoszeń przez często zmieniany plik oraz drobne przypadki brzegowe wyszukiwania i przeładowania.
- Linux Mint 22.3: build od zera bez ostrzeżeń, `ctest` 17/17 PASS, testy GUI PASS offscreen i na X11, testy obserwacji plików stabilne w 20 przebiegach. Ręczna kontrola (użytkownik): OK.

### Poprawiono (2026-11-06)

- Plik HTML zapisany bez kropki w nazwie (np. „Nowy 1html”) otwierał się bez kolorowania: pliki bez rozszerzenia są teraz rozpoznawane po początku treści (`<!DOCTYPE html>`, `<html`, `<?xml`, `<?php`), a „Zapisz jako” nowego dokumentu podpowiada rozszerzenie zgodne z językiem (D025).

### Dodano (E2, 2026-11-06)

- Kolorowanie składni dla 12 języków (zwykły tekst, C/C++, Python, Java, JavaScript, HTML, CSS, PHP, SQL, JSON, XML, Bash) przez lexery QScintilla — bez jakiejkolwiek analizy poprawności.
- `LanguageRegistry`: wykrywanie po rozszerzeniu, znanych nazwach plików (`.bashrc`, `.profile` …) i shebangu; menu „Język” z ręcznym wyborem (ma pierwszeństwo, także po „Zapisz jako”).
- `ThemePalette` (motyw jasny i ciemny, menu Widok → Motyw) i `SyntaxHighlighter`: kategorie tokenów z opisów stylów; style „unclosed/unknown/error” w kolorze kategorii bazowej, jedno tło, bez wypełnień i podkreśleń.
- Mieszane końce linii: pytanie przy otwarciu (zachowaj / ujednolić do LF / do CRLF).
- Automatyczne wcięcia: Enter zachowuje wcięcie, w C/C++, Javie, JavaScript, CSS i PHP „{” wcina, a „}” cofa; tabulatory dla plików nimi wciętych, w pozostałych 4 spacje (D024).
- Wcięcia według znaczników w HTML, XML i części HTML plików PHP: znacznik otwierający wcina następną linię, „</” na początku linii cofa wcięcie; elementy puste (`<br>`, `<img>` …), samozamykające, komentarze i treść `<script>` nie zmieniają wcięć.
- Testy: `tst_language_registry`, `tst_syntax_highlighting`, `tst_indentation`, `tst_auto_indent`; rozszerzone `tst_tabs` i `tst_document_controller`.
- Decyzje D019–D024.

### Weryfikacja E2 (2026-11-06)

- Linux Mint 22.3: build od zera bez ostrzeżeń, `ctest` 11/11 PASS (0 pominiętych), testy GUI PASS offscreen i na X11. Ręczna kontrola (użytkownik): OK.

### Dodano (E1, 2026-11-05)

- `EditorWidget` (QsciScintilla): UTF-8, numery linii, podświetlenie bieżącej linii; autouzupełnianie i call tips wyłączone w konstruktorze i po każdej zmianie lexera (D013).
- Karty dokumentów i menu Plik/Edycja: Nowy, Otwórz, Zapisz, Zapisz jako, Zapisz wszystkie, Zamknij kartę, Cofnij, Ponów, Wytnij, Kopiuj, Wklej, Zaznacz wszystko, Przejdź do wiersza; status kursora w pasku stanu.
- Ochrona niezapisanych zmian przy zamykaniu karty i okna (Zapisz / Nie zapisuj / Anuluj); nieudany zapis nie czyści flagi modyfikacji.
- `EncodingService`: UTF-8 z BOM i bez, odmowa bajtów NUL i niepoprawnego UTF-8 (bez U+FFFD), rozpoznawanie LF/CRLF/mieszanych.
- `FileService`: odczyt bajtów, zapis `QSaveFile` bez `directWriteFallback`; odmowa dla plików tylko do odczytu, zapis przez dowiązania symboliczne do pliku docelowego.
- `DocumentManager`, `DocumentController`: rejestr dokumentów, deduplikacja kart po ścieżce kanonicznej.
- `UserPrompts`: interfejs dialogów (testy GUI bez okien modalnych).
- Testy: `tst_encoding`, `tst_fileservice`, `tst_document_controller`, `tst_editor_no_suggestions`, `tst_tabs`.
- Decyzje D013–D018 w `docs/DECYZJE.md`.

### Weryfikacja E1 (2026-11-06)

- Linux Mint 22.3 (przez SSH): build Debug od zera OK, 0 ostrzeżeń; `ctest` 7/7 PASS (0 pominiętych); testy GUI PASS w trybie offscreen i na ekranie X11. Ręczna kontrola w programie (użytkownik): OK.

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

- Linux Mint 22.3 (VirtualBox; g++ 13.3.0, CMake 3.28.3, Qt 6.4.2, QScintilla 2.14.1): Debug configure/build OK, `ctest` 2/2 PASS, okno sprawdzone ręcznie (działa). Preset `release` i ostrzeżenia przy pełnym przebudowaniu — NIEZWERYFIKOWANE.
- Na maszynie roboczej (Windows 11, bez narzędzi budowania) sprawdzono tylko `bash -n` skryptów, poprawność JSON presetów oraz brak CR i BOM.
- Przegląd kodu (subagent `recenzent-kodu`) zamiast kompilacji. Naprawiono: średnik w `REASON_FAILURE_MESSAGE` FindQScintillaQt6 (przerywałby każdą konfigurację); dodano `qt6-qpa-plugins` do zależności; preset testowy `debug-offscreen`; `tests/fixtures/** -text` w `.gitattributes`, by fixture'y CRLF/mixed nie były normalizowane.
