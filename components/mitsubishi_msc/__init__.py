import esphome.codegen as cg
from esphome.components import climate_ir

mitsubishi_msc_ns = cg.esphome_ns.namespace("mitsubishi_msc")
MitsubishiMSCClimate = mitsubishi_msc_ns.class_("MitsubishiMSCClimate", climate_ir.ClimateIR)
