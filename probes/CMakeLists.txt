# Messproben (siehe probes/README.md). Label "integration": laufen NICHT in
# build.ps1 -Stage test, sondern nur auf Wunsch: ctest --test-dir out/rel -C Release -L integration
add_executable(ldu_probe_keys key_probe.cpp)
target_link_libraries(ldu_probe_keys PRIVATE ldu::compile_defaults)
ldu_strict_warnings(ldu_probe_keys)
set_target_properties(ldu_probe_keys PROPERTIES FOLDER "probes")
add_dependencies(ldu_probe_keys LichtDuell)
add_test(NAME ldu_probe_keys COMMAND ldu_probe_keys $<TARGET_FILE:LichtDuell>)
set_tests_properties(ldu_probe_keys PROPERTIES LABELS integration TIMEOUT 300)

# Messung am echten Streifen, jemand muss hinschauen – deshalb kein ctest.
add_executable(ldu_probe_govee govee_probe.cpp)
target_link_libraries(ldu_probe_govee PRIVATE ldu::compile_defaults ldu::govee ldu::net ws2_32)
ldu_strict_warnings(ldu_probe_govee)
set_target_properties(ldu_probe_govee PROPERTIES FOLDER "probes")

# Lauscht auf 127.0.0.1:4003 – was sendet das Spiel beim Beenden? Von Hand, siehe README.
add_executable(ldu_probe_listen listen_probe.cpp)
target_link_libraries(ldu_probe_listen PRIVATE ldu::compile_defaults ldu::govee ws2_32)
ldu_strict_warnings(ldu_probe_listen)
set_target_properties(ldu_probe_listen PROPERTIES FOLDER "probes")

# Wie viel Zeit bleibt beim Schließen des Konsolenfensters? Von Hand, siehe README.
add_executable(ldu_probe_ctrl ctrl_probe.cpp)
target_link_libraries(ldu_probe_ctrl PRIVATE ldu::compile_defaults)
ldu_strict_warnings(ldu_probe_ctrl)
set_target_properties(ldu_probe_ctrl PROPERTIES FOLDER "probes")
