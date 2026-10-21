// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "esphome/core/component.h"
#include "esphome/components/ethernet/ethernet_component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/switch/switch.h"
#include "freertos/FreeRTOS.h"
#include <atomic>
#include "lldp_protocol.h"
#include "lldp_unifi.h"
namespace esphome::lldp {
class LLDPComponent;
class LLDPEnableSwitch final:public switch_::Switch {
 public:void set_parent(LLDPComponent *p){parent_=p;}
 protected:void write_state(bool state) override;LLDPComponent *parent_{nullptr};
};
class LLDPComponent final:public Component {
 public:
#ifdef USE_RTL_FAULT_INJECTION
  std::string last_wire_hex() const;
#endif
  void setup() override;void loop() override;void dump_config() override;
  void on_shutdown() override;
  float get_setup_priority() const override{return setup_priority::ETHERNET-1.0f;}
  void set_ethernet(ethernet::EthernetComponent *e){ethernet_=e;}
  void set_enabled(bool enabled);bool is_enabled()const{return enabled_.load();}
  void set_mode(unsigned mode){mode_=mode;force_tx_=true;}
  void set_interval(uint32_t ms){interval_=ms;}
  void set_ttl(uint16_t ttl){identity_.ttl=ttl;}
  void set_chassis_id(const std::string &s){identity_.chassis=s;}
  void set_port_id(const std::string &s){identity_.port=s;}
  void set_system_name(const std::string &s){identity_.name=protocol::text(reinterpret_cast<const uint8_t *>(s.data()),s.size());force_tx_=true;}
  void set_port_description(const std::string &s){identity_.port_description=protocol::text(reinterpret_cast<const uint8_t *>(s.data()),s.size());force_tx_=true;}
  void set_system_description(const std::string &s){identity_.description=protocol::text(reinterpret_cast<const uint8_t *>(s.data()),s.size());force_tx_=true;}
  const std::string &get_system_name()const{return identity_.name;}
  const std::string &get_port_description()const{return identity_.port_description;}
  const std::string &get_system_description()const{return identity_.description;}
  void set_capabilities(uint16_t supported,uint16_t enabled){identity_.capabilities=supported;identity_.enabled_capabilities=enabled;}
  void set_management_address(const std::vector<uint8_t> &v){identity_.management=v;}
  void add_unifi_port_rule(uint64_t chassis,uint64_t base,unsigned count){unifi_rules_.push_back({chassis,base,count});}
  void set_sensor(unsigned index,text_sensor::TextSensor *s){if(index<11)sensors_[index]=s;}
  void set_enable_switch(LLDPEnableSwitch *s){switch_=s;}
 protected:
  static bool receive(void *context,const uint8_t *data,size_t length);
  bool send_(bool shutdown=false);void publish_();void clear_();
  ethernet::EthernetComponent *ethernet_{nullptr};LLDPEnableSwitch *switch_{nullptr};
  std::array<text_sensor::TextSensor *,11> sensors_{};
  std::vector<UniFiPortRule> unifi_rules_;
  protocol::Identity identity_;protocol::Neighbor neighbor_;
  std::atomic<bool> enabled_{true};std::atomic<unsigned> mode_{3};
  struct Packet {uint16_t length;uint8_t bytes[1514];};Packet queue_[2]{};
#ifdef USE_RTL_FAULT_INJECTION
  Packet last_wire_{};
#endif
  portMUX_TYPE mux_=portMUX_INITIALIZER_UNLOCKED;unsigned head_{0},count_{0};
  std::atomic<uint32_t> queue_drops_{0};
  uint32_t last_seen_{0},last_tx_{0},last_publish_{0},interval_{30000};
  uint32_t received_{0},transmitted_{0},invalid_{0},tx_rejected_{0};
  bool setup_done_{false},neighbor_present_{false},expired_{false},was_link_{false},force_tx_{true};
};
}
