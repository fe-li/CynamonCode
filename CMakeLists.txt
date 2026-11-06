cmake_minimum_required(VERSION 3.21)

project(CynamonCode
    VERSION 0.0.1
    DESCRIPTION "Lekki edytor tekstu i kodu dla Linuksa"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Typ budowania" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Udostępnia opcję BUILD_TESTING (domyślnie ON) i włącza CTest.
include(CTest)

# Wyłącznie Qt6. Brak Qt6 to błąd konfiguracji — nie ma fallbacku do Qt5.
find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets)

# QScintilla w wariancie Qt6, wykrywana po nagłówku i bibliotece
# (bez zakładania istnienia QScintillaConfig.cmake). Tworzy QScintilla::QScintilla.
find_package(QScintillaQt6 REQUIRED)

# Wspólne ostrzeżenia kompilatora dla kodu projektu.
add_library(cynamoncode_warnings INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(cynamoncode_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

# Logika bez GUI: model dokumentu, pliki, kodowanie, przypadki użycia (tylko Qt6::Core).
add_library(cynamoncode_core STATIC
    src/domain/DocumentState.h
    src/domain/LanguageId.h
    src/domain/LineEnding.h
    src/domain/SearchOptions.h
    src/services/EncodingService.cpp
    src/services/EncodingService.h
    src/services/FileService.cpp
    src/services/FileService.h
    src/services/FileWatchService.cpp
    src/services/FileWatchService.h
    src/services/Indentation.cpp
    src/services/Indentation.h
    src/services/DocumentManager.cpp
    src/services/DocumentManager.h
    src/services/LanguageRegistry.cpp
    src/services/LanguageRegistry.h
    src/services/SearchService.cpp
    src/services/SearchService.h
    src/services/SettingsService.cpp
    src/services/SettingsService.h
    src/services/TextPositions.cpp
    src/services/TextPositions.h
    src/controllers/DocumentController.cpp
    src/controllers/DocumentController.h)
target_include_directories(cynamoncode_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(cynamoncode_core
    PUBLIC Qt6::Core
    PRIVATE cynamoncode_warnings)

# Interfejs: okno, karty, edytor QScintilla, dialogi.
add_library(cynamoncode_ui STATIC
    src/editor/EditorWidget.cpp
    src/editor/EditorWidget.h
    src/editor/SyntaxHighlighter.cpp
    src/editor/SyntaxHighlighter.h
    src/editor/ThemePalette.cpp
    src/editor/ThemePalette.h
    src/ui/FindReplaceDialog.cpp
    src/ui/FindReplaceDialog.h
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/UserPrompts.cpp
    src/ui/UserPrompts.h)
target_link_libraries(cynamoncode_ui
    PUBLIC cynamoncode_core Qt6::Widgets QScintilla::QScintilla
    PRIVATE cynamoncode_warnings)

add_executable(cynamoncode src/main.cpp)
target_compile_definitions(cynamoncode PRIVATE CYNAMONCODE_VERSION="${PROJECT_VERSION}")
target_link_libraries(cynamoncode PRIVATE cynamoncode_ui cynamoncode_warnings)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
