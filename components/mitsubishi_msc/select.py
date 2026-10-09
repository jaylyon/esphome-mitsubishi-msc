import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.types import ConfigType

from . import MitsubishiMSCClimate, mitsubishi_msc_ns

CODEOWNERS = ["@jaylyon"]

CONF_MITSUBISHI_MSC_ID = "mitsubishi_msc_id"

MitsubishiMSCVaneSelect = mitsubishi_msc_ns.class_("MitsubishiMSCVaneSelect", select.Select)

VANE_OPTIONS = [
    "Auto",
    "Highest",
    "Second Highest",
    "Middle",
    "Next Lower",
    "Lowest",
    "Swing",
]

CONFIG_SCHEMA = select.select_schema(MitsubishiMSCVaneSelect).extend(
    {
        cv.Required(CONF_MITSUBISHI_MSC_ID): cv.use_id(MitsubishiMSCClimate),
    }
)


async def to_code(config: ConfigType) -> None:
    var = await select.new_select(config, options=VANE_OPTIONS)
    climate_var = await cg.get_variable(config[CONF_MITSUBISHI_MSC_ID])
    cg.add(var.set_climate(climate_var))
    cg.add(climate_var.set_vane_select(var))
