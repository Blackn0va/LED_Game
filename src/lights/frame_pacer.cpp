#include "lights/frame_pacer.hpp"

#include <algorithm>

namespace ldu::lights {

FramePacer::FramePacer(double max_fps, double keepalive_s)
    : min_interval_s_(1.0 / std::max(max_fps, 1.0)), keepalive_s_(keepalive_s) {}

bool FramePacer::should_send(double now_s, bool changed) const {
    if (!sent_any_) {
        return true;
    }
    const double since = now_s - last_sent_s_;
    if (since < min_interval_s_) {
        return false;
    }
    return changed || since >= keepalive_s_;
}

void FramePacer::mark_sent(double now_s) {
    last_sent_s_ = now_s;
    sent_any_ = true;
}

}  // namespace ldu::lights
