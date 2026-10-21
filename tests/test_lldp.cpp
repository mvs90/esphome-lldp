// SPDX-License-Identifier: GPL-3.0-only
#include "lldp_protocol.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <random>
#include <iostream>
using namespace esphome::lldp::protocol;
static std::vector<uint8_t> load(const char *p){std::ifstream f(p,std::ios::binary);assert(f);return {(std::istreambuf_iterator<char>(f)),{}};}
int main(int argc,char **argv){
 assert(argc==3);auto node=load(argv[1]),peer=load(argv[2]);Neighbor n;
 assert(decode(peer.data(),peer.size(),n));assert(n.name=="Switch-Test"&&n.port=="Port 9"&&n.ttl==120&&n.management=="192.0.2.254");
 assert(n.chassis=="02:00:00:00:00:02"&&n.port_description=="Test port"&&n.capabilities==4&&n.enabled_capabilities==4);
 Identity id;id.name="Node-Test";id.management={1,192,0,2,1};uint8_t mac[6]={2,0,0,0,0,1};std::vector<uint8_t> wire;
 assert(encode(id,mac,wire)&&wire==node);assert(decode(wire.data(),wire.size(),n));assert(n.name==id.name&&n.management=="192.0.2.1");
 assert(encode(id,mac,wire,true)&&wire.size()==60);assert(decode(wire.data(),wire.size(),n)&&!n.ttl&&n.name.empty()&&n.description.empty());
 for(size_t len=0;len<peer.size();len++){Neighbor untouched;untouched.name="sentinel";assert(!decode(peer.data(),len,untouched));assert(untouched.name=="sentinel");}
 auto bad=peer;bad[12]=8;assert(!decode(bad.data(),bad.size(),n));bad=peer;bad[0]=2;assert(!decode(bad.data(),bad.size(),n));
 bad=peer;bad[6]|=1;assert(!decode(bad.data(),bad.size(),n));bad=peer;std::fill(bad.begin()+6,bad.begin()+12,0);assert(!decode(bad.data(),bad.size(),n));
 bad=peer;bad[14]=4;assert(!decode(bad.data(),bad.size(),n));bad=peer;bad[15]=255;assert(!decode(bad.data(),bad.size(),n));
 // Duplicate mandatory TLV, illegal fixed length, unknown vendor TLV accepted.
 bad=peer;bad.insert(bad.begin()+23,peer.begin()+14,peer.begin()+23);assert(!decode(bad.data(),bad.size(),n));
 bad=peer;bad.insert(bad.end()-2,{0xfe,4,0,1,2,3});assert(decode(bad.data(),bad.size(),n));
 // Inject malformed management address/OID and capability subset.
 for(size_t at=14;at+2<peer.size();){size_t len=be16(peer.data()+at)&511;unsigned type=be16(peer.data()+at)>>9;
  if(type==8){bad=peer;bad[at+2]=31;assert(!decode(bad.data(),bad.size(),n));bad=peer;bad[at+2+11]=100;assert(!decode(bad.data(),bad.size(),n));}
  if(type==7){bad=peer;bad[at+2+3]=0x80;assert(!decode(bad.data(),bad.size(),n));}
  at+=2+len;if(!type)break;
 }
 assert(expired(1000,0,1)&&!expired(999,0,1));assert(!expired(499,UINT32_MAX-500,2)&&expired(1500,UINT32_MAX-500,2));
 id.management={2,0x20,1,0x0d,0xb8,0,0,0,0,0,0,0,0,0,0,0,1};assert(encode(id,mac,wire));assert(decode(wire.data(),wire.size(),n)&&n.management=="2001:db8:0:0:0:0:0:1");
 id.chassis="custom-chassis";id.port="custom-port";id.name="Gerät";assert(encode(id,mac,wire));assert(decode(wire.data(),wire.size(),n)&&n.chassis==id.chassis&&n.port==id.port&&n.name==id.name);
 const uint8_t invalid[]={0xc0,0x80,0xed,0xa0,0x80,0xff,0,10,'x'};auto clean=text(invalid,sizeof(invalid));assert(clean.find('\0')==std::string::npos&&clean.back()=='x');
 std::string long_text(254,'a');long_text+="ä";assert(text(reinterpret_cast<const uint8_t *>(long_text.data()),long_text.size()).size()==254);
 id.name=std::string(256,'x');assert(!encode(id,mac,wire));id.name="ok";id.port="";assert(!encode(id,mac,wire));id.port="eth0";id.management={1,1};assert(!encode(id,mac,wire));
 std::mt19937 rng(8021);std::vector<uint8_t> fuzz(1600);
 for(unsigned trial=0;trial<20000;trial++){for(auto &x:fuzz)x=uint8_t(rng());if(trial&1){memcpy(fuzz.data(),DEST,6);fuzz[12]=0x88;fuzz[13]=0xcc;}decode(fuzz.data(),rng()%1600,n);}
 std::cout<<"PASS independent LLDP wire goldens, RX/TX/shutdown, malformed TLVs, UTF8, IPv4/6, TTL wrap,20000 malformed frames\n";
}
