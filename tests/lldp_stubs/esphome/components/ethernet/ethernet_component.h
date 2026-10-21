#pragma once
#include <vector>
#include <cstdint>
#include <cstring>
using esp_err_t=int;constexpr int ESP_OK=0;
namespace esphome::ethernet {class EthernetComponent {public:using Handler=bool(*)(void*,const uint8_t*,size_t);Handler handler=nullptr;void *ctx=nullptr;bool ready=false;int send_result=0;std::vector<std::vector<uint8_t>> sent;bool add_l2_listener(uint16_t type,Handler h,void*c){if(type!=0x88cc||handler)return false;handler=h;ctx=c;return true;}bool is_l2_ready()const{return ready;}void get_eth_mac_address_raw(uint8_t*p){uint8_t m[6]={2,0,0,0,0,1};memcpy(p,m,6);}int transmit_l2(const uint8_t*p,size_t n){if(!send_result)sent.emplace_back(p,p+n);return send_result;}bool inject(const std::vector<uint8_t>&v){return handler(ctx,v.data(),v.size());}};}
