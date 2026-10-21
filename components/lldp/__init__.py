# SPDX-License-Identifier: MIT
"""Optional classic LLDP agent for this project's Ethernet frame interface."""
import ipaddress
from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components import ethernet, text_sensor, switch
from esphome.const import CONF_ID, ENTITY_CATEGORY_DIAGNOSTIC
DEPENDENCIES = ['ethernet']
AUTO_LOAD = ['text_sensor', 'switch']
ns = cg.esphome_ns.namespace('lldp')
LLDPComponent = ns.class_('LLDPComponent', cg.Component)
LLDPEnableSwitch = ns.class_('LLDPEnableSwitch', switch.Switch)
MODES = {'RX_ONLY': 1, 'TX_ONLY': 2, 'RX_TX': 3}
FIELDS = ['neighbor_system_name','neighbor_chassis_id','neighbor_port_id','neighbor_port_description','neighbor_system_description','neighbor_management_address','status','statistics','neighbor_source_mac','neighbor_physical_port','neighbor_port_mapping']
NAMES = ['LLDP Neighbor System Name','LLDP Neighbor Chassis ID','LLDP Neighbor Port ID','LLDP Neighbor Port Description','LLDP Neighbor System Description','LLDP Neighbor Management Address','LLDP Status','LLDP Statistics','LLDP Neighbor Source MAC','LLDP Neighbor Physical Port','LLDP Neighbor Port Mapping']
def bounded(value):
    value=cv.string_strict(value)
    if len(value.encode('utf-8'))>255:raise cv.Invalid('LLDP strings must fit255 UTF-8 bytes')
    if any(ord(c)<32 or ord(c)==127 for c in value):raise cv.Invalid('LLDP strings must not contain control characters')
    return value

def unicast_mac(value):
    value=cv.mac_address(value)
    number=int(str(value).replace(':',''),16)
    if not number or number & (1<<40):raise cv.Invalid('Mapping requires a nonzero unicast MAC')
    return value

UNI_RULE=cv.Schema({cv.Required('chassis_mac'):unicast_mac,cv.Optional('port_mac_base'):unicast_mac,cv.Required('port_count'):cv.int_range(min=1,max=256)})
def validate_uni_rules(value):
    rules=cv.ensure_list(UNI_RULE)(value)
    if len(rules)>8:raise cv.Invalid('At most8 calibrated switch rules are supported')
    keys=[str(r['chassis_mac']) for r in rules]
    if len(set(keys))!=len(keys):raise cv.Invalid('Duplicate calibrated chassis MAC')
    return rules

def validate(config):
    if config['ttl']*1000<2*config['transmit_interval'].total_milliseconds:raise cv.Invalid('ttl must be at least twice transmit_interval')
    if config['enabled_capabilities']&~config['capabilities']:raise cv.Invalid('enabled_capabilities must be a subset of capabilities')
    if not config['port_id']:raise cv.Invalid('port_id must not be empty')
    return config
schema={
    cv.Optional('unifi_port_mapping',default=[]):validate_uni_rules,
    cv.GenerateID(): cv.declare_id(LLDPComponent),
    cv.GenerateID('ethernet_id'):cv.use_id(ethernet.EthernetComponent),
    cv.Optional('enabled',default=True):cv.boolean,
    cv.Optional('mode',default='RX_TX'):cv.one_of(*MODES,upper=True),
    cv.Optional('transmit_interval',default='30s'):cv.All(cv.positive_time_period_milliseconds,cv.Range(min=cv.TimePeriod(seconds=5),max=cv.TimePeriod(seconds=3600))),
    cv.Optional('ttl',default=120):cv.int_range(min=1,max=65535),
    cv.Optional('chassis_id',default=''):bounded,
    cv.Optional('port_id',default='eth0'):bounded,
    cv.Optional('system_name'):bounded,
    cv.Optional('port_description',default='USB Ethernet'):bounded,
    cv.Optional('system_description',default='ESPHome LLDP'):bounded,
    cv.Optional('capabilities',default=128):cv.int_range(min=0,max=65535),
    cv.Optional('enabled_capabilities',default=128):cv.int_range(min=0,max=65535),
    cv.Optional('management_address'):cv.ipaddress,
    cv.Optional('enable_switch',default={'name':'LLDP'}):switch.switch_schema(LLDPEnableSwitch,entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
}
for field,name in zip(FIELDS,NAMES):schema[cv.Optional(field,default={'name':name})]=text_sensor.text_sensor_schema(entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
CONFIG_SCHEMA=cv.All(cv.Schema(schema).extend(cv.COMPONENT_SCHEMA),validate,cv.only_on_esp32)
def final_validate(config):
    eth=fv.full_config.get().get('ethernet',{})
    if eth.get('type') not in ('USB_ECM','USB_RTL8152'):raise cv.Invalid('LLDP currently requires this project\'s USB Ethernet overlay')
FINAL_VALIDATE_SCHEMA=final_validate
async def to_code(config):
    var=cg.new_Pvariable(config[CONF_ID]);await cg.register_component(var,config)
    eth=await cg.get_variable(config['ethernet_id']);cg.add(var.set_ethernet(eth))
    cg.add(var.set_enabled(config['enabled']));cg.add(var.set_mode(MODES[config['mode']]))
    cg.add(var.set_interval(config['transmit_interval'].total_milliseconds));cg.add(var.set_ttl(config['ttl']))
    for field in ('chassis_id','port_id','system_name','port_description','system_description'):
        if field in config:cg.add(getattr(var,'set_'+field)(config[field]))
    cg.add(var.set_capabilities(config['capabilities'],config['enabled_capabilities']))
    if 'management_address' in config:
        addr=ipaddress.ip_address(str(config['management_address']));cg.add(var.set_management_address([1 if addr.version==4 else 2,*addr.packed]))
    for rule in config['unifi_port_mapping']:
        chassis=int(str(rule['chassis_mac']).replace(':',''),16);base=int(str(rule.get('port_mac_base',rule['chassis_mac'])).replace(':',''),16)
        cg.add(var.add_unifi_port_rule(chassis,base,rule['port_count']))
    for index,field in enumerate(FIELDS):
        sensor=await text_sensor.new_text_sensor(config[field]);cg.add(var.set_sensor(index,sensor))
    toggle=await switch.new_switch(config['enable_switch']);cg.add(toggle.set_parent(var));cg.add(var.set_enable_switch(toggle))
