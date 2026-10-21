// SPDX-License-Identifier: GPL-3.0-only
#include "lldp_unifi.h"
#include <cassert>
#include <iostream>
using namespace esphome::lldp;
static void set_mac(uint64_t value,std::array<uint8_t,6> &bytes){for(int i=5;i>=0;i--){bytes[i]=value;value>>=8;}}
static protocol::Neighbor peer(uint64_t chassis,uint64_t source){protocol::Neighbor p;std::array<uint8_t,6> key{};set_mac(chassis,key);p.chassis_key=std::string(1,char(4))+std::string(reinterpret_cast<const char*>(key.data()),6);set_mac(source,p.source_mac);p.port="te1/0/1";return p;}
int main(){
 constexpr uint64_t base=0x0200000010a2ULL;std::vector<UniFiPortRule> rules{{base,base,24}};
 for(unsigned port:{1u,7u,9u,20u,21u,24u}){auto n=peer(base,base+port);auto r=unifi_port(n,rules);assert(r.port==port&&n.port=="te1/0/1");}
 for(uint64_t source:{base-1,base,base+25})assert(!unifi_port(peer(base,source),rules).port);
 assert(!unifi_port(peer(base+1,base+9),rules).port);assert(!unifi_port(peer(base,base+9),{}).port);
 auto non_mac=peer(base,base+9);non_mac.chassis_key[0]=7;assert(!unifi_port(non_mac,rules).port);
 // Whole48-bit arithmetic must carry across the last byte; no OUI/low-byte guess.
 constexpr uint64_t carry=0x0200000010feULL;rules={{carry,carry,24}};assert(unifi_port(peer(carry,carry+21),rules).port==21);
 rules={{base,base-4,24}};assert(unifi_port(peer(base,base+9),rules).port==13);
 rules={{base,base,0}};assert(!unifi_port(peer(base,base+1),rules).port);
 rules={{base,base,257}};assert(!unifi_port(peer(base,base+1),rules).port);
 std::cout<<"PASS calibrated UniFi mapping: raw IDs preserved, bounded identities/ranges, full48-bit carry, unsupported peers fail closed\n";
}
