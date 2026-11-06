# Historia zmian

Format oparty na [Keep a Changelog](https://keepachangelog.com/pl/1.1.0/). Wersje według [SemVer](https://semver.org/lang/pl/).

## [Niewydane]

## [1.0.0] — 2026-11-06

Pierwsza stabilna wersja (E5), tag `v1.0.0`. Zawiera także zmiany z wersji 0.1 i 0.2 opisane niżej.

### Dodano (po E5, przed tagiem, 2026-11-06)

- Ustawienia → „Domyślny program dla plików…”: wybrane typy (HTML, CSS, JavaScript, PHP i inne z wpisu menu) otwierają się dwuklikiem w CynamonCode; zapis tylko dla bieżącego konta w `~/.config/mimeapps.list`, odznaczenie przywraca poprzedni program (D047). Testy `tst_mime_apps`, `tst_default_apps`.
- Pakiet `.deb`: `qt6-translations-l10n` w polu `Recommends` zamiast `Suggests` — `apt` domyślnie instaluje polskie napisy standardowych okien Qt (wybór pliku, czcionki).

### Dodano (E5, 2026-11-06)

- Konfigurowalne skróty klawiszowe: Ustawienia → „Skróty klawiszowe…” (wyszukiwanie polecenia, przypisanie, usunięcie, przywrócenie domyślnych). Zajęty skrót jest przenoszony z informacją; w ustawieniach zapisywane są tylko zmiany (D045).
- Klawiatura: Ctrl+Tab / Ctrl+Shift+Tab i Ctrl+PgDown / Ctrl+PgUp przełączają karty, F8 przenosi kursor do drugiego widoku; nazwy dostępności dla edytorów, paneli i paska stanu (D046).
- Pomoc programu (F1): opis funkcji, zasady pracy z plikami i aktualna lista skrótów, bez sieci.
- Wiersz poleceń: `--help`, `--version`; polskie napisy w standardowych oknach Qt, gdy w systemie są tłumaczenia Qt.
- Integracja z systemem: identyfikator `pl.feliszewski.CynamonCode`, wpis w menu (Akcesoria) z ikoną, typy MIME dla „Otwórz za pomocą” bez zmiany domyślnych programów, metadane AppStream, strona `man` (D041–D043).
- Pakiet `cynamoncode_1.0.0_amd64.deb` (CPack, zależności z `dpkg-shlibdeps`, plik `copyright` w formacie DEP-5) i archiwum źródeł `cynamoncode-1.0.0.tar.gz` (D044).
- Nagłówki `SPDX-License-Identifier: GPL-3.0-or-later` we wszystkich plikach źródłowych.
- Testy: `tst_shortcuts`, test pomocy w `tst_about`, skróty w `tst_settings`, `tst_cli_version`, `tst_cli_help`, `tst_install_layout`, `tst_spdx_headers`.

### Weryfikacja E5 (2026-11-06)

- Przegląd kodu: skróty tylko z jednej kombinacji klawiszy, klawisze poleceń zdejmowane z wbudowanej mapy klawiszy QScintilli (np. Ctrl+T nie zamienia już wierszy, gdy jest skrótem polecenia), błędny wpis w ustawieniach zostawia skrót domyślny, Ctrl+Shift+Tab z X11 rozpoznawany, `--version` bez ekranu, poprawki pakietu i archiwum źródeł.
- Linux Mint 22.3: build Debug i Release od zera bez ostrzeżeń, `ctest` 29/29 PASS (offscreen, X11, Release), archiwum źródeł buduje się samodzielnie, pakiet `.deb` z poprawnymi uprawnieniami i zależnościami. Instalacja i odinstalowanie pakietu (użytkownik): poprawny wpis w menu, „Otwórz za pomocą” bez zmiany domyślnych programów, po odinstalowaniu nie zostają żadne pliki.

### Zmieniono (E5)

- Wersja programu 1.0.0 (`project(VERSION)`).
- Ikona okna dopasowana do wpisu w menu przez identyfikator aplikacji zamiast nazwy `cynamoncode`.


### Dodano (2026-11-06)

- Licencja GPL-3.0-or-later: plik `LICENSE` (oficjalny tekst GNU GPL v3), sekcja w README i informacja w oknie „O programie” (D040).

### Dodano (E4, 2026-11-06)

- Panel plików: Plik → „Otwórz folder…” (Ctrl+Shift+O), drzewo folderu z filtrem nazw, otwieranie plików dwuklikiem.
- Przywracanie sesji przy starcie (włączone domyślnie, Ustawienia → „Przywracaj sesję przy starcie”): karty w obu panelach, aktywna karta, kursor, ręcznie wybrany język, otwarty folder.
- Dwa panele kart: „Przenieś do drugiego widoku” i „Sklonuj do drugiego widoku” (wspólny bufor, edycja widoczna w obu).
- „Znajdź we wszystkich otwartych” (Ctrl+Shift+F) z panelem wyników oraz „Zamień we wszystkich otwartych” z potwierdzeniem.
- Pytanie przed otwarciem pliku większego niż 20 MiB; pliki z wiersza poleceń.
- Testy: `tst_session`, `tst_split_view`, `tst_session_window`, `tst_workspace`, `tst_find_in_all`, `tst_large_files` (etykieta `perf`).
- Decyzje D034–D039.

### Zmieniono (E4)

- Okno główne przebudowane na dwa panele; każda operacja (zapis, zamykanie, zmiany na dysku, język) działa raz na dokument, niezależnie od liczby widoków.
- „Zamień wszystkie” przy wielu trafieniach podmienia zakres jedną operacją: 100 000 zamian w pliku 4,7 MiB — 142 ms (wcześniej ponad 5 minut).

### Weryfikacja E4 (2026-11-06)

- Przegląd kodu: poprawione zachowanie końców linii CRLF w klonach, zgodność aktywnego panelu z fokusem klawiatury, szybkość podglądu wyników na długich wierszach, kursor i przewinięcie przy zbiorczej zamianie, tworzenie katalogu ustawień, przycinanie pozycji z sesji, brak zapisu sesji przy wyłączonym przywracaniu.
- Linux Mint 22.3: build od zera bez ostrzeżeń, `ctest` 24/24 PASS, testy GUI PASS offscreen i na X11, testy zależne od czasu stabilne w 50 przebiegach. Ręczna kontrola (użytkownik): OK.

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
