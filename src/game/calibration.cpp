#include "game/calibration.hpp"

#include <algorithm>
#include <cmath>

namespace ldu {

std::vector<Rgb> segment_calibration_frame(int segments, int skip, int zone, double t_s) {
    const int n = std::max(segments, 1);
    const int k = std::clamp(skip, 0, n - 1);
    const int z = std::clamp(zone, 0, n - k - 1);
    const int field_end = n - z;  // erster Abschnitt des Effektbereichs
    std::vector<Rgb> frame(static_cast<std::size_t>(n), kBlack);
    for (int u = k; u < n; ++u) {
        Rgb c;
        if (u >= field_end) {
            c = scale(kZoneMarker, (u - field_end) % 2 == 0 ? kCalibrationDim : kHiddenDim);
        } else if ((u - k) % 10 == 0) {
            c = kWhite;
        } else {
            c = scale(to_rgb((u - k) % 2 == 1 ? Color::green : Color::blue), kCalibrationDim);
        }
        frame[static_cast<std::size_t>(u)] = c;
    }
    if (field_end - 1 > k) {
        frame[static_cast<std::size_t>(field_end - 1)] = to_rgb(Color::purple);
    }
    frame[static_cast<std::size_t>(k)] = scale(kWhite, 0.55 + 0.45 * std::sin(t_s * 5.0));
    return frame;
}

}  // namespace ldu
