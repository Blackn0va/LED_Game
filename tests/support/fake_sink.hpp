// Senke ohne Netz für Ausgabe-Tests: schreibt jedes Datagramm und jede Wartezeit mit und
// kann Sendefehler spielen. So belegen die Treibertests, WAS in welcher Reihenfolge an das
// Gerät ginge – ob ein echtes Gerät es annimmt, belegen sie nicht.
//
// Threads: ein Test, ein Thread.
// Fehler: keine.

#pragma once

#include "net/udp.hpp"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace ldu::test {

struct SinkLog final {
    std::vector<std::string> packets;
    std::vector<double> waits;
    bool fail = false;

    long count(const std::string& packet) const {
        return static_cast<long>(std::count(packets.begin(), packets.end(), packet));
    }

    void clear() {
        packets.clear();
        waits.clear();
    }
};

class FakeSink final : public net::PacketSink {
public:
    explicit FakeSink(SinkLog& log) : log_(log) {}

    bool send(std::string_view payload, std::string& error) override {
        if (log_.fail) {
            error = "Netz weg";
            return false;
        }
        log_.packets.emplace_back(payload);
        return true;
    }

private:
    SinkLog& log_;
};

inline std::unique_ptr<net::PacketSink> fake_sink(SinkLog& log) {
    return std::make_unique<FakeSink>(log);
}

// Wartet nicht, schreibt nur mit (passt auf lights::Sleeper).
inline std::function<void(double)> recording_sleeper(SinkLog& log) {
    return [&log](double seconds) { log.waits.push_back(seconds); };
}

}  // namespace ldu::test
