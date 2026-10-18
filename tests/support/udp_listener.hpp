// UDP-Empfänger auf 127.0.0.1 für Loopback-Tests: Was kommt tatsächlich als Datagramm an?
//
// Belegt den Weg Treiber → LightOutput → UdpSender → Socket ohne fremdes Gerät. Loopback
// verlässt den Rechner nicht und geht an der Windows-Firewall vorbei, deshalb Label „unit“.
//
// Threads: ein Test, ein Thread. Eine ldu::net::NetworkSession muss bestehen.
// Fehler: Konstruktor wirft std::runtime_error, wenn der Port belegt ist.

#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>

#include <chrono>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

namespace ldu::test {

class UdpListener final {
public:
    // port 0 = vom System gewählt (siehe port()).
    explicit UdpListener(std::uint16_t port) {
        socket_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socket_ == INVALID_SOCKET) {
            throw std::runtime_error("Test-Socket lässt sich nicht öffnen");
        }
        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_port = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &local.sin_addr);
        if (bind(socket_, reinterpret_cast<const sockaddr*>(&local), static_cast<int>(sizeof(local))) == SOCKET_ERROR) {
            closesocket(socket_);
            throw std::runtime_error("127.0.0.1:" + std::to_string(port) + " ist belegt");
        }
        int len = static_cast<int>(sizeof(local));
        getsockname(socket_, reinterpret_cast<sockaddr*>(&local), &len);
        port_ = ntohs(local.sin_port);
    }

    ~UdpListener() {
        closesocket(socket_);
    }

    UdpListener(const UdpListener&) = delete;
    UdpListener& operator=(const UdpListener&) = delete;

    std::uint16_t port() const noexcept {
        return port_;
    }

    // Ein Datagramm an 127.0.0.1:port – für Tests der Empfangsseite.
    bool send_to(std::uint16_t port, const std::string& payload) {
        sockaddr_in to{};
        to.sin_family = AF_INET;
        to.sin_port = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &to.sin_addr);
        return sendto(socket_, payload.data(), static_cast<int>(payload.size()), 0, reinterpret_cast<const sockaddr*>(&to),
                      static_cast<int>(sizeof(to))) != SOCKET_ERROR;
    }

    // Nächstes Datagramm oder nullopt nach timeout.
    std::optional<std::string> receive(std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) {
        WSAPOLLFD pfd{};
        pfd.fd = socket_;
        pfd.events = POLLRDNORM;
        if (WSAPoll(&pfd, 1, static_cast<INT>(timeout.count())) <= 0) {
            return std::nullopt;
        }
        char buf[4096];
        const int n = recv(socket_, buf, static_cast<int>(sizeof(buf)), 0);
        if (n == SOCKET_ERROR) {
            return std::nullopt;
        }
        return std::string(buf, static_cast<std::size_t>(n));
    }

private:
    SOCKET socket_ = INVALID_SOCKET;
    std::uint16_t port_ = 0;
};

}  // namespace ldu::test
