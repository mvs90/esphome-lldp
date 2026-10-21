#pragma once
namespace esphome::switch_ {class Switch {public:virtual ~Switch()=default;bool state=false;void publish_state(bool s){state=s;}void control(bool s){write_state(s);}protected:virtual void write_state(bool)=0;};}
