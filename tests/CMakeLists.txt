# Komponent Test jest potrzebny wyłącznie przy BUILD_TESTING=ON.
find_package(Qt6 REQUIRED COMPONENTS Test)

# cynamoncode_add_qt_test(<nazwa> <plik.cpp> [biblioteki...])
function(cynamoncode_add_qt_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE Qt6::Test cynamoncode_warnings ${ARGN})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

cynamoncode_add_qt_test(tst_qscintilla_link unit/tst_qscintilla_link.cpp
    QScintilla::QScintilla)
set_tests_properties(tst_qscintilla_link PROPERTIES LABELS "unit")

# Test GUI: bez serwera wyświetlania uruchamiaj z QT_QPA_PLATFORM=offscreen (scripts/test.sh robi to sam).
cynamoncode_add_qt_test(tst_mainwindow_smoke gui/tst_mainwindow_smoke.cpp
    cynamoncode_ui)
set_tests_properties(tst_mainwindow_smoke PROPERTIES LABELS "gui")
