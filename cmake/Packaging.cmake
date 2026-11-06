# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Kamil Feliszewski

# Pakiet .deb przez CPack (D044) i archiwum źródeł (GPL — kompletne źródła).
#
#   cmake --preset release && cmake --build --preset release
#   cpack --config build-release/CPackConfig.cmake -B build-release/pakiety
#   cpack --config build-release/CPackSourceConfig.cmake -B build-release/pakiety

set(CPACK_PACKAGE_NAME "cynamoncode")
set(CPACK_PACKAGE_VENDOR "Kamil Feliszewski")
set(CPACK_PACKAGE_CONTACT "Kamil Feliszewski <kamil@feliszewski.pl>")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "lightweight text and code editor with syntax highlighting")
# Wiersze krótsze niż 80 znaków — CPack nie zawija opisu sam.
string(JOIN "\n" CPACK_PACKAGE_DESCRIPTION
    "CynamonCode is a lightweight, native Qt 6 text and code editor with tabs,"
    "syntax highlighting for 12 languages, find and replace, two views, a file"
    "panel, session restore and configurable shortcuts. It deliberately offers"
    "no code completion, no linting and never runs the files you open."
    ""
    "Files are handled safely: UTF-8 only, atomic saving and detection of"
    "changes made by other programs.")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_STRIP_FILES ON)

set(CPACK_GENERATOR "DEB")
# Prefiks /usr to domyślna wartość generatora DEB; ustawiony globalnie trafiłby
# też do archiwum źródeł.
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT) # cynamoncode_<wersja>_<arch>.deb
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
set(CPACK_DEBIAN_PACKAGE_SECTION "editors")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${PROJECT_HOMEPAGE_URL}")
# Biblioteki (Qt6, QScintilla, libstdc++) wylicza dpkg-shlibdeps. Wtyczka platformy
# X11 (libqxcb.so) jest w Ubuntu 24.04 / Mint 22 częścią libqt6gui6t64, więc nie
# wymaga osobnej zależności.
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
# Polskie napisy w standardowych oknach Qt (wybór pliku, czcionki) — nie są wymagane.
set(CPACK_DEBIAN_PACKAGE_SUGGESTS "qt6-translations-l10n")
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION ON)

# Archiwum źródeł: bez katalogów budowania, historii Git i lokalnych ustawień.
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "cynamoncode-${PROJECT_VERSION}")
# Wzorce są dopasowywane do ścieżek bezwzględnych — zakotwiczone w katalogu źródeł,
# żeby np. projekt w ~/build/CynamonCode nie dał pustego archiwum. Podwójne
# ukośniki: wartość trafia jeszcze raz do pliku CPackSourceConfig.cmake.
string(REGEX REPLACE "([.+*?^$()|])" "\\\\\\\\\\1" _cynamoncode_source_re "${PROJECT_SOURCE_DIR}")
set(CPACK_SOURCE_IGNORE_FILES
    "^${_cynamoncode_source_re}/\\\\.git/"
    "^${_cynamoncode_source_re}/build/"
    "^${_cynamoncode_source_re}/build-[^/]*/"
    "^${_cynamoncode_source_re}/CMakeUserPresets\\\\.json$"
    "^${_cynamoncode_source_re}/compile_commands\\\\.json$"
    "^${_cynamoncode_source_re}/\\\\.claude/settings\\\\.local\\\\.json$"
    "^${_cynamoncode_source_re}/\\\\.vscode/"
    "^${_cynamoncode_source_re}/\\\\.idea/"
    "^${_cynamoncode_source_re}/\\\\.cache/")

include(CPack)
