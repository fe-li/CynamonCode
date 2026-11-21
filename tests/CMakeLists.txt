# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Kamil Feliszewski

# Komponent Test jest potrzebny wyłącznie przy BUILD_TESTING=ON.
find_package(Qt6 REQUIRED COMPONENTS Test)

# cynamoncode_add_qt_test(<nazwa> <etykieta> <plik.cpp> [biblioteki...])
function(cynamoncode_add_qt_test name label source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE Qt6::Test cynamoncode_warnings ${ARGN})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES LABELS "${label}")
endfunction()

# Testy bez GUI.
cynamoncode_add_qt_test(tst_qscintilla_link unit unit/tst_qscintilla_link.cpp
    QScintilla::QScintilla)
cynamoncode_add_qt_test(tst_encoding unit unit/tst_encoding.cpp cynamoncode_core)
cynamoncode_add_qt_test(tst_fileservice unit unit/tst_fileservice.cpp cynamoncode_core)
cynamoncode_add_qt_test(tst_document_controller unit unit/tst_document_controller.cpp
    cynamoncode_core)
cynamoncode_add_qt_test(tst_language_registry unit unit/tst_language_registry.cpp cynamoncode_core)
cynamoncode_add_qt_test(tst_indentation unit unit/tst_indentation.cpp cynamoncode_core)
cynamoncode_add_qt_test(tst_search unit unit/tst_search.cpp cynamoncode_core)
cynamoncode_add_qt_test(tst_settings unit unit/tst_settings.cpp cynamoncode_core)
cynamoncode_add_qt_test(tst_session unit unit/tst_session.cpp cynamoncode_core)
cynamoncode_add_qt_test(tst_disk_changes unit unit/tst_disk_changes.cpp cynamoncode_core)
cynamoncode_add_qt_test(tst_mime_apps unit unit/tst_mime_apps.cpp cynamoncode_core)

# Testy GUI: bez serwera wyświetlania uruchamiaj z QT_QPA_PLATFORM=offscreen
# (scripts/test.sh robi to sam; preset testowy debug-offscreen).
cynamoncode_add_qt_test(tst_mainwindow_smoke gui gui/tst_mainwindow_smoke.cpp cynamoncode_ui)
cynamoncode_add_qt_test(tst_editor_no_suggestions gui gui/tst_editor_no_suggestions.cpp
    cynamoncode_ui)
