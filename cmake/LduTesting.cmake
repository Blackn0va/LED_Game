# Testrahmen: doctest 2.4.11 (vendort), ein Test-Programm je Modul, ctest-Labels.
#
#   ldu_add_test(<name> SOURCES … [LIBS …] [LABELS unit|net …])
#
# Label "unit": ohne Netz, ohne Streifen, schnell – läuft in build.ps1 -Stage test.
# Label "net": bräuchte einen echten Govee-Streifen im LAN – gibt es derzeit nicht,
# die Prüfung am Gerät steht in Gedaechtnis.md unter „Offen“.
#
# Jedes Test-Programm bekommt LDU_TEST_BINARY_DIR (eigener Ausgabeordner).

include_guard(GLOBAL)

function(ldu_add_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS;LABELS" ${ARGN})
    add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE ldu::compile_defaults ldu::test_main ${ARG_LIBS})
    ldu_strict_warnings(${name})
    target_compile_definitions(${name} PRIVATE LDU_TEST_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    set_target_properties(${name} PROPERTIES FOLDER "tests")
    set(_labels "${ARG_LABELS}")
    if(NOT _labels)
        set(_labels unit)
    endif()
    add_test(NAME ${name} COMMAND ${name} --no-intro=true)
    set_tests_properties(${name} PROPERTIES LABELS "${_labels}" TIMEOUT 300)
endfunction()
