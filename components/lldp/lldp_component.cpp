// SPDX-License-Identifier: GPL-3.0-only
#include "lldp_component.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
namespace esphome::lldp {
static const char *const TAG="lldp";
void LLDPEnableSwitch::write_state(bool state){if(parent_)parent_->set_enabled(state);publish_state(state);}
void LLDPComponent::setup(){
  if(!ethernet_||!ethernet_->add_l2_listener(0x88cc,receive,this)){ESP_LOGE(TAG,"Ethernet LLDP listener unavailable");mark_failed();return;}
  setup_done_=true;
  if(identity_.name.empty())identity_.name=App.get_name();
  if(switch_)switch_->publish_state(enabled_.load());publish_();
}
void LLDPComponent::dump_config(){
  ESP_LOGCONFIG(TAG,"LLDP: enabled=%u mode=%u interval=%us ttl=%us",enabled_.load(),mode_.load(),unsigned(interval_/1000),identity_.ttl);
  ESP_LOGCONFIG(TAG,"  Own system=%s port=%s",identity_.name.c_str(),identity_.port.c_str());
}
bool LLDPComponent::receive(void *context,const uint8_t *data,size_t length){
  if(!protocol::frame(data,length))return false;
  auto *p=static_cast<LLDPComponent *>(context);
  if(!p->enabled_.load()||!(p->mode_.load()&1))return true;
  portENTER_CRITICAL(&p->mux_);
  if(length>1514||p->count_==2){p->queue_drops_++;portEXIT_CRITICAL(&p->mux_);return true;}
  auto &slot=p->queue_[(p->head_+p->count_)%2];slot.length=length;memcpy(slot.bytes,data,length);p->count_++;
  portEXIT_CRITICAL(&p->mux_);return true;
}
bool LLDPComponent::send_(bool shutdown){
  if(!ethernet_->is_l2_ready())return false;
  uint8_t mac[6]{};ethernet_->get_eth_mac_address_raw(mac);std::vector<uint8_t> wire;
  if(!protocol::encode(identity_,mac,wire,shutdown)){tx_rejected_++;return false;}
  esp_err_t e=ethernet_->transmit_l2(wire.data(),wire.size());
  if(e!=ESP_OK){tx_rejected_++;return false;}
  transmitted_++;ESP_LOGD(TAG,"LLDP%s queued: %u bytes",shutdown?" shutdown":"",unsigned(wire.size()));return true;
}
void LLDPComponent::clear_(){neighbor_=protocol::Neighbor{};neighbor_present_=false;}
void LLDPComponent::set_enabled(bool enabled){
  if(!enabled&&setup_done_&&enabled_.load()&&(mode_.load()&2)&&ethernet_)send_(true);
  enabled_.store(enabled);force_tx_=true;
  if(!enabled){clear_();expired_=false;portENTER_CRITICAL(&mux_);head_=count_=0;portEXIT_CRITICAL(&mux_);}
  if(switch_)switch_->publish_state(enabled);publish_();
}
void LLDPComponent::on_shutdown(){if(setup_done_&&enabled_.load()&&(mode_.load()&2))send_(true);}
void LLDPComponent::loop(){
  const uint32_t now=millis();const bool link=ethernet_->is_l2_ready();
  if(!link&&was_link_){clear_();expired_=false;}
  if(link&&!was_link_)force_tx_=true;was_link_=link;
  for(unsigned i=0;i<2;i++){
    Packet packet;bool available=false;portENTER_CRITICAL(&mux_);
    if(count_){packet=queue_[head_];head_=(head_+1)%2;count_--;available=true;}
    portEXIT_CRITICAL(&mux_);if(!available)break;
    if(!enabled_.load()||!(mode_.load()&1)||!link)continue;
    uint8_t own[6]{};ethernet_->get_eth_mac_address_raw(own);if(!memcmp(packet.bytes+6,own,6))continue;
#ifdef USE_RTL_FAULT_INJECTION
    last_wire_=packet;
#endif
    protocol::Neighbor peer;if(!protocol::decode(packet.bytes,packet.length,peer)){invalid_++;continue;}
    received_++;
    if(!peer.ttl){if(neighbor_present_&&neighbor_.same(peer)){clear_();expired_=false;}continue;}
    neighbor_=std::move(peer);neighbor_present_=true;expired_=false;last_seen_=now;
  }
  if(neighbor_present_&&protocol::expired(now,last_seen_,neighbor_.ttl)){clear_();expired_=true;}
  if(enabled_.load()&&(mode_.load()&2)&&link&&((force_tx_&&uint32_t(now-last_tx_)>=1000)||uint32_t(now-last_tx_)>=interval_)){
    last_tx_=now;force_tx_=!send_();
  }
  if(uint32_t(now-last_publish_)>=1000){last_publish_=now;publish_();}
}
void LLDPComponent::publish_(){
  const std::string *values[6]={&neighbor_.name,&neighbor_.chassis,&neighbor_.port,&neighbor_.port_description,&neighbor_.description,&neighbor_.management};
  for(unsigned i=0;i<6;i++)if(sensors_[i]&&(!sensors_[i]->has_state()||sensors_[i]->state!=*values[i]))sensors_[i]->publish_state(*values[i]);
  const char *status=!enabled_.load()?"disabled":(!ethernet_||!ethernet_->is_l2_ready()?"link_down":(neighbor_present_?"neighbor_present":(expired_?"expired":"waiting")));
  const auto mapping=unifi_port(neighbor_,unifi_rules_);
  const std::string extra[3]={neighbor_present_?protocol::hex(neighbor_.source_mac.data(),6):"",neighbor_present_&&mapping.port?std::to_string(mapping.port):"",neighbor_present_?mapping.status:"no_neighbor"};
  for(unsigned i=0;i<3;i++)if(sensors_[8+i]&&(!sensors_[8+i]->has_state()||sensors_[8+i]->state!=extra[i]))sensors_[8+i]->publish_state(extra[i]);
  if(sensors_[6]&&(!sensors_[6]->has_state()||sensors_[6]->state!=status))sensors_[6]->publish_state(status);
  if(sensors_[7]){char b[150];snprintf(b,sizeof(b),"rx=%u tx_queued=%u invalid=%u queue_drop=%u tx_rejected=%u ttl_remaining=%u",unsigned(received_),unsigned(transmitted_),unsigned(invalid_),unsigned(queue_drops_.load()),unsigned(tx_rejected_),neighbor_present_?unsigned(neighbor_.ttl-std::min<uint32_t>(neighbor_.ttl,uint32_t(millis()-last_seen_)/1000)):0);if(!sensors_[7]->has_state()||sensors_[7]->state!=b)sensors_[7]->publish_state(b);}
}
#ifdef USE_RTL_FAULT_INJECTION
std::string LLDPComponent::last_wire_hex() const {
  std::string out;out.reserve(last_wire_.length*2);char b[3];
  for(unsigned i=0;i<last_wire_.length;i++){snprintf(b,sizeof(b),"%02X",last_wire_.bytes[i]);out+=b;}
  return out;
}
#endif

}
