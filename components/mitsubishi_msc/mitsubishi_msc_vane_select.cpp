#include "mitsubishi_msc_vane_select.h"
#include "mitsubishi_msc_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mitsubishi_msc {

static const char *const TAG = "mitsubishi_msc.select";

void MitsubishiMSCVaneSelect::control(const std::string &value) {
  for (size_t i = 0; i < MSC_VANE_OPTIONS_COUNT; i++) {
    if (value == MSC_VANE_OPTIONS[i].name) {
      this->publish_state(value);
      if (this->climate_ != nullptr)
        this->climate_->set_vane_position(MSC_VANE_OPTIONS[i].code);
      return;
    }
  }
  ESP_LOGW(TAG, "Unknown vane option '%s'", value.c_str());
}

void MitsubishiMSCVaneSelect::publish_vane_state(uint8_t vane_code) {
  for (size_t i = 0; i < MSC_VANE_OPTIONS_COUNT; i++) {
    if (MSC_VANE_OPTIONS[i].code == vane_code) {
      this->publish_state(MSC_VANE_OPTIONS[i].name);
      return;
    }
  }
  ESP_LOGW(TAG, "Received unknown vane code 0x%02X", vane_code);
}

}  // namespace mitsubishi_msc
}  // namespace esphome
