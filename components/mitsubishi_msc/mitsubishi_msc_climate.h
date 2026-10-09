#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace mitsubishi_msc {

class MitsubishiMSCVaneSelect;  // defined in mitsubishi_msc_vane_select.h

class MitsubishiMSCClimate : public climate_ir::ClimateIR {
 public:
  MitsubishiMSCClimate()
      : climate_ir::ClimateIR(
            /* minimum_temperature= */ 0, /* maximum_temperature= */ 100, /* temperature_step= */ 1.0f,
            /* supports_dry= */ true, /* supports_fan_only= */ true,
            {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
             climate::CLIMATE_FAN_HIGH},
            {} /* no climate swing modes -- vane is exposed via a separate select, see MitsubishiMSCVaneSelect */
        ) {}

  void set_min_temperature(float min_temperature) { this->min_temperature_ = min_temperature; }
  void set_max_temperature(float max_temperature) { this->max_temperature_ = max_temperature; }

  // Wired up by the `mitsubishi_msc` select platform so that received vane
  // state can be reflected back into Home Assistant.
  void set_vane_select(MitsubishiMSCVaneSelect *vane_select) { this->vane_select_ = vane_select; }

  // Called by MitsubishiMSCVaneSelect::control() when the vane select changes.
  void set_vane_position(uint8_t vane_code);

 protected:
  void setup() override;
  void transmit_state() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;

  float min_temperature_{17};
  float max_temperature_{30};
  uint8_t vane_position_{0x00};  // MSC_VANE_AUTO
  MitsubishiMSCVaneSelect *vane_select_{nullptr};
};

}  // namespace mitsubishi_msc
}  // namespace esphome
