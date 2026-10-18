ldu_add_module(govee
    SOURCES protocol.cpp discovery.cpp driver.cpp
    PUBLIC_DEPS ldu::core ldu::net ldu::lights)
