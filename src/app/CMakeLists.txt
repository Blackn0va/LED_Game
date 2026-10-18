ldu_add_executable(LichtDuell
    SOURCES main.cpp
    DEPS ldu::game ldu::devices ldu::govee ldu::setup ldu::console ldu::cli ldu::sound ldu::score winmm
    DESCRIPTION "LichtDuell – Farbduell auf dem LED-Streifen")
