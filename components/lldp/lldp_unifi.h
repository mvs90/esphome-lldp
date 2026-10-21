// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "lldp_protocol.h"
namespace esphome::lldp {
struct UniFiPortRule {
  uint64_t chassis_mac;
  uint64_t port_mac_base;
  unsigned port_count;
};
struct UniFiPortResult {
  unsigned port;
  const char *status;
};
inline uint64_t mac_number(const uint8_t *bytes) {
  uint64_t value=0;
  for(unsigned i=0;i<6;i++)value=(value<<8)|bytes[i];
  return value;
}
// Only explicitly calibrated switch identities are eligible. LLDP port strings
// remain raw; this is an additional diagnostic, never a forwarding decision.
inline UniFiPortResult unifi_port(const protocol::Neighbor &peer,const std::vector<UniFiPortRule> &rules) {
  if(rules.empty())return {0,"disabled"};
  if(peer.chassis_key.size()!=7 || uint8_t(peer.chassis_key[0])!=4)return {0,"non_mac_chassis"};
  auto chassis=mac_number(reinterpret_cast<const uint8_t *>(peer.chassis_key.data()+1));
  auto source=mac_number(peer.source_mac.data());
  for(const auto &rule:rules) {
    if(rule.chassis_mac!=chassis)continue;
    if((peer.source_mac[0]&1)||!source||!rule.port_count||rule.port_count>256 || source<=rule.port_mac_base || source-rule.port_mac_base>rule.port_count)return {0,"outside_calibrated_range"};
    return {unsigned(source-rule.port_mac_base),"calibrated_source_mac_offset"};
  }
  return {0,"uncalibrated_switch"};
}
}
