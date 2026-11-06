# CynamonCode

**CynamonCode** to lekki, natywny edytor tekstu i kodu dla Linux Mint i innych dystrybucji Linuksa. Wygodą edycji ma przypominać Notepad++, ale ma własną implementację, nazwę i identyfikację graficzną.

> **Stan: wersja 0.2 (etapy E0–E4 ukończone).** Następny etap: E5 — wydanie 1.0 z pakietem `.deb` (`TASKS.md`).

## Funkcje

- Karty i dwa panele (przenoszenie, klon ze wspólnym buforem), panel plików z filtrem, przywracanie sesji.
- Kolorowanie składni 12 języków, motyw jasny i ciemny, automatyczne wcięcia.
- Znajdź i zamień (także wyrażenia regularne), szukanie i zamiana we wszystkich otwartych plikach.
- Bezpieczne pliki: tylko UTF-8 bez cichych zmian bajtów, zapis atomowy, wykrywanie zmian na dysku, ochrona niezapisanych zmian.
- Pasek stanu, ustawienia zapamiętywane między uruchomieniami.

## Technologie

C++20 · Qt 6 Widgets · QScintilla 2 dla Qt6 · CMake · Ninja · Qt Test/CTest.

**Kluczowe ograniczenie:** ma być **kolorowanie składni**, ale **bez podpowiadania kodu, autouzupełniania, call tips, LSP, sprawdzania poprawności i oznaczania błędów**. Nie ma też uruchamiania ani kompilacji kodu użytkownika.

## Budowanie (Linux Mint 22.x / Ubuntu 24.04)

1. Sprawdź zależności (skrypt niczego nie instaluje bez `--install` i Twojego potwierdzenia):

   ```bash
   bash scripts/bootstrap-mint.sh
   ```

2. Zbuduj wersję Debug (katalog `build/`) i uruchom testy:

   ```bash
   bash scripts/build.sh
   bash scripts/test.sh
   ```

   To samo bez skryptów:

   ```bash
   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
   cmake --build build --parallel
   ctest --test-dir build --output-on-failure
   ```

   Bez serwera wyświetlania testy GUI wymagają `QT_QPA_PLATFORM=offscreen` (`scripts/test.sh` ustawia to sam).

3. Uruchom program:

   ```bash
   ./build/cynamoncode
   ```

Wersja Release: `bash scripts/build.sh release` (katalog `build-release/`). Szczegóły i typowe problemy: `docs/LINUX_MINT.md`.

## Struktura

| Ścieżka | Przeznaczenie |
|---|---|
| `CMakeLists.txt`, `CMakePresets.json` | konfiguracja budowania (presety `debug` i `release`) |
| `cmake/FindQScintillaQt6.cmake` | wykrywanie QScintilla Qt6 po nagłówku i bibliotece |
| `src/` | kod aplikacji: `domain/`, `services/`, `controllers/` (bez GUI), `editor/`, `ui/`, `app/` |
| `resources/` | zasoby Qt: ikony i logo źródłowe |
| `tests/` | testy Qt Test/CTest (`unit/`, `gui/`; wydajność z etykietą `perf`) |
| `scripts/` | `bootstrap-mint.sh`, `build.sh`, `test.sh`, `generate-icons.py` |
| `LICENSE` | pełny tekst GNU GPL v3 |
| `CLAUDE.md`, `START_CLAUDE.md` | reguły i polecenie startowe dla Claude Code |
| `docs/SPECYFIKACJA.md` | wymagania i zachowanie aplikacji |
| `docs/ARCHITEKTURA.md` | granice modułów, stany, przepływy |
| `docs/ETAPY.md` | etapy E0–E5 i kryteria akceptacji |
| `docs/TESTY.md` | macierz testów |
| `docs/DECYZJE.md` | rejestr decyzji i otwarte kwestie |
| `docs/LINUX_MINT.md` | zależności, kompilacja, rozwiązywanie problemów |
| `TASKS.md`, `CHANGELOG.md` | stan prac i historia zmian |
| `.claude/agents/*.md` | subagenci: architektura, przegląd kodu, testy |

## Licencja

Copyright © 2026 Kamil Feliszewski

CynamonCode jest wolnym oprogramowaniem: możesz je rozpowszechniać i modyfikować na warunkach **GNU General Public License w wersji 3 lub (według Twojego wyboru) dowolnej późniejszej** (SPDX: `GPL-3.0-or-later`), opublikowanej przez Free Software Foundation. Program jest rozpowszechniany bez jakiejkolwiek gwarancji. Pełny tekst licencji: plik [`LICENSE`](LICENSE).

Program korzysta z Qt 6 (LGPLv3/GPL) i QScintilla (GPLv3 lub licencja komercyjna) — obie są zgodne z GPL-3.0-or-later.

## Nazwa

Nazwa **CynamonCode** jest wybrana na potrzeby projektu. Przed publiczną dystrybucją sprawdź kolizje nazw, znaków towarowych i identyfikatorów pakietów. Projekt nie jest oficjalnie związany z Linux Mint ani środowiskiem Cinnamon.
