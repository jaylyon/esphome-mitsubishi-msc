// Host-side checks for the frame helpers. Build and run:
//   g++ -std=c++17 -Wall -Wextra -I components/mitsubishi_msc tests/test_protocol.cpp -o /tmp/test_protocol && /tmp/test_protocol
#include <cstdio>
#include <cstring>

#include "mitsubishi_msc_protocol.h"

using namespace esphome::mitsubishi_msc;

static int failures = 0;

#define CHECK(cond)                                                \
  do {                                                             \
    if (!(cond)) {                                                 \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);  \
      failures++;                                                  \
    }                                                              \
  } while (0)

int main() {
  // Captured from the original remote: on, Cool, 24 C, fan Low, vane Highest.
  const uint8_t capture[MSC_FRAME_LEN] = {0x23, 0xCB, 0x26, 0x01, 0x00, 0xA4, 0x03,
                                          0x07, 0x0A, 0x00, 0x00, 0x00, 0x00, 0xCD};
  CHECK(msc_frame_has_prefix(capture));
  CHECK(msc_frame_checksum_ok(capture));

  uint8_t frame[MSC_FRAME_LEN];
  msc_build_frame(frame, 0xA4, MSC_MODE_COOL, 24, MSC_FAN_LOW, MSC_VANE_HIGHEST);
  CHECK(std::memcmp(frame, capture, MSC_FRAME_LEN) == 0);

  // What this component transmits for the same state (no 0x80 flag in byte 5).
  msc_build_frame(frame, MSC_POWER_ON, MSC_MODE_COOL, 24, MSC_FAN_LOW, MSC_VANE_HIGHEST);
  CHECK(frame[5] == 0x24);
  CHECK(msc_frame_checksum_ok(frame));
  CHECK((frame[5] & MSC_POWER_ON_BIT) != 0);
  msc_build_frame(frame, MSC_POWER_OFF, MSC_MODE_AUTO, 24, MSC_FAN_AUTO, MSC_VANE_AUTO);
  CHECK((frame[5] & MSC_POWER_ON_BIT) == 0);

  // A corrupted frame must be rejected.
  uint8_t bad[MSC_FRAME_LEN];
  std::memcpy(bad, capture, MSC_FRAME_LEN);
  bad[7] ^= 0x01;
  CHECK(!msc_frame_checksum_ok(bad));
  std::memcpy(bad, capture, MSC_FRAME_LEN);
  bad[2] = 0x00;
  CHECK(!msc_frame_has_prefix(bad));

  // Temperature encoding: 24 C -> 0x07, 25 C -> 0x06, clamped to 16..31 C.
  msc_build_frame(frame, MSC_POWER_ON, MSC_MODE_COOL, 24, MSC_FAN_AUTO, MSC_VANE_AUTO);
  CHECK(frame[7] == 0x07);
  msc_build_frame(frame, MSC_POWER_ON, MSC_MODE_COOL, 25, MSC_FAN_AUTO, MSC_VANE_AUTO);
  CHECK(frame[7] == 0x06);
  msc_build_frame(frame, MSC_POWER_ON, MSC_MODE_COOL, 40, MSC_FAN_AUTO, MSC_VANE_AUTO);
  CHECK(frame[7] == 0);
  msc_build_frame(frame, MSC_POWER_ON, MSC_MODE_COOL, 5, MSC_FAN_AUTO, MSC_VANE_AUTO);
  CHECK(frame[7] == 15);

  // Fan values from the physical unit.
  CHECK(MSC_FAN_AUTO == 0x00 && MSC_FAN_LOW == 0x02 && MSC_FAN_MEDIUM == 0x03 && MSC_FAN_HIGH == 0x05);

  // Fan and vane share byte 8 and must round-trip without overlapping.
  const uint8_t fans[] = {MSC_FAN_AUTO, MSC_FAN_LOW, MSC_FAN_MEDIUM, MSC_FAN_HIGH};
  CHECK(MSC_VANE_OPTIONS_COUNT == 7);
  for (size_t v = 0; v < MSC_VANE_OPTIONS_COUNT; v++) {
    for (uint8_t fan : fans) {
      uint8_t vane = MSC_VANE_OPTIONS[v].code;
      CHECK((fan & vane) == 0);
      msc_build_frame(frame, MSC_POWER_ON, MSC_MODE_COOL, 24, fan, vane);
      CHECK((frame[8] & MSC_FAN_MASK) == fan);
      CHECK((frame[8] & MSC_VANE_MASK) == vane);
    }
  }

  if (failures == 0)
    std::printf("all protocol checks passed\n");
  return failures == 0 ? 0 : 1;
}
