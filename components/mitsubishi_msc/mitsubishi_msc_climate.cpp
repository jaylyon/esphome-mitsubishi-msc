#include "mitsubishi_msc_climate.h"
#include "mitsubishi_msc_protocol.h"
#include "mitsubishi_msc_vane_select.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mitsubishi_msc {

static const char *const TAG = "mitsubishi_msc.climate";

void MitsubishiMSCClimate::setup() {
  climate_ir::ClimateIR::setup();
  // The select entity has no state of its own to restore, so give it an
  // initial value here rather than leaving it stuck on "Unknown" until the
  // first manual change or a frame from the physical remote arrives.
  if (this->vane_select_ != nullptr)
    this->vane_select_->publish_vane_state(this->vane_position_);
}

void MitsubishiMSCClimate::set_vane_position(uint8_t vane_code) {
  this->vane_position_ = vane_code;
  this->transmit_state();
  this->publish_state();
}

void MitsubishiMSCClimate::transmit_state() {
  uint8_t power = MSC_POWER_OFF;
  uint8_t mode = MSC_MODE_AUTO;

  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      power = MSC_POWER_ON;
      mode = MSC_MODE_COOL;
      break;
    case climate::CLIMATE_MODE_DRY:
      power = MSC_POWER_ON;
      mode = MSC_MODE_DRY;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      power = MSC_POWER_ON;
      mode = MSC_MODE_FAN;
      break;
    case climate::CLIMATE_MODE_HEAT:
      power = MSC_POWER_ON;
      mode = MSC_MODE_HEAT;
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
    case climate::CLIMATE_MODE_AUTO:
      power = MSC_POWER_ON;
      mode = MSC_MODE_AUTO;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      power = MSC_POWER_OFF;
      mode = MSC_MODE_AUTO;
      break;
  }

  // Fix for the generic ESPHome heatpumpir wrapper's fan mapping bug (it sends
  // FAN_2/FAN_3/FAN_4, which only line up correctly for protocols with 4+ fan
  // speeds). This protocol has exactly 3 real speeds, confirmed against the
  // physical remote: Low=0x02, Medium=0x03, High=0x05.
  uint8_t fan_speed = MSC_FAN_AUTO;
  switch (this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO)) {
    case climate::CLIMATE_FAN_LOW:
      fan_speed = MSC_FAN_LOW;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      fan_speed = MSC_FAN_MEDIUM;
      break;
    case climate::CLIMATE_FAN_HIGH:
      fan_speed = MSC_FAN_HIGH;
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      fan_speed = MSC_FAN_AUTO;
      break;
  }

  uint8_t temperature = (uint8_t) clamp(this->target_temperature, this->min_temperature_, this->max_temperature_);

  uint8_t frame[MSC_FRAME_LEN];
  msc_build_frame(frame, power, mode, temperature, fan_speed, this->vane_position_);

  auto call = this->transmitter_->transmit();
  auto *data = call.get_data();
  data->set_carrier_frequency(MSC_CARRIER_HZ);

  data->mark(MSC_HDR_MARK);
  data->space(MSC_HDR_SPACE);

  for (uint8_t i = 0; i < MSC_FRAME_LEN; i++) {
    uint8_t byte = frame[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      data->mark(MSC_BIT_MARK);
      data->space((byte & 0x01) ? MSC_ONE_SPACE : MSC_ZERO_SPACE);
      byte >>= 1;
    }
  }

  data->mark(MSC_BIT_MARK);
  data->space(0);

  call.perform();
}

bool MitsubishiMSCClimate::on_receive(remote_base::RemoteReceiveData data) {
  if (!data.expect_item(MSC_HDR_MARK, MSC_HDR_SPACE))
    return false;

  uint8_t frame[MSC_FRAME_LEN] = {0};
  for (uint8_t i = 0; i < MSC_FRAME_LEN; i++) {
    uint8_t value = 0;
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (data.expect_item(MSC_BIT_MARK, MSC_ONE_SPACE)) {
        value |= (1 << bit);
      } else if (data.expect_item(MSC_BIT_MARK, MSC_ZERO_SPACE)) {
        // bit already 0
      } else {
        return false;  // not a (complete) Mitsubishi MSC frame
      }
    }
    frame[i] = value;
  }

  if (!data.expect_mark(MSC_BIT_MARK))
    return false;

  if (!msc_frame_has_prefix(frame))
    return false;

  if (!msc_frame_checksum_ok(frame)) {
    ESP_LOGW(TAG, "Discarding Mitsubishi MSC frame with bad checksum");
    return false;
  }

  ESP_LOGD(TAG, "Received Mitsubishi MSC frame: power=0x%02X mode=0x%02X temp=0x%02X fan/vane=0x%02X", frame[5],
            frame[6], frame[7], frame[8]);

  bool power_on = (frame[5] & MSC_POWER_ON_BIT) != 0;
  if (!power_on) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else {
    switch (frame[6] & 0x0F) {
      case MSC_MODE_HEAT:
        if (this->supports_heat_)
          this->mode = climate::CLIMATE_MODE_HEAT;
        else
          ESP_LOGW(TAG, "Ignoring Heat mode from remote (supports_heat is false)");
        break;
      case MSC_MODE_DRY:
        this->mode = climate::CLIMATE_MODE_DRY;
        break;
      case MSC_MODE_COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        break;
      case MSC_MODE_FAN:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
      case MSC_MODE_AUTO:
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        break;
      default:
        ESP_LOGW(TAG, "Unknown Mitsubishi MSC operating mode 0x%02X", frame[6]);
        break;
    }
  }

  float temperature = 31 - frame[7];
  if (temperature >= this->min_temperature_ && temperature <= this->max_temperature_)
    this->target_temperature = temperature;

  switch (frame[8] & MSC_FAN_MASK) {
    case MSC_FAN_AUTO:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
    case MSC_FAN_LOW:
      this->fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case MSC_FAN_MEDIUM:
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
      break;
    case MSC_FAN_HIGH:
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
    default:
      ESP_LOGW(TAG, "Unknown Mitsubishi MSC fan speed 0x%02X", frame[8] & MSC_FAN_MASK);
      break;
  }

  this->vane_position_ = frame[8] & MSC_VANE_MASK;
  if (this->vane_select_ != nullptr)
    this->vane_select_->publish_vane_state(this->vane_position_);

  // Reflect the physical remote's command in Home Assistant WITHOUT
  // re-transmitting it -- publish_state() only updates local state.
  this->publish_state();
  return true;
}

}  // namespace mitsubishi_msc
}  // namespace esphome
