ldu_add_module(net
    SOURCES udp.cpp http.cpp mdns.cpp
    PUBLIC_DEPS ldu::core
    PRIVATE_DEPS ws2_32 iphlpapi)
