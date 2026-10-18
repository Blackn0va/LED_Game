ldu_add_module(artnet
    SOURCES protocol.cpp driver.cpp discovery.cpp
    PUBLIC_DEPS ldu::core ldu::net ldu::lights)
