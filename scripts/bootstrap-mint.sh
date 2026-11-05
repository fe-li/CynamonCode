#!/usr/bin/env bash
# Sprawdza zależności CynamonCode na Linux Mint / Ubuntu (dpkg/apt).
#
#   bash scripts/bootstrap-mint.sh            — tylko raport, nic nie instaluje
#   bash scripts/bootstrap-mint.sh --install  — po jawnym potwierdzeniu uruchamia sudo apt
#
# Skrypt nigdy nie wywołuje sudo bez potwierdzenia człowieka w terminalu.
set -euo pipefail

REQUIRED_PACKAGES=(
    build-essential
    cmake
    ninja-build
    git
    pkg-config
    qt6-base-dev
    qt6-tools-dev
    qt6-tools-dev-tools
    qt6-qpa-plugins      # wtyczki platform (xcb, offscreen) — bez nich okno i testy GUI nie wystartują
    libqscintilla2-qt6-dev
    libgl-dev            # Qt6::Gui w CMake wymaga nagłówków OpenGL (WrapOpenGL)
)
OPTIONAL_PACKAGES=(
    qtcreator
)

install_requested=0
case "${1:-}" in
    "") ;;
    --install) install_requested=1 ;;
    -h|--help)
        sed -n '2,8p' "$0"
        exit 0
        ;;
    *)
        echo "Nieznana opcja: $1 (użyj --install lub --help)" >&2
        exit 2
        ;;
esac

if [[ -r /etc/os-release ]]; then
    ( . /etc/os-release; echo "System: ${PRETTY_NAME:-nieznany}" )
fi

if ! command -v dpkg-query >/dev/null 2>&1; then
    echo "Brak dpkg-query: skrypt obsługuje tylko dystrybucje oparte na Debianie/Ubuntu." >&2
    exit 2
fi

is_installed() {
    [[ "$(dpkg-query -W -f='${Status}' "$1" 2>/dev/null || true)" == *"install ok installed"* ]]
}

echo
echo "Narzędzia:"
for tool in cmake ninja g++ git; do
    if command -v "$tool" >/dev/null 2>&1; then
        printf '  %-6s %s\n' "$tool" "$("$tool" --version 2>/dev/null | head -n 1)"
    else
        printf '  %-6s brak\n' "$tool"
    fi
done

echo
echo "Pakiety wymagane:"
missing=()
for pkg in "${REQUIRED_PACKAGES[@]}"; do
    if is_installed "$pkg"; then
        version="$(dpkg-query -W -f='${Version}' "$pkg" 2>/dev/null)"
        printf '  [OK]   %-26s %s\n' "$pkg" "$version"
    else
        printf '  [BRAK] %s\n' "$pkg"
        missing+=("$pkg")
    fi
done

echo
echo "Pakiety opcjonalne:"
for pkg in "${OPTIONAL_PACKAGES[@]}"; do
    if is_installed "$pkg"; then
        printf '  [OK]   %s\n' "$pkg"
    else
        printf '  [—]    %s (nie jest wymagany do budowania)\n' "$pkg"
    fi
done

echo
if (( ${#missing[@]} == 0 )); then
    echo "Wszystkie wymagane pakiety są zainstalowane."
    exit 0
fi

echo "Brakujące pakiety można zainstalować poleceniem (do wykonania przez właściciela maszyny):"
echo "  sudo apt update && sudo apt install ${missing[*]}"
echo "Przed instalacją sprawdź dostępność: apt-cache policy ${missing[*]}"

if (( install_requested == 0 )); then
    exit 1
fi

if [[ ! -t 0 ]]; then
    echo "Instalacja wymaga interaktywnego terminala — przerwano." >&2
    exit 1
fi

read -r -p "Uruchomić teraz instalację przez sudo? [t/N] " answer
case "$answer" in
    t|T|tak|TAK)
        sudo apt update
        sudo apt install "${missing[@]}"
        ;;
    *)
        echo "Anulowano — nic nie zainstalowano."
        exit 1
        ;;
esac
