#pragma once

#include <cstddef>
#include <cstdint>

// Ground-truth Mitsubishi "MSC" IR protocol constants, reverse-engineered from
// captures of the original remote for MS09TW / MS17TN indoor units (KP1A-style
// remote). These match the tonia/HeatpumpIR library's MitsubishiMSCHeatpumpIR
// implementation byte-for-byte; do not change these without new captures.
//
// Frame: 14 bytes, LSB-first per byte, pulse-distance encoding.
//   Bit:  mark(BIT_MARK) + space(ZERO_SPACE or ONE_SPACE)
//   Frame: mark(HDR_MARK) + space(HDR_SPACE) + 14 bytes + trailing mark(BIT_MARK)
//
// Byte layout (0-indexed):
//   0-4  Fixed prefix: 23 CB 26 01 00
//   5    Power: 0x24 = on, 0x20 = off (bit 0x04 distinguishes on/off)
//   6    Operating mode (low nibble)
//   7    Temperature: 31 - target_celsius
//   8    Fan speed (low 3 bits) OR'd with vertical vane (bits 3-5)
//   9-12 Fixed 0x00 in all captures
//   13   Checksum: sum of bytes 0-12, truncated to 8 bits

namespace esphome {
namespace mitsubishi_msc {

// Timing (microseconds), confirmed against captures
constexpr uint32_t MSC_HDR_MARK = 3060;
constexpr uint32_t MSC_HDR_SPACE = 1580;
constexpr uint32_t MSC_BIT_MARK = 350;
constexpr uint32_t MSC_ZERO_SPACE = 390;
constexpr uint32_t MSC_ONE_SPACE = 1150;
constexpr uint32_t MSC_CARRIER_HZ = 38000;
constexpr uint8_t MSC_FRAME_LEN = 14;

// Fixed frame prefix
constexpr uint8_t MSC_PREFIX[5] = {0x23, 0xCB, 0x26, 0x01, 0x00};

// Power (byte 5)
constexpr uint8_t MSC_POWER_ON = 0x24;
constexpr uint8_t MSC_POWER_OFF = 0x20;
constexpr uint8_t MSC_POWER_ON_BIT = 0x04;

// Operating mode (byte 6, low nibble)
constexpr uint8_t MSC_MODE_HEAT = 0x01;
constexpr uint8_t MSC_MODE_DRY = 0x02;
constexpr uint8_t MSC_MODE_COOL = 0x03;
constexpr uint8_t MSC_MODE_FAN = 0x07;
constexpr uint8_t MSC_MODE_AUTO = 0x08;

// Fan speed (byte 8, low 3 bits) -- confirmed against the physical unit
constexpr uint8_t MSC_FAN_AUTO = 0x00;
constexpr uint8_t MSC_FAN_LOW = 0x02;
constexpr uint8_t MSC_FAN_MEDIUM = 0x03;
constexpr uint8_t MSC_FAN_HIGH = 0x05;
constexpr uint8_t MSC_FAN_MASK = 0x07;

// Vertical vane (byte 8, bits 3-5) -- all 7 remote positions
constexpr uint8_t MSC_VANE_AUTO = 0x00;
constexpr uint8_t MSC_VANE_HIGHEST = 0x08;
constexpr uint8_t MSC_VANE_SECOND_HIGHEST = 0x10;
constexpr uint8_t MSC_VANE_MIDDLE = 0x18;
constexpr uint8_t MSC_VANE_NEXT_LOWER = 0x20;
constexpr uint8_t MSC_VANE_LOWEST = 0x28;
constexpr uint8_t MSC_VANE_SWING = 0x38;
constexpr uint8_t MSC_VANE_MASK = 0x38;

struct VaneOption {
  const char *name;
  uint8_t code;
};

// Order shown in the Home Assistant select entity
constexpr VaneOption MSC_VANE_OPTIONS[] = {
    {"Auto", MSC_VANE_AUTO},
    {"Highest", MSC_VANE_HIGHEST},
    {"Second Highest", MSC_VANE_SECOND_HIGHEST},
    {"Middle", MSC_VANE_MIDDLE},
    {"Next Lower", MSC_VANE_NEXT_LOWER},
    {"Lowest", MSC_VANE_LOWEST},
    {"Swing", MSC_VANE_SWING},
};
constexpr size_t MSC_VANE_OPTIONS_COUNT = sizeof(MSC_VANE_OPTIONS) / sizeof(MSC_VANE_OPTIONS[0]);

// Valid target range: byte 7 is 31 - T, so T must be 16..31 C.
constexpr uint8_t MSC_TEMP_MIN_C = 16;
constexpr uint8_t MSC_TEMP_MAX_C = 31;

inline uint8_t msc_checksum(const uint8_t *frame) {
  uint8_t sum = 0;
  for (uint8_t i = 0; i < MSC_FRAME_LEN - 1; i++)
    sum += frame[i];
  return sum;
}

// `power` is the raw byte 5 value; `temperature_c` is clamped to 16..31.
inline void msc_build_frame(uint8_t *frame, uint8_t power, uint8_t mode, uint8_t temperature_c, uint8_t fan,
                            uint8_t vane) {
  if (temperature_c < MSC_TEMP_MIN_C)
    temperature_c = MSC_TEMP_MIN_C;
  if (temperature_c > MSC_TEMP_MAX_C)
    temperature_c = MSC_TEMP_MAX_C;
  for (uint8_t i = 0; i < MSC_FRAME_LEN; i++)
    frame[i] = 0;
  for (uint8_t i = 0; i < sizeof(MSC_PREFIX); i++)
    frame[i] = MSC_PREFIX[i];
  frame[5] = power;
  frame[6] = mode;
  frame[7] = MSC_TEMP_MAX_C - temperature_c;
  frame[8] = fan | vane;
  frame[MSC_FRAME_LEN - 1] = msc_checksum(frame);
}

inline bool msc_frame_has_prefix(const uint8_t *frame) {
  for (uint8_t i = 0; i < sizeof(MSC_PREFIX); i++) {
    if (frame[i] != MSC_PREFIX[i])
      return false;
  }
  return true;
}

inline bool msc_frame_checksum_ok(const uint8_t *frame) { return msc_checksum(frame) == frame[MSC_FRAME_LEN - 1]; }

}  // namespace mitsubishi_msc
}  // namespace esphome
