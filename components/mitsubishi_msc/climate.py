import esphome.codegen as cg
from esphome.components import climate_ir
import esphome.config_validation as cv
from esphome.const import CONF_MAX_TEMPERATURE, CONF_MIN_TEMPERATURE, CONF_SUPPORTS_HEAT, CONF_VISUAL
from esphome.types import ConfigType

from . import MitsubishiMSCClimate

AUTO_LOAD = ["climate_ir"]

CODEOWNERS = ["@jaylyon"]


def _default_visual(config: ConfigType) -> ConfigType:
    # Seed the visual min/max shown in Home Assistant from the same
    # min/max_temperature values used to clamp the transmitted IR command.
    visual = config.setdefault(CONF_VISUAL, {})
    visual.setdefault(CONF_MAX_TEMPERATURE, config[CONF_MAX_TEMPERATURE])
    visual.setdefault(CONF_MIN_TEMPERATURE, config[CONF_MIN_TEMPERATURE])
    return config


CONFIG_SCHEMA = cv.All(
    climate_ir.climate_ir_with_receiver_schema(MitsubishiMSCClimate).extend(
        {
            # These units are cooling-only; override the climate_ir default of True.
            cv.Optional(CONF_SUPPORTS_HEAT, default=False): cv.boolean,
            cv.Optional(CONF_MIN_TEMPERATURE, default=17): cv.temperature,
            cv.Optional(CONF_MAX_TEMPERATURE, default=30): cv.temperature,
        }
    ),
    cv.Any(cv.only_with_arduino, cv.only_on_esp32),
    _default_visual,
)


async def to_code(config: ConfigType) -> None:
    var = await climate_ir.new_climate_ir(config)
    cg.add(var.set_min_temperature(config[CONF_MIN_TEMPERATURE]))
    cg.add(var.set_max_temperature(config[CONF_MAX_TEMPERATURE]))
