cmake_minimum_required(VERSION 3.20)
project(Aldoria CXX)

# Statische Laufzeitbibliothek: die .exe läuft ohne installierte Visual-C++-Pakete
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# Unit-Tests sind optional (im Linux-Quellpaket fehlt der Ordner tests, dann wird auch doctest nicht geladen)
option(ALDORIA_BUILD_TESTS "Unit-Tests bauen" ON)
if(NOT EXISTS ${CMAKE_SOURCE_DIR}/tests)
    set(ALDORIA_BUILD_TESTS OFF)
endif()

# raylib (statisch gelinkt)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
)
# Zwei einzelne Header aus den offiziellen Repos: JSON und Test-Framework
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
    DOWNLOAD_NO_EXTRACT TRUE
)
FetchContent_Declare(
    doctest
    URL https://raw.githubusercontent.com/doctest/doctest/v2.4.11/doctest/doctest.h
    DOWNLOAD_NO_EXTRACT TRUE
)
FetchContent_MakeAvailable(raylib nlohmann_json)
if(ALDORIA_BUILD_TESTS)
    FetchContent_MakeAvailable(doctest)
endif()

# Pfad zum data-Ordner im Quellbaum (Dev-Build), damit JSON-Änderungen ohne Kopieren wirken
configure_file(src/core/build_config.h.in ${CMAKE_BINARY_DIR}/generated/core/build_config.h @ONLY)

# Alles außer main.cpp kommt in eine Bibliothek, die Spiel und Tests gemeinsam nutzen
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS src/*.cpp)
list(FILTER CORE_SOURCES EXCLUDE REGEX "src/main\\.cpp$")

add_library(aldoria_core STATIC ${CORE_SOURCES})
target_include_directories(aldoria_core PUBLIC src ${CMAKE_BINARY_DIR}/generated)
target_include_directories(aldoria_core SYSTEM PUBLIC ${nlohmann_json_SOURCE_DIR})
target_link_libraries(aldoria_core PUBLIC raylib)
target_compile_definitions(aldoria_core PUBLIC _CRT_SECURE_NO_WARNINGS)

add_executable(aldoria src/main.cpp)
target_link_libraries(aldoria PRIVATE aldoria_core)

if(ALDORIA_BUILD_TESTS)
    file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
    add_executable(aldoria_tests ${TEST_SOURCES})
    target_include_directories(aldoria_tests SYSTEM PRIVATE ${doctest_SOURCE_DIR})
    target_link_libraries(aldoria_tests PRIVATE aldoria_core)

    enable_testing()
    add_test(NAME aldoria_tests COMMAND aldoria_tests)
endif()

# Installation (Linux-Pakete: PKGBUILD und .deb installieren mit --prefix /opt/aldoria,
# ein Wrapper-Skript unter /usr/bin/aldoria startet die Binärdatei mit dem richtigen data-Ordner)
install(TARGETS aldoria RUNTIME DESTINATION .)
install(DIRECTORY data/ DESTINATION data)
install(FILES README.md LICENSE NOTICE DESTINATION .)

if(MSVC)
    set(ALDORIA_TARGETS aldoria_core aldoria)
    if(ALDORIA_BUILD_TESTS)
        list(APPEND ALDORIA_TARGETS aldoria_tests)
    endif()
    foreach(target ${ALDORIA_TARGETS})
        target_compile_options(${target} PRIVATE /utf-8 /W4 /wd4100 /external:W0)
    endforeach()
    # Spiel ohne Konsolenfenster; Logs stehen in aldoria.log neben der .exe
    target_link_options(aldoria PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
endif()
