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
cynamoncode_add_qt_test(tst_disk_changes unit unit/tst_disk_changes.cpp cynamoncode_core)

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
