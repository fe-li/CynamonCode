# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Kamil Feliszewski

# FindQScintillaQt6
# -----------------
#
# Wyszukuje QScintilla 2 zbudowaną dla Qt6 po nagłówku i bibliotece.
# Nie zakłada istnienia QScintillaConfig.cmake ani pobierania czegokolwiek z sieci.
#
# Wymaga wcześniejszego find_package(Qt6 ... COMPONENTS Widgets).
#
# Zmienne wejściowe (opcjonalne):
#   QScintillaQt6_ROOT         katalog instalacji QScintilla (CMP0074)
#
# Zmienne wynikowe:
#   QScintillaQt6_FOUND
#   QScintillaQt6_VERSION      z QSCINTILLA_VERSION_STR w Qsci/qsciglobal.h
#   QScintillaQt6_INCLUDE_DIR  katalog zawierający podkatalog Qsci/
#   QScintillaQt6_LIBRARY      ścieżka biblioteki qscintilla2_qt6
#
# Target importowany:
#   QScintilla::QScintilla

if(NOT TARGET Qt6::Widgets)
    message(FATAL_ERROR
        "FindQScintillaQt6: najpierw wywołaj find_package(Qt6 REQUIRED COMPONENTS Widgets).")
endif()

# Podpowiedzi ścieżek na podstawie znalezionego Qt6 (np. /usr/include/x86_64-linux-gnu/qt6).
set(_qsci_include_hints "")
set(_qsci_library_hints "")

get_target_property(_qsci_qt_includes Qt6::Core INTERFACE_INCLUDE_DIRECTORIES)
if(_qsci_qt_includes)
    foreach(_dir IN LISTS _qsci_qt_includes)
        if(NOT _dir MATCHES "\\$<")
            list(APPEND _qsci_include_hints "${_dir}")
        endif()
    endforeach()
endif()

get_target_property(_qsci_qt_configs Qt6::Core IMPORTED_CONFIGURATIONS)
if(_qsci_qt_configs)
    foreach(_cfg IN LISTS _qsci_qt_configs)
        get_target_property(_qsci_qt_location Qt6::Core IMPORTED_LOCATION_${_cfg})
        if(_qsci_qt_location)
            get_filename_component(_qsci_qt_libdir "${_qsci_qt_location}" DIRECTORY)
            list(APPEND _qsci_library_hints "${_qsci_qt_libdir}")
        endif()
    endforeach()
endif()

if(DEFINED QT6_INSTALL_PREFIX)
    if(DEFINED QT6_INSTALL_HEADERS)
        list(APPEND _qsci_include_hints "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_HEADERS}")
    endif()
    if(DEFINED QT6_INSTALL_LIBS)
        list(APPEND _qsci_library_hints "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_LIBS}")
    endif()
endif()

find_path(QScintillaQt6_INCLUDE_DIR
    NAMES Qsci/qsciscintilla.h
    HINTS ${_qsci_include_hints}
    PATH_SUFFIXES qt6
    DOC "Katalog zawierający Qsci/qsciscintilla.h (QScintilla dla Qt6)")

# Tylko nazwy wariantu Qt6 — biblioteka Qt5 (qscintilla2_qt5) nie może zostać wybrana.
find_library(QScintillaQt6_LIBRARY
    NAMES qscintilla2_qt6
    HINTS ${_qsci_library_hints}
    DOC "Biblioteka QScintilla dla Qt6")

# Ochrona przed nagłówkami Qt5 (np. /usr/include/x86_64-linux-gnu/qt5/Qsci).
if(QScintillaQt6_INCLUDE_DIR MATCHES "(^|/)[Qq][Tt]5(/|$)")
    message(FATAL_ERROR
        "FindQScintillaQt6: znaleziono nagłówki QScintilla dla Qt5 (${QScintillaQt6_INCLUDE_DIR}). "
        "Zainstaluj libqscintilla2-qt6-dev lub wskaż QScintillaQt6_ROOT. Qt5 nie jest obsługiwane.")
endif()

if(QScintillaQt6_INCLUDE_DIR AND EXISTS "${QScintillaQt6_INCLUDE_DIR}/Qsci/qsciglobal.h")
    file(STRINGS "${QScintillaQt6_INCLUDE_DIR}/Qsci/qsciglobal.h" _qsci_version_line
        REGEX "^#define[ \t]+QSCINTILLA_VERSION_STR[ \t]+\"[^\"]+\"")
    if(_qsci_version_line)
        string(REGEX REPLACE ".*\"([^\"]+)\".*" "\\1" QScintillaQt6_VERSION "${_qsci_version_line}")
    endif()
endif()

include(FindPackageHandleStandardArgs)
# Uwaga: w REASON_FAILURE_MESSAGE nie używaj średników — FPHSA rozbiłby napis
# na elementy listy i zgłosił „Unknown keywords”.
find_package_handle_standard_args(QScintillaQt6
    REQUIRED_VARS QScintillaQt6_LIBRARY QScintillaQt6_INCLUDE_DIR
    VERSION_VAR QScintillaQt6_VERSION
    REASON_FAILURE_MESSAGE
        "Nie znaleziono QScintilla dla Qt6 (nagłówek Qsci/qsciscintilla.h i biblioteka qscintilla2_qt6). \
Na Linux Mint / Ubuntu: sudo apt install libqscintilla2-qt6-dev, \
zawartość pakietu: dpkg -L libqscintilla2-qt6-dev. \
Niestandardową instalację wskaż przez -DQScintillaQt6_ROOT=<katalog>.")

if(QScintillaQt6_FOUND AND NOT TARGET QScintilla::QScintilla)
    add_library(QScintilla::QScintilla UNKNOWN IMPORTED)
    set_target_properties(QScintilla::QScintilla PROPERTIES
        IMPORTED_LOCATION "${QScintillaQt6_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${QScintillaQt6_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES Qt6::Widgets)
endif()

mark_as_advanced(QScintillaQt6_INCLUDE_DIR QScintillaQt6_LIBRARY)

unset(_qsci_include_hints)
unset(_qsci_library_hints)
unset(_qsci_qt_includes)
unset(_qsci_qt_configs)
unset(_qsci_qt_location)
unset(_qsci_qt_libdir)
unset(_qsci_version_line)
