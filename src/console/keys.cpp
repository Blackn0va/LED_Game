#include "console/keys.hpp"

#include <windows.h>

namespace ldu::console {

std::optional<Color> color_for_key(std::uint16_t vk) {
    switch (vk) {
    case '1':
    case VK_NUMPAD1:
    case 'R':
        return Color::red;
    case '2':
    case VK_NUMPAD2:
    case 'G':
        return Color::green;
    case '3':
    case VK_NUMPAD3:
    case 'B':
        return Color::blue;
    case '4':
    case VK_NUMPAD4:
    case 'Y':
        return Color::yellow;
    case '5':
    case VK_NUMPAD5:
    case 'L':
        return Color::purple;
    default:
        return std::nullopt;
    }
}

Action action_for_key(std::uint16_t vk) {
    switch (vk) {
    case VK_ESCAPE:
        return Action::quit;
    case 'P':
        return Action::pause;
    case VK_RETURN:
        return Action::restart;
    case VK_SPACE:
        return Action::joker;
    case VK_LEFT:
        return Action::left;
    case VK_RIGHT:
        return Action::right;
    case VK_UP:
        return Action::up;
    case VK_DOWN:
        return Action::down;
    case 'S':
        return Action::save;
    case 'A':
        return Action::toggle_auto;
    default:
        return Action::none;
    }
}

}  // namespace ldu::console
