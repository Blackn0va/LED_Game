ldu_add_module(sacn
    SOURCES protocol.cpp driver.cpp router.cpp
    PUBLIC_DEPS ldu::core ldu::net ldu::lights)
