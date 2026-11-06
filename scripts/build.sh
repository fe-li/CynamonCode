#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Kamil Feliszewski
# Konfiguruje i buduje CynamonCode przez CMakePresets.json.
#
#   bash scripts/build.sh            — Debug (katalog build/)
#   bash scripts/build.sh release    — Release (katalog build-release/)
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

cmake --preset "$preset"
cmake --build --preset "$preset" --parallel
