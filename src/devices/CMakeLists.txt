ldu_add_module(devices
    SOURCES connect.cpp
    PUBLIC_DEPS ldu::core ldu::net ldu::lights
    PRIVATE_DEPS ldu::govee ldu::wled ldu::artnet ldu::sacn)
