# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Kamil Feliszewski

# Każdy plik źródłowy ma na początku nagłówek licencji SPDX (D040, E5).
#
#   cmake -DSOURCE_DIR=<źródła> -P check_spdx.cmake

if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "Brak zmiennej SOURCE_DIR")
endif()

file(GLOB_RECURSE files RELATIVE "${SOURCE_DIR}"
    "${SOURCE_DIR}/src/*.h" "${SOURCE_DIR}/src/*.cpp"
    "${SOURCE_DIR}/tests/*.h" "${SOURCE_DIR}/tests/*.cpp" "${SOURCE_DIR}/tests/*.cmake"
    "${SOURCE_DIR}/tests/CMakeLists.txt"
    "${SOURCE_DIR}/cmake/*.cmake"
    "${SOURCE_DIR}/scripts/*.sh" "${SOURCE_DIR}/scripts/*.py"
    "${SOURCE_DIR}/packaging/linux/*")
list(APPEND files "CMakeLists.txt")

set(missing "")
set(checked 0)
foreach(file IN LISTS files)
    # Pierwsze ~400 bajtów: wiersz shebang, deklaracja XML lub komentarz troff.
    file(READ "${SOURCE_DIR}/${file}" head LIMIT 400)
    string(REGEX MATCH "SPDX-License-Identifier: (GPL-3\\.0-or-later|CC0-1\\.0)" license "${head}")
    string(FIND "${head}" "SPDX-FileCopyrightText: 2026 Kamil Feliszewski" copyright)
    if(license STREQUAL "" OR copyright EQUAL -1)
        list(APPEND missing "${file}")
    endif()
    math(EXPR checked "${checked} + 1")
endforeach()

if(missing)
    string(REPLACE ";" "\n  " list "${missing}")
    message(FATAL_ERROR "Pliki bez nagłówka SPDX:\n  ${list}")
endif()
message(STATUS "Nagłówki SPDX: ${checked} plików OK")
