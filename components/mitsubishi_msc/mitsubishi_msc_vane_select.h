#pragma once

#include "esphome/components/select/select.h"
#include "mitsubishi_msc_climate.h"

namespace esphome {
namespace mitsubishi_msc {

// Exposes all 7 vertical vane positions of the Mitsubishi MSC protocol as a
// select entity, since ESPHome's climate swing_mode only has 4 states
// (off/vertical/horizontal/both) and can't represent them all.
class MitsubishiMSCVaneSelect : public select::Select {
 public:
  void set_climate(MitsubishiMSCClimate *climate) { this->climate_ = climate; }

  // Called by MitsubishiMSCClimate::on_receive() to reflect a vane position
  // seen from the physical remote, without transmitting anything.
  void publish_vane_state(uint8_t vane_code);

 protected:
  void control(const std::string &value) override;

  MitsubishiMSCClimate *climate_{nullptr};
};

}  // namespace mitsubishi_msc
}  // namespace esphome
