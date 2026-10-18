add_library(ldu_test_main STATIC test_main.cpp)
add_library(ldu::test_main ALIAS ldu_test_main)
target_link_libraries(ldu_test_main PUBLIC ldu::compile_defaults ldu::doctest)
# Gemeinsame Test-Helfer als "support/…" (fake_sink.hpp: Senke ohne Netz).
target_include_directories(ldu_test_main PUBLIC "${LDU_SOURCE_ROOT}/tests")
ldu_strict_warnings(ldu_test_main)
set_target_properties(ldu_test_main PROPERTIES FOLDER "tests")
