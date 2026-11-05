#!/usr/bin/env bash
# Uruchamia testy CTest dla zbudowanego presetu (najpierw scripts/build.sh).
#
#   bash scripts/test.sh            — Debug
#   bash scripts/test.sh release    — Release
#
# Bez serwera wyświetlania ustawia QT_QPA_PLATFORM=offscreen. Tryb offscreen
# nie zastępuje ręcznej weryfikacji wyglądu okna.
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."

preset="${1:-debug}"
case "$preset" in
    debug|release) ;;
    *)
        echo "Użycie: $0 [debug|release]" >&2
        exit 2
        ;;
esac

if [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" && -z "${QT_QPA_PLATFORM:-}" ]]; then
    export QT_QPA_PLATFORM=offscreen
    echo "Brak serwera wyświetlania: testy GUI w trybie QT_QPA_PLATFORM=offscreen."
fi

ctest --preset "$preset"
