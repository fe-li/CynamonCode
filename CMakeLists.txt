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

# Kod aplikacji jako biblioteka statyczna, żeby testy mogły go linkować.
add_library(cynamoncode_ui STATIC
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h)
target_include_directories(cynamoncode_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(cynamoncode_ui
    PUBLIC Qt6::Widgets
    PRIVATE cynamoncode_warnings)

add_executable(cynamoncode src/main.cpp)
target_compile_definitions(cynamoncode PRIVATE CYNAMONCODE_VERSION="${PROJECT_VERSION}")
target_link_libraries(cynamoncode PRIVATE cynamoncode_ui cynamoncode_warnings)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
