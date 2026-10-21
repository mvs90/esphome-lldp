#pragma once
#include <string>
namespace esphome::text_sensor {class TextSensor {public:std::string state;bool seen=false;void publish_state(const std::string&s){state=s;seen=true;}bool has_state()const{return seen;}};}
