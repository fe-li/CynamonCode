# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Kamil Feliszewski

# T23 (część automatyczna): instalacja do katalogu tymczasowego i kontrola
# kompletnej listy plików — nic ponad to, co pakiet ma zawierać (D044).
#
#   cmake -DBUILD_DIR=<build> -DWORK_DIR=<katalog> -DAPP_ID=<id> -P check_install.cmake

foreach(var BUILD_DIR WORK_DIR APP_ID)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "Brak zmiennej ${var}")
    endif()
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
set(prefix "${WORK_DIR}/usr")
execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${BUILD_DIR}" --prefix "${prefix}"
    RESULT_VARIABLE result
    OUTPUT_QUIET)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "cmake --install zakończone błędem: ${result}")
endif()

set(expected
    "bin/cynamoncode"
    "share/applications/${APP_ID}.desktop"
    "share/metainfo/${APP_ID}.metainfo.xml"
    "share/doc/cynamoncode/copyright"
    "share/doc/cynamoncode/LICENSE")
foreach(size IN ITEMS 16 24 32 48 64 128 256 512)
    list(APPEND expected "share/icons/hicolor/${size}x${size}/apps/${APP_ID}.png")
endforeach()
if(EXISTS "${BUILD_DIR}/cynamoncode.1.gz")
    list(APPEND expected "share/man/man1/cynamoncode.1.gz")
else()
    list(APPEND expected "share/man/man1/cynamoncode.1")
endif()

file(GLOB_RECURSE installed LIST_DIRECTORIES false RELATIVE "${prefix}" "${prefix}/*")
list(SORT installed)
list(SORT expected)
if(NOT installed STREQUAL expected)
    string(REPLACE ";" "\n  " got "${installed}")
    string(REPLACE ";" "\n  " want "${expected}")
    message(FATAL_ERROR "Niezgodna lista plików.\nZainstalowane:\n  ${got}\nOczekiwane:\n  ${want}")
endif()

# Wpis w menu: polecenie z plikami (%F), ikona i typy MIME (D043).
file(READ "${prefix}/share/applications/${APP_ID}.desktop" desktop)
foreach(line IN ITEMS "Exec=cynamoncode %F" "Icon=${APP_ID}" "Type=Application"
        "Categories=Utility;TextEditor;Qt;")
    string(FIND "${desktop}" "\n${line}\n" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Brak wiersza w pliku .desktop: ${line}")
    endif()
endforeach()
foreach(mime IN ITEMS text/plain text/x-c++src text/x-python text/html application/json)
    string(FIND "${desktop}" "${mime};" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Brak typu MIME w pliku .desktop: ${mime}")
    endif()
endforeach()

# Licencja w copyright (DEP-5) i pełny tekst GPLv3.
file(READ "${prefix}/share/doc/cynamoncode/copyright" copyright)
string(FIND "${copyright}" "License: GPL-3.0-or-later" position)
if(position EQUAL -1)
    message(FATAL_ERROR "Plik copyright nie wskazuje licencji GPL-3.0-or-later")
endif()
file(SHA256 "${prefix}/share/doc/cynamoncode/LICENSE" license_hash)
if(NOT license_hash STREQUAL "3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986")
    message(FATAL_ERROR "LICENSE różni się od oficjalnego tekstu GPLv3 (D040)")
endif()

# Narzędzia walidacji, jeśli są w systemie (desktop-file-utils, appstream).
find_program(DESKTOP_FILE_VALIDATE desktop-file-validate)
if(DESKTOP_FILE_VALIDATE)
    execute_process(
        COMMAND "${DESKTOP_FILE_VALIDATE}" "${prefix}/share/applications/${APP_ID}.desktop"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE output)
    if(NOT result EQUAL 0 OR NOT output STREQUAL "")
        message(FATAL_ERROR "desktop-file-validate:\n${output}")
    endif()
    message(STATUS "desktop-file-validate: OK")
else()
    message(STATUS "desktop-file-validate: brak narzędzia — POMINIĘTE")
endif()

find_program(APPSTREAMCLI appstreamcli)
if(APPSTREAMCLI)
    execute_process(
        COMMAND "${APPSTREAMCLI}" validate --no-net
                "${prefix}/share/metainfo/${APP_ID}.metainfo.xml"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE output)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "appstreamcli validate:\n${output}")
    endif()
    message(STATUS "appstreamcli validate: OK\n${output}")
else()
    message(STATUS "appstreamcli: brak narzędzia — POMINIĘTE")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
message(STATUS "Instalacja: OK")
