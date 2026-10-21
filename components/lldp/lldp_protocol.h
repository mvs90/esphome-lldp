// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
namespace esphome::lldp::protocol {
inline constexpr uint8_t DEST[6]={1,0x80,0xc2,0,0,0x0e};
inline uint16_t be16(const uint8_t *p){return (uint16_t(p[0])<<8)|p[1];}
inline bool frame(const uint8_t *p,size_t n){return n>=14&&!memcmp(p,DEST,6)&&be16(p+12)==0x88cc;}
inline std::string hex(const uint8_t *p,size_t n){
  std::string s;char b[4];for(size_t i=0;i<n;i++){if(i)s+=':';snprintf(b,sizeof(b),"%02X",p[i]);s+=b;}return s;
}
// Bound HA text and replace invalid/control UTF-8. Never cut a code point.
inline std::string text(const uint8_t *p,size_t n){
  std::string s;for(size_t i=0;i<n&&s.size()<255;){
    uint8_t a=p[i];size_t k=a<0x80?1:(a>=0xc2&&a<=0xdf?2:(a>=0xe0&&a<=0xef?3:(a>=0xf0&&a<=0xf4?4:0)));
    bool ok=k&&i+k<=n;uint32_t cp=k==1?a:(a&((1u<<(7-k))-1));
    if(ok)for(size_t j=1;j<k;j++){if((p[i+j]&0xc0)!=0x80){ok=false;break;}cp=(cp<<6)|(p[i+j]&63);}
    if(ok&&((k==2&&cp<0x80)||(k==3&&cp<0x800)||(k==4&&cp<0x10000)||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff)))ok=false;
    if(!ok){s+='?';i++;continue;}
    if(cp<0x20||(cp>=0x7f&&cp<=0x9f)){s+=' ';i+=k;continue;}
    if(s.size()+k>255)break;s.append(reinterpret_cast<const char *>(p+i),k);i+=k;
  }return s;
}
inline std::string address(const uint8_t *p,size_t n){
  char b[64];if(n==5&&p[0]==1){snprintf(b,sizeof(b),"%u.%u.%u.%u",p[1],p[2],p[3],p[4]);return b;}
  if(n==17&&p[0]==2){snprintf(b,sizeof(b),"%x:%x:%x:%x:%x:%x:%x:%x",be16(p+1),be16(p+3),be16(p+5),be16(p+7),be16(p+9),be16(p+11),be16(p+13),be16(p+15));return b;}return hex(p,n);
}
struct Neighbor {
  std::array<uint8_t,6> source_mac{};
  std::string chassis_key,port_key,chassis,port,port_description,name,description,management;
  uint16_t ttl{0},capabilities{0},enabled_capabilities{0};
  bool same(const Neighbor &o)const{return chassis_key==o.chassis_key&&port_key==o.port_key;}
};
inline bool decode(const uint8_t *p,size_t n,Neighbor &out){
  if(!frame(p,n)||n>1514||(p[6]&1))return false;
  uint8_t any=0;for(unsigned i=6;i<12;i++)any|=p[i];if(!any)return false;
  Neighbor v;std::copy(p+6,p+12,v.source_mac.begin());unsigned mandatory=1;uint16_t seen=0;bool ended=false;size_t at=14;
  while(at<n){
    if(n-at<2)return false;uint16_t head=be16(p+at);at+=2;unsigned type=head>>9;size_t len=head&511;
    if(len>n-at)return false;const uint8_t *d=p+at;at+=len;
    if(!type){if(len||mandatory!=4)return false;ended=true;break;}
    if(mandatory<4){if(type!=mandatory)return false;mandatory++;}
    else if(type<=3)return false;
    if(type>=4&&type<=7){if(seen&(1u<<type))return false;seen|=1u<<type;}
    if(type==1||type==2){
      if(len<2||len>256||!d[0]||d[0]>7)return false;
      bool mac=d[0]==(type==1?4:3);bool net=d[0]==(type==1?5:4);
      if(mac&&len!=7)return false;
      if(net&&(len<3||(d[1]==1&&len!=6)||(d[1]==2&&len!=18)))return false;
      auto value=mac?hex(d+1,len-1):(net?address(d+1,len-1):text(d+1,len-1));
      auto key=std::string(reinterpret_cast<const char *>(d),len);
      if(type==1){v.chassis=value;v.chassis_key=key;}else{v.port=value;v.port_key=key;}
    }else if(type==3){if(len!=2)return false;v.ttl=be16(d);}
    else if(type==4){if(len>255)return false;v.port_description=text(d,len);}
    else if(type==5){if(len>255)return false;v.name=text(d,len);}
    else if(type==6){if(len>255)return false;v.description=text(d,len);}
    else if(type==7){if(len!=4)return false;v.capabilities=be16(d);v.enabled_capabilities=be16(d+2);if(v.enabled_capabilities&~v.capabilities)return false;}
    else if(type==8){
      if(len<1||d[0]<2||d[0]>31)return false;size_t pos=1+d[0];
      if(pos+6>len||d[pos]<1||d[pos]>3||pos+6+d[pos+5]!=len)return false;
      if((d[1]==1&&d[0]!=5)||(d[1]==2&&d[0]!=17))return false;
      if(v.management.empty()&&(d[1]==1||d[1]==2))v.management=address(d+1,d[0]);
    }
  }
  if(!ended)return false;out=std::move(v);return true;
}
struct Identity {
  std::string chassis,port{"eth0"},port_description{"USB Ethernet"},name,description{"ESPHome LLDP"};
  uint16_t ttl{120},capabilities{0x80},enabled_capabilities{0x80};
  std::vector<uint8_t> management;
};
inline bool encode(const Identity &id,const uint8_t *mac,std::vector<uint8_t> &out,bool shutdown=false){
  if(!mac||(mac[0]&1)||!(mac[0]|mac[1]|mac[2]|mac[3]|mac[4]|mac[5])||id.port.empty()||id.port.size()>255||id.chassis.size()>255||id.name.size()>255||id.description.size()>255||id.port_description.size()>255||(!id.ttl&&!shutdown)||(id.enabled_capabilities&~id.capabilities))return false;
  if(!id.management.empty()&&!((id.management[0]==1&&id.management.size()==5)||(id.management[0]==2&&id.management.size()==17)))return false;
  std::vector<uint8_t> v;v.reserve(512);v.insert(v.end(),DEST,DEST+6);v.insert(v.end(),mac,mac+6);v.push_back(0x88);v.push_back(0xcc);
  auto tlv=[&](unsigned type,const uint8_t *data,size_t n){uint16_t h=(type<<9)|n;v.push_back(h>>8);v.push_back(h);if(n)v.insert(v.end(),data,data+n);};
  std::vector<uint8_t> chassis;if(id.chassis.empty()){chassis.push_back(4);chassis.insert(chassis.end(),mac,mac+6);}else{chassis.push_back(7);chassis.insert(chassis.end(),id.chassis.begin(),id.chassis.end());}tlv(1,chassis.data(),chassis.size());
  std::vector<uint8_t> port{7};port.insert(port.end(),id.port.begin(),id.port.end());tlv(2,port.data(),port.size());
  uint16_t lifetime=shutdown?0:id.ttl;uint8_t ttl[2]={uint8_t(lifetime>>8),uint8_t(lifetime)};tlv(3,ttl,2);
  if(!shutdown){
    auto optional=[&](unsigned type,const std::string &s){if(!s.empty())tlv(type,reinterpret_cast<const uint8_t *>(s.data()),s.size());};
    optional(4,id.port_description);optional(5,id.name);optional(6,id.description);
    uint8_t cap[4]={uint8_t(id.capabilities>>8),uint8_t(id.capabilities),uint8_t(id.enabled_capabilities>>8),uint8_t(id.enabled_capabilities)};tlv(7,cap,4);
    if(!id.management.empty()){std::vector<uint8_t> management{uint8_t(id.management.size())};management.insert(management.end(),id.management.begin(),id.management.end());management.insert(management.end(),{1,0,0,0,0,0});tlv(8,management.data(),management.size());}
  }
  tlv(0,nullptr,0);if(v.size()>1514)return false;if(v.size()<60)v.resize(60,0);out=std::move(v);return true;
}
inline bool expired(uint32_t now,uint32_t last,uint16_t ttl){return uint32_t(now-last)>=uint32_t(ttl)*1000;}
}
