# CynamonCode

**CynamonCode** to lekki, natywny edytor tekstu i kodu dla Linux Mint i innych dystrybucji Linuksa. Wygodą edycji ma przypominać Notepad++, ale ma własną implementację, nazwę i identyfikację graficzną.

> **Stan: etap E0.** Jest szkielet projektu CMake, puste okno główne i smoke testy. Edytor, karty i operacje na plikach powstaną w kolejnych etapach (`TASKS.md`).

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
| `src/` | kod aplikacji (`main.cpp`, `ui/MainWindow`) |
| `tests/` | testy Qt Test/CTest (`unit/`, `gui/`) |
| `scripts/` | `bootstrap-mint.sh`, `build.sh`, `test.sh` |
| `CLAUDE.md`, `START_CLAUDE.md` | reguły i polecenie startowe dla Claude Code |
| `docs/SPECYFIKACJA.md` | wymagania i zachowanie aplikacji |
| `docs/ARCHITEKTURA.md` | granice modułów, stany, przepływy |
| `docs/ETAPY.md` | etapy E0–E5 i kryteria akceptacji |
| `docs/TESTY.md` | macierz testów |
| `docs/DECYZJE.md` | rejestr decyzji i otwarte kwestie |
| `docs/LINUX_MINT.md` | zależności, kompilacja, rozwiązywanie problemów |
| `TASKS.md`, `CHANGELOG.md` | stan prac i historia zmian |
| `.claude/agents/*.md` | subagenci: architektura, przegląd kodu, testy |

## Nazwa i licencje

Nazwa **CynamonCode** jest wybrana na potrzeby projektu. Przed publiczną dystrybucją sprawdź kolizje nazw, znaków towarowych i identyfikatorów pakietów. Projekt nie jest oficjalnie związany z Linux Mint ani środowiskiem Cinnamon.

QScintilla jest dostępna na licencji GPLv3 lub komercyjnej. Planowana licencja projektu to GPL-3.0-or-later (decyzja D011, wstępna); plik `LICENSE` zostanie dodany po jej zatwierdzeniu (`docs/DECYZJE.md`).
