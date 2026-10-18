# LichtDuell – Farbduell auf einem Govee-WLAN-LED-Streifen, gesteuert über die Tastatur.
#
# Der Computer schickt farbige Lichtpunkte vom fernen Ende los, du schießt vom nahen Ende
# zurück. Gleiche Farbe löst den vordersten Punkt auf, falsche Farbe steckt sich vorn an
# und muss selbst erst wieder weggeschossen werden.
#
# Aufbau: statische Kernbibliotheken ldu_<modul> ohne GUI, LichtDuell.exe (Konsole),
# Tests je Modul (doctest, ctest). Muster: Signierungstool/kraken („Muster CaptionBridgeCPP“),
# bewusst ohne Qt, Setup und Signatur – das Spiel wird nicht ausgeliefert.
#
# Bau ausschließlich über build.ps1. Von Hand:
#   cmake -S . -B out/rel -G "Visual Studio 17 2022" -A x64
#   cmake --build out/rel --config Release
#   ctest --test-dir out/rel -C Release -L unit

cmake_minimum_required(VERSION 3.30)

# Einzige Versionsquelle. build.ps1 und die Versionsressource lesen sie von hier.
project(LichtDuell VERSION 0.1.0 DESCRIPTION "LichtDuell" LANGUAGES CXX)

if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "LichtDuell baut nur mit MSVC für x64.")
endif()
enable_language(RC)

option(LDU_BUILD_TESTS "Tests bauen (ctest)" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# /MD wie in allen C++-Werkzeugen des Hauses (dort wegen Qt als DLL); hier ohne Qt,
# aber gleiche Laufzeit, damit vendorte Teile und Muster austauschbar bleiben.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
# Alle EXEs in einen Ordner (out\rel\bin\Release): LichtDuell.exe liegt dann an einer
# festen Stelle, die README und build.ps1 nennen können.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(LduCompile)
include(LduDeps)
include(LduTesting)

message(STATUS "LichtDuell ${PROJECT_VERSION}: Tests=${LDU_BUILD_TESTS}")

# Module in Abhängigkeitsreihenfolge (keine Zyklen).
set(LDU_MODULES core net lights govee wled artnet sacn devices game sound score console cli setup)
foreach(_m IN LISTS LDU_MODULES)
    add_subdirectory(src/${_m})
endforeach()
add_subdirectory(src/app)

if(LDU_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/support)
    foreach(_m IN LISTS LDU_MODULES)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/${_m}/CMakeLists.txt")
            add_subdirectory(tests/${_m})
        endif()
    endforeach()
    add_subdirectory(probes)
endif()
