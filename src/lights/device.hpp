// Welche Gerätearten LichtDuell ansprechen kann, mit ihren festen Grenzen.
//
// Ein neuer Hersteller bekommt hier einen Eintrag, ein eigenes Modul unter src/<name>/
// mit einer LightProtocol-Ableitung (lights/output.hpp) und einen Fall in
// devices/connect.cpp – Anleitung in CONTRIBUTING.md.
//
// Grenzen je Bild (Farben = Abschnitte des Streifens):
//   Govee   84  – Längenbyte des Razer-Pakets (govee/protocol.hpp); der H612C nimmt
//               gemessen höchstens 40 (2026-10-18), andere Modelle sind nicht geprüft.
//   WLED   490  – DRGB-Paket laut WLED-Doku „UDP Realtime“.
//   Art-Net 170 – ein Universum hat 512 Kanäle, je LED 3 (RGB).
//   sACN   170  – ebenso (E1.31, 512 Kanäle je Universum).
// Mehrere Universen und WLED-DNRGB für längere Streifen sind nicht eingebaut.
//
// Threads: reine Funktionen.
// Fehler: keine Würfe.

#pragma once

#include <array>
#include <optional>
#include <string_view>

namespace ldu::lights {

enum class DeviceType { govee, wled, artnet, sacn };

inline constexpr std::array<DeviceType, 4> kDeviceTypes{DeviceType::govee, DeviceType::wled, DeviceType::artnet,
                                                        DeviceType::sacn};

// Schlüssel für --gerät und einstellungen.txt („govee“, „wled“, „artnet“, „sacn“).
std::string_view device_key(DeviceType type) noexcept;
// Anzeigename („Govee“, „WLED“, „Art-Net“, „sACN (E1.31)“).
std::string_view device_name(DeviceType type) noexcept;
std::optional<DeviceType> parse_device_type(std::string_view key) noexcept;

// Höchstens so viele Farben je Bild (siehe Kopf).
int max_units(DeviceType type) noexcept;

// Art-Net und sACN brauchen ein Universum; Govee und WLED nicht.
bool uses_universe(DeviceType type) noexcept;
// Art-Net: Port-Adresse 0–32767 (15 Bit), Vorgabe 0. sACN: 1–63999 (E1.31 §6.2.7), Vorgabe 1.
int min_universe(DeviceType type) noexcept;
int max_universe(DeviceType type) noexcept;
int default_universe(DeviceType type) noexcept;

}  // namespace ldu::lights
