#include "game_runtime.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace zumbi {
namespace {
bool truthy(const ZsValue& value) {
    if(const auto p=std::get_if<bool>(&value)) return *p;
    if(const auto p=std::get_if<double>(&value)) return *p!=0.0;
    if(const auto p=std::get_if<std::string>(&value)) return !p->empty();
    return !std::holds_alternative<std::monostate>(value);
}
std::string valueText(const ZsValue& value) {
    if(const auto p=std::get_if<std::string>(&value)) return *p;
    if(const auto p=std::get_if<double>(&value)) return std::to_string(*p);
    if(const auto p=std::get_if<bool>(&value)) return *p?"true":"false";
    return "null";
}
}
NativeGameRuntime::NativeGameRuntime() : random_(0x5a42524u) {
    input_["cameraYaw"]=0.0;
    input_["deltaTime"]=1.0/60.0;
    input_["MoveForward"]=false;
    input_["MoveBackward"]=false;
    input_["MoveLeft"]=false;
    input_["MoveRight"]=false;
    input_["Sprint"]=false;
    input_["Crouch"]=false;
    input_["Jump"]=false;
    input_["grounded"]=true;
    input_["ceiling"]=false;
}
void NativeGameRuntime::setInput(const std::string& name,ZsValue value) { input_[name]=std::move(value); }
void NativeGameRuntime::setGameActive(bool active) { gameActive_=active; }
const UiNodeState* NativeGameRuntime::uiNode(const std::string& id) const {
    const auto it=ui_.find(id);
    return it==ui_.end()?nullptr:&it->second;
}
std::size_t NativeGameRuntime::playerCount() const { return players_.size(); }
std::size_t NativeGameRuntime::eventCount() const { return events_.pending(); }
bool NativeGameRuntime::failed() const { return failed_; }
const std::string& NativeGameRuntime::failure() const { return failure_; }
void NativeGameRuntime::fail(const std::string& message) { failed_=true; failure_=message; }
bool NativeGameRuntime::number(const std::vector<ZsValue>& args,std::size_t i,double& value) const {
    if(i>=args.size()) return false;
    if(const auto p=std::get_if<double>(&args[i])) { value=*p; return std::isfinite(value); }
    return false;
}
bool NativeGameRuntime::string(const std::vector<ZsValue>& args,std::size_t i,std::string& value) const {
    if(i>=args.size()) return false;
    if(const auto p=std::get_if<std::string>(&args[i])) { value=*p; return true; }
    return false;
}
bool NativeGameRuntime::entity(const std::vector<ZsValue>& args,std::size_t i,Entity& value) const {
    double raw=0;
    if(!number(args,i,raw)||raw<1||raw>4294967295.0||std::floor(raw)!=raw) return false;
    value=static_cast<Entity>(raw);
    return world_.alive(value)&&players_.contains(value);
}
ZsValue NativeGameRuntime::invoke(const std::string& name,const std::vector<ZsValue>& args) {
    auto n=[&](std::size_t i,double& v){return number(args,i,v);};
    auto s=[&](std::size_t i,std::string& v){return string(args,i,v);};
    if(name=="log"||name=="print") {
        for(std::size_t i=0;i<args.size();++i) { if(i) std::cout<<' '; std::cout<<valueText(args[i]); }
        std::cout<<'\n'; return std::monostate{};
    }
    if(name=="player.create"&&args.empty()) {
        const Entity id=world_.createEntity();
        auto& p=players_[id];
        p={{"health",100.0},{"maxHealth",100.0},{"armor",100.0},{"attackDamage",10.0},
           {"stamina",100.0},{"maxStamina",100.0},{"minStamina",10.0},
           {"currentNoise",0.0},{"smelliness",0.0},{"dead",false},{"bleeding",false},
           {"dummy",false},{"immortal",false},{"tireless",false},{"silent",false},
           {"unbleedable",false},{"movementState",0.0},{"verticalVelocity",0.0},
           {"walkingSpeed",10.0},{"sprintingSpeed",16.0},{"crouchingSpeed",4.0},
           {"standingHeight",2.0},{"crouchingHeight",1.0},{"currentHeight",2.0},
           {"jumpHeight",1.2},{"currentSpeed",10.0},{"bleedingDuration",0.0},
           {"nextBleed",0.0},{"averageBleedingDuration",60.0},{"bleedingVariation",10.0},
           {"bleedingDamage",2.0},{"bleedingRate",0.7},{"noiseReduction",2.0},
           {"standingNoise",0.0},{"walkingNoise",5.0},{"sprintingNoise",15.0},
           {"crouchingNoise",1.0},{"jumpingNoise",10.0},{"standingStaminaGain",5.0},
           {"walkingStaminaGain",2.0},{"sprintingStaminaGain",-8.0},{"crouchingStaminaGain",4.0},
           {"aerialStaminaGain",-2.0},{"jumpingStaminaCost",10.0}};
        return static_cast<double>(id);
    }
    if(name=="player.destroy"&&args.size()==1) {
        Entity id{}; if(!entity(args,0,id)) return false;
        players_.erase(id); return world_.destroyEntity(id);
    }
    if((name=="player.get"&&args.size()==2)||(name=="player.set"&&args.size()==3)) {
        Entity id{}; std::string key;
        if(!entity(args,0,id)||!s(1,key)) return std::monostate{};
        auto& p=players_[id];
        if(name=="player.get") { const auto it=p.find(key); return it==p.end()?ZsValue(std::monostate{}):it->second; }
        p[key]=args[2]; return args[2];
    }
    if((name=="input.get"||name=="input.consume")&&args.size()==1) {
        std::string key; if(!s(0,key)) return false;
        const auto it=input_.find(key);
        if(it==input_.end()) return false;
        const ZsValue value=it->second;
        if(name=="input.consume") it->second=false;
        return value;
    }
    if(name=="game.active"&&args.empty()) return gameActive_;
    if(name=="time.delta"&&args.empty()) {
        const auto it=input_.find("deltaTime");
        return it==input_.end()?ZsValue(1.0/60.0):it->second;
    }
    if(name=="state.set"&&args.size()==2) {
        std::string key; if(!s(0,key)) return false;
        state_[key]=args[1]; return args[1];
    }
    if((name=="state.get"||name=="state.has"||name=="state.erase")&&args.size()==1) {
        std::string key; if(!s(0,key)) return std::monostate{};
        const auto it=state_.find(key);
        if(name=="state.has") return it!=state_.end();
        if(it==state_.end()) return std::monostate{};
        if(name=="state.get") return it->second;
        state_.erase(it); return true;
    }
    if(name=="physics.grounded"&&args.size()==1) {
        Entity id{}; if(!entity(args,0,id)) return false;
        const auto it=input_.find("grounded"); return it!=input_.end()&&truthy(it->second);
    }
    if(name=="physics.ceiling"&&args.size()==1) {
        Entity id{}; if(!entity(args,0,id)) return false;
        const auto it=input_.find("ceiling"); return it!=input_.end()&&truthy(it->second);
    }
    if(name=="physics.gravity_y"&&args.empty()) return -9.81;
    if(name=="camera.yaw"&&args.empty()) {
        const auto it=input_.find("cameraYaw"); return it==input_.end()?ZsValue(0.0):it->second;
    }
    if(name=="physics.move"&&args.size()==4) {
        Entity id{}; double x{},y{},z{};
        if(!entity(args,0,id)||!n(1,x)||!n(2,y)||!n(3,z)) return false;
        auto transform=world_.getTransform(id)?*world_.getTransform(id):Transform{};
        transform.position.x+=x; transform.position.y+=y; transform.position.z+=z;
        world_.setTransform(id,transform); return true;
    }
    if(name=="player.position"&&args.size()==2) {
        Entity id{}; std::string axis;
        if(!entity(args,0,id)||!s(1,axis)) return std::monostate{};
        const auto* t=world_.getTransform(id); if(!t) return std::monostate{};
        if(axis=="x") return t->position.x; if(axis=="y") return t->position.y; if(axis=="z") return t->position.z;
        return std::monostate{};
    }
    if(name=="math.sqrt"&&args.size()==1) { double v{}; return n(0,v)&&v>=0?ZsValue(std::sqrt(v)):ZsValue(0.0); }
    if(name=="math.sin"&&args.size()==1) { double v{}; return n(0,v)?ZsValue(std::sin(v)):ZsValue(0.0); }
    if(name=="math.cos"&&args.size()==1) { double v{}; return n(0,v)?ZsValue(std::cos(v)):ZsValue(1.0); }
    if(name=="random.value"&&args.empty()) { std::uniform_real_distribution<double> d(0.0,1.0); return d(random_); }
    if(name=="random.range"&&args.size()==2) {
        double a{},b{}; if(!n(0,a)||!n(1,b)) return 0.0;
        std::uniform_real_distribution<double> d(std::min(a,b),std::max(a,b)); return d(random_);
    }
    if(name=="ui.create"&&args.size()==7) {
        std::string id,kind,parent; double x{},y{},width{},height{};
        if(!s(0,id)||!s(1,kind)||!s(2,parent)||!n(3,x)||!n(4,y)||!n(5,width)||!n(6,height)||id.empty()) return false;
        auto& node=ui_[id]; node.kind=kind; node.parent=parent; node.x=x; node.y=y;
        node.width=width; node.height=height; return true;
    }
    if(name=="ui.action"&&args.size()==3) {
        std::string id,action; double argument{};
        if(!s(0,id)||!s(1,action)||!n(2,argument)) return false;
        ui_[id].action=action; ui_[id].actionArgument=argument; return true;
    }
    if(name=="ui.click"&&args.size()==1) {
        std::string id; if(!s(0,id)) return false;
        const auto it=ui_.find(id); if(it==ui_.end()||!it->second.enabled||!it->second.visible) return false;
        input_["ui.clicked."+id]=true;
        events_.publish({"ui.click",0,{it->second.actionArgument}});
        return true;
    }
    if(name=="ui.get_text"&&args.size()==1) {
        std::string id; if(!s(0,id)) return std::string{};
        const auto it=ui_.find(id); return it==ui_.end()?ZsValue(std::string{}):ZsValue(it->second.text);
    }
    if((name=="ui.text"||name=="ui.visible"||name=="ui.enabled"||name=="ui.alpha")&&args.size()==2) {
        std::string id; if(!s(0,id)) return false;
        auto& node=ui_[id];
        if(name=="ui.text") { std::string value; if(!s(1,value)) return false; node.text=value; }
        else if(name=="ui.visible") node.visible=truthy(args[1]);
        else if(name=="ui.enabled") node.enabled=truthy(args[1]);
        else { double value{}; if(!n(1,value)) return false; node.alpha=std::clamp(value,0.0,1.0); }
        return true;
    }
    if(name=="ui.get_visible"&&args.size()==1) {
        std::string id; if(!s(0,id)) return false;
        const auto it=ui_.find(id); return it!=ui_.end()&&it->second.visible;
    }
    if(name=="ui.get_value"&&args.size()==1) {
        std::string id; if(!s(0,id)) return 0.0;
        const auto it=ui_.find(id); return it==ui_.end()?ZsValue(0.0):ZsValue(it->second.value);
    }
    if(name=="ui.value"&&args.size()==3) {
        std::string id; double value{},maximum{};
        if(!s(0,id)||!n(1,value)||!n(2,maximum)) return false;
        auto& node=ui_[id]; node.maximum=std::max(0.0,maximum); node.value=std::clamp(value,0.0,node.maximum); return true;
    }
    if(name=="event.emit"&&args.size()>=1&&args.size()<=3) {
        std::string eventName; if(!s(0,eventName)) return false;
        Entity source=0; if(args.size()>=2) { double raw{}; if(n(1,raw)&&raw>0) source=static_cast<Entity>(raw); }
        std::vector<double> values; if(args.size()==3) { double v{}; if(n(2,v)) values.push_back(v); }
        events_.publish({eventName,source,std::move(values)}); return true;
    }
    if(name=="audio.play"&&args.size()==1) {
        std::string clip; if(!s(0,clip)) return false; events_.publish({"audio.play",0,{}}); ui_["audio.last_clip"].text=clip; return true;
    }
    if(name=="particles.set"&&args.size()==2) {
        std::string id; if(!s(0,id)) return false; ui_["particle."+id].visible=truthy(args[1]); return true;
    }
    if(name=="inventory.death_drop"&&args.size()==1) {
        Entity id{}; if(!entity(args,0,id)) return false; events_.publish({"inventory.death_drop",id,{}}); return true;
    }
    if(name=="player_manager.death"&&args.size()==1) {
        Entity id{}; if(!entity(args,0,id)) return false; events_.publish({"player.death",id,{}}); return true;
    }
    if(name=="test.expect"&&args.size()==2) {
        std::string message;
        if(!s(1,message)) message="ZS runtime assertion failed";
        if(!truthy(args[0])) fail(message);
        return truthy(args[0]);
    }
    fail("Unregistered or invalid native API call: "+name);
    return std::monostate{};
}
bool NativeGameRuntime::run(const std::filesystem::path& bytecode,std::string& error) {
    failed_=false; failure_.clear();
    const ZsNativeCall native=[this](const std::string& name,const std::vector<ZsValue>& args){return invoke(name,args);};
    if(!ZsVm::run(bytecode,native,error)) return false;
    events_.dispatch();
    if(failed_) { error=failure_; return false; }
    return true;
}
}