cynamoncode_add_qt_test(tst_tabs gui gui/tst_tabs.cpp cynamoncode_ui)
cynamoncode_add_qt_test(tst_syntax_highlighting gui gui/tst_syntax_highlighting.cpp cynamoncode_ui)
cynamoncode_add_qt_test(tst_auto_indent gui gui/tst_auto_indent.cpp cynamoncode_ui)
cynamoncode_add_qt_test(tst_find_replace gui gui/tst_find_replace.cpp cynamoncode_ui)
cynamoncode_add_qt_test(tst_external_changes gui gui/tst_external_changes.cpp cynamoncode_ui)
cynamoncode_add_qt_test(tst_settings_window gui gui/tst_settings_window.cpp cynamoncode_ui)
cynamoncode_add_qt_test(tst_about gui gui/tst_about.cpp cynamoncode_ui)
cynamoncode_add_qt_test(tst_split_view gui gui/tst_split_view.cpp cynamoncode_ui)
cynamoncode_add_qt_test(tst_session_window gui gui/tst_session_window.cpp cynamoncode_ui)
cynamoncode_add_qt_test(tst_workspace gui gui/tst_workspace.cpp cynamoncode_ui)
cynamoncode_add_qt_test(tst_find_in_all gui gui/tst_find_in_all.cpp cynamoncode_ui)
cynamoncode_add_qt_test(tst_shortcuts gui gui/tst_shortcuts.cpp cynamoncode_ui)
cynamoncode_add_qt_test(tst_windows1250 gui gui/tst_windows1250.cpp cynamoncode_ui)
# Domyślny program przez mimeapps.list — tylko Linux (Windows i macOS: D053).
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    cynamoncode_add_qt_test(tst_default_apps gui gui/tst_default_apps.cpp cynamoncode_ui)
    target_compile_definitions(tst_default_apps PRIVATE
        CYNAMONCODE_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
endif()

# Testy zależne od fokusu klawiatury i od czasu zdarzeń obserwacji plików nie
# biegną równolegle z innymi (ctest -j) — na prawdziwym pulpicie okna innych
# testów zabierają fokus, a obciążenie opóźnia zdarzenia systemu plików.
set_tests_properties(tst_shortcuts tst_disk_changes tst_external_changes tst_split_view
    PROPERTIES RUN_SERIAL TRUE)

# Wydajność (D039): łagodne limity czasu; etykieta perf pozwala pominąć: ctest -LE perf
cynamoncode_add_qt_test(tst_large_files perf gui/tst_large_files.cpp cynamoncode_ui)
set_tests_properties(tst_large_files PROPERTIES TIMEOUT 300)

# Wydanie (E5): wiersz poleceń programu, instalacja, nagłówki licencji.
# --version bez serwera wyświetlania (np. przez SSH) — nie może wymagać ekranu.
add_test(NAME tst_cli_version COMMAND cynamoncode --version)
set_tests_properties(tst_cli_version PROPERTIES
    LABELS release
    PASS_REGULAR_EXPRESSION "^cynamoncode ${PROJECT_VERSION}\r?\n$")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_tests_properties(tst_cli_version PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=xcb;DISPLAY=;WAYLAND_DISPLAY=")
endif()
add_test(NAME tst_cli_help COMMAND cynamoncode --help)
set_tests_properties(tst_cli_help PROPERTIES
    LABELS release
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    PASS_REGULAR_EXPRESSION "--version.*pliki")
# Windows: program okienkowy — Qt pokazałby tekst w oknie komunikatu zamiast na wyjściu.
if(WIN32)
    set_property(TEST tst_cli_version tst_cli_help APPEND PROPERTY
        ENVIRONMENT "QT_COMMAND_LINE_PARSER_NO_GUI_MESSAGE_BOXES=1")
endif()
# Układ instalacji pakietu .deb (FHS) — tylko Linux.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_test(NAME tst_install_layout
        COMMAND "${CMAKE_COMMAND}" "-DBUILD_DIR=${PROJECT_BINARY_DIR}"
                "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/instalacja-testowa"
                "-DAPP_ID=${CYNAMONCODE_APP_ID}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/packaging/check_install.cmake")
    set_tests_properties(tst_install_layout PROPERTIES LABELS release)
endif()
add_test(NAME tst_spdx_headers
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/packaging/check_spdx.cmake")
set_tests_properties(tst_spdx_headers PROPERTIES LABELS release)

# Windows: biblioteki DLL Qt i QScintilli nie leżą obok programów testowych —
# CTest dopisuje ich katalogi do PATH (bez kopiowania i bez zmian w systemie).
if(WIN32)
    set(_cynamoncode_runtime_dirs "")
    foreach(_config IN ITEMS RELEASE DEBUG)
        get_target_property(_location Qt6::Core IMPORTED_LOCATION_${_config})
        if(_location)
            get_filename_component(_dir "${_location}" DIRECTORY)
            list(APPEND _cynamoncode_runtime_dirs "${_dir}")
        endif()
    endforeach()
    foreach(_dll IN ITEMS "${QScintillaQt6_DLL_RELEASE}" "${QScintillaQt6_DLL_DEBUG}")
        if(_dll)
            get_filename_component(_dir "${_dll}" DIRECTORY)
            list(APPEND _cynamoncode_runtime_dirs "${_dir}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES _cynamoncode_runtime_dirs)
    set(_cynamoncode_path_changes "")
    foreach(_dir IN LISTS _cynamoncode_runtime_dirs)
        file(TO_NATIVE_PATH "${_dir}" _native)
        list(APPEND _cynamoncode_path_changes "PATH=path_list_prepend:${_native}")
    endforeach()
    get_property(_cynamoncode_tests DIRECTORY PROPERTY TESTS)
    set_tests_properties(${_cynamoncode_tests} PROPERTIES
        ENVIRONMENT_MODIFICATION "${_cynamoncode_path_changes}")
endif()
