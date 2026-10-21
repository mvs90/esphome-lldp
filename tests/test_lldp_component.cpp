// SPDX-License-Identifier: GPL-3.0-only
#include "lldp_component.h"
#include "esphome/core/hal.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <iostream>
using namespace esphome;using namespace esphome::lldp;
static std::vector<uint8_t> load(const char *p){std::ifstream f(p,std::ios::binary);assert(f);return {(std::istreambuf_iterator<char>(f)),{}};}
int main(int argc,char **argv){
 assert(argc==2);auto peer=load(argv[1]);ethernet::EthernetComponent eth;LLDPComponent agent;LLDPEnableSwitch toggle;text_sensor::TextSensor sensors[8];
 agent.set_ethernet(&eth);agent.set_enable_switch(&toggle);toggle.set_parent(&agent);for(unsigned i=0;i<8;i++)agent.set_sensor(i,&sensors[i]);agent.setup();assert(!agent.failed&&toggle.state&&sensors[6].state=="link_down");
 assert(eth.inject(peer));test_millis=1001;agent.loop();assert(eth.sent.empty()&&sensors[0].state.empty());
 eth.ready=true;test_millis=2001;agent.loop();assert(eth.sent.size()==1);protocol::Neighbor self;assert(protocol::decode(eth.sent.back().data(),eth.sent.back().size(),self)&&self.name=="Test-Node");
 assert(eth.inject(peer));test_millis=3001;agent.loop();assert(sensors[0].state=="Switch-Test"&&sensors[2].state=="Port 9"&&sensors[6].state=="neighbor_present");
 // Malformed packets do not replace or renew a neighbor.
 auto bad=peer;bad[15]=255;eth.inject(bad);test_millis=4001;agent.loop();assert(sensors[0].state=="Switch-Test"&&sensors[7].state.find("invalid=1")!=std::string::npos);
 test_millis=123001;agent.loop();assert(sensors[0].state.empty()&&sensors[6].state=="expired");
 // Foreign shutdown must not delete the current identity; matching TTL0 does.
 eth.inject(peer);test_millis=124001;agent.loop();protocol::Identity foreign;foreign.port="other";std::vector<uint8_t> shutdown;uint8_t mac[6]={2,0,0,0,0,2};assert(protocol::encode(foreign,mac,shutdown,true));eth.inject(shutdown);test_millis=125001;agent.loop();assert(sensors[0].state=="Switch-Test");
 foreign.port="Port 9";assert(protocol::encode(foreign,mac,shutdown,true));eth.inject(shutdown);test_millis=126001;agent.loop();assert(sensors[0].state.empty());
 // Bounded receive queue reports overflow and consumes only LLDP frames.
 assert(eth.inject(peer)&&eth.inject(peer)&&eth.inject(peer));test_millis=127001;agent.loop();assert(sensors[7].state.find("queue_drop=1")!=std::string::npos);auto not_lldp=peer;not_lldp[12]=8;assert(!eth.inject(not_lldp));
 size_t tx=eth.sent.size();toggle.control(false);assert(!toggle.state&&sensors[6].state=="disabled"&&eth.sent.size()==tx+1);assert(protocol::decode(eth.sent.back().data(),eth.sent.back().size(),self)&&!self.ttl);
 eth.inject(peer);test_millis+=60000;agent.loop();assert(eth.sent.size()==tx+1&&sensors[0].state.empty());
 toggle.control(true);test_millis+=1001;agent.loop();assert(eth.sent.size()==tx+2);eth.inject(peer);test_millis+=1001;agent.loop();assert(sensors[0].state=="Switch-Test");
 agent.set_system_name("Runtime-Name");test_millis+=1001;agent.loop();assert(protocol::decode(eth.sent.back().data(),eth.sent.back().size(),self)&&self.name=="Runtime-Name");
 eth.ready=false;test_millis+=1001;agent.loop();assert(sensors[0].state.empty()&&sensors[6].state=="link_down");
 eth.ready=true;test_millis+=1001;agent.loop();eth.send_result=1;agent.set_system_name("Retry");test_millis+=1001;agent.loop();assert(sensors[7].state.find("tx_rejected=1")!=std::string::npos);
 // TX rejection retries at one-second bound and retains the pending identity.
 eth.send_result=0;test_millis+=999;size_t previous=eth.sent.size();agent.loop();assert(eth.sent.size()==previous);
 test_millis+=2;agent.loop();assert(eth.sent.size()==previous+1);assert(protocol::decode(eth.sent.back().data(),eth.sent.back().size(),self)&&self.name=="Retry");
 for(unsigned mode:{1u,2u}){
   ethernet::EthernetComponent port;port.ready=true;LLDPComponent isolated;isolated.set_ethernet(&port);isolated.set_mode(mode);text_sensor::TextSensor name;isolated.set_sensor(0,&name);isolated.setup();
   test_millis+=2000;assert(port.inject(peer));isolated.loop();assert((!name.state.empty())==(mode==1));assert((!port.sent.empty())==(mode==2));
 }
 ethernet::EthernetComponent off_port;off_port.ready=true;LLDPComponent off;off.set_ethernet(&off_port);off.set_enabled(false);off.setup();test_millis+=2000;off_port.inject(peer);off.loop();assert(off_port.sent.empty());
 // Calibrated source-MAC diagnostics must not replace the received Port ID.
 ethernet::EthernetComponent mapped_port;mapped_port.ready=true;LLDPComponent mapped;mapped.set_ethernet(&mapped_port);text_sensor::TextSensor extra[3];
 for(unsigned i=0;i<3;i++)mapped.set_sensor(8+i,&extra[i]);mapped.add_unifi_port_rule(0x020000000002ULL,0x020000000000ULL,24);mapped.setup();test_millis+=2000;mapped_port.inject(peer);mapped.loop();
 assert(extra[0].state=="02:00:00:00:00:02"&&extra[1].state=="2"&&extra[2].state=="calibrated_source_mac_offset");mapped_port.ready=false;test_millis+=1001;mapped.loop();assert(extra[0].state.empty()&&extra[1].state.empty()&&extra[2].state=="no_neighbor");
 std::cout<<"PASS actual LLDP component: IP-independent RX/TX, bounded queue, invalid/no renewal, TTL/shutdown identity, disable/enable, runtime identity, link loss, TX failure\n";
}
