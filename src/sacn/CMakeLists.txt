ldu_add_module(sacn
    SOURCES protocol.cpp driver.cpp
    PUBLIC_DEPS ldu::core ldu::net ldu::lights)
