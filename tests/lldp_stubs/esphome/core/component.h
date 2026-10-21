#pragma once
namespace esphome {namespace setup_priority {constexpr float ETHERNET=250;}class Component {public:virtual ~Component()=default;virtual void setup(){}virtual void loop(){}virtual void dump_config(){}virtual void on_shutdown(){}virtual float get_setup_priority()const{return 0;}void mark_failed(){failed=true;}bool failed=false;};}
