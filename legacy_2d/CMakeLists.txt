cmake_minimum_required(VERSION 3.20)
project(LegendeVonAldoria CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# raylib wird beim ersten Konfigurieren automatisch von GitHub geholt und statisch gelinkt
include(FetchContent)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

add_executable(rpg src/main.cpp)
target_link_libraries(rpg PRIVATE raylib)

if(MSVC)
    # UTF-8 im Quelltext (Umlaute) und kein Konsolenfenster
    target_compile_options(rpg PRIVATE /utf-8 /W3)
    target_link_options(rpg PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
endif()
