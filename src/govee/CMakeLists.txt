ldu_add_module(govee
    SOURCES protocol.cpp frame_pacer.cpp udp.cpp strip.cpp
    PUBLIC_DEPS ldu::core
    PRIVATE_DEPS ws2_32 iphlpapi)
