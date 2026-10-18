// Welche Gerätearten LichtDuell ansprechen kann, mit ihren festen Grenzen.
//
// Ein neuer Hersteller bekommt hier einen Eintrag, ein eigenes Modul unter src/<name>/
// mit einer LightProtocol-Ableitung (lights/output.hpp) und einen Fall in
// devices/connect.cpp – Anleitung in CONTRIBUTING.md.
//
// Grenzen je Bild (Farben = Abschnitte des Streifens):
//   Govee    84  – Längenbyte des Razer-Pakets (govee/protocol.hpp); der H612C nimmt
//                gemessen höchstens 40 (2026-10-18), andere Modelle sind nicht geprüft.
//   WLED   2040  – bis 490 ein DRGB-Paket, darüber DNRGB-Pakete zu je 489 LEDs mit
//                Startindex (WLED-Doku „UDP Realtime“, nachgelesen 2026-10-18).
//   Art-Net 2040 – je 170 LEDs ein Universum (512 Kanäle, je LED 3), aufeinanderfolgend ab
//                --universum.
//   sACN   2040  – ebenso (E1.31, 512 Kanäle je Universum).
// 2040 ist eine gewählte Grenze, keine des Protokolls: 12 Universen bzw. 5 DNRGB-Pakete je
// Bild, bei 30 fps also höchstens 360 Pakete je Sekunde (gerechnet, nicht gemessen). Vorher
// war bei 490 bzw. 170 Schluss; ein 300er-Pixelstreifen an Art-Net bekam nur 170 Farben.
// Wer die Grenze hebt, prüft vorher die Last im WLAN.
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

inline constexpr int kMaxLongStripUnits = 2040;
inline constexpr int kLedsPerUniverse = 170;

// Höchstens so viele Farben je Bild (siehe Kopf).
int max_units(DeviceType type) noexcept;

// So viele Universen belegt ein Bild mit units Farben (mindestens eins).
int universes_needed(int units) noexcept;

// Art-Net und sACN brauchen ein Universum; Govee und WLED nicht.
bool uses_universe(DeviceType type) noexcept;
// Art-Net: Port-Adresse 0–32767 (15 Bit), Vorgabe 0. sACN: 1–63999 (E1.31 §6.2.7), Vorgabe 1.
int min_universe(DeviceType type) noexcept;
int max_universe(DeviceType type) noexcept;
int default_universe(DeviceType type) noexcept;

}  // namespace ldu::lights
