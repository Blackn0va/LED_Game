#include "lights/device.hpp"

namespace ldu::lights {

std::string_view device_key(DeviceType type) noexcept {
    switch (type) {
    case DeviceType::govee:
        return "govee";
    case DeviceType::wled:
        return "wled";
    case DeviceType::artnet:
        return "artnet";
    case DeviceType::sacn:
        return "sacn";
    }
    return "govee";
}

std::string_view device_name(DeviceType type) noexcept {
    switch (type) {
    case DeviceType::govee:
        return "Govee";
    case DeviceType::wled:
        return "WLED";
    case DeviceType::artnet:
        return "Art-Net";
    case DeviceType::sacn:
        return "sACN (E1.31)";
    }
    return "Govee";
}

std::optional<DeviceType> parse_device_type(std::string_view key) noexcept {
    for (const DeviceType t : kDeviceTypes) {
        if (key == device_key(t)) {
            return t;
        }
    }
    // Verbreitete Schreibweisen zulassen, damit „--gerät e131“ oder „art-net“ nicht scheitert.
    if (key == "e131" || key == "e1.31") {
        return DeviceType::sacn;
    }
    if (key == "art-net") {
        return DeviceType::artnet;
    }
    return std::nullopt;
}

int max_units(DeviceType type) noexcept {
    switch (type) {
    case DeviceType::govee:
        return 84;
    case DeviceType::wled:
    case DeviceType::artnet:
    case DeviceType::sacn:
        return kMaxLongStripUnits;
    }
    return 84;
}

int universes_needed(int units) noexcept {
    return units <= kLedsPerUniverse ? 1 : (units + kLedsPerUniverse - 1) / kLedsPerUniverse;
}

bool uses_universe(DeviceType type) noexcept {
    return type == DeviceType::artnet || type == DeviceType::sacn;
}

int min_universe(DeviceType type) noexcept {
    return type == DeviceType::sacn ? 1 : 0;
}

int max_universe(DeviceType type) noexcept {
    return type == DeviceType::sacn ? 63999 : 32767;
}

int default_universe(DeviceType type) noexcept {
    return type == DeviceType::sacn ? 1 : 0;
}

}  // namespace ldu::lights
