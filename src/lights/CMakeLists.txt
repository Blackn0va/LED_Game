ldu_add_module(lights
    SOURCES device.cpp layout.cpp frame_pacer.cpp output.cpp
    PUBLIC_DEPS ldu::core ldu::net)
