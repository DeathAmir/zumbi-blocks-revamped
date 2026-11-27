#include "game_runtime.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <cstdlib>

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
    if(const auto p=std::get_if<double>(&value)) { std::ostringstream out; out<<*p; return out.str(); }
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
    state_["options.volume"]=1.0;
    state_["options.fullscreen"]=0.0;
    state_["options.resolution"]=0.0;
    state_["options.quality"]=2.0;
    state_["options.mouseSensitivity"]=6.0;
    events_.subscribe("game.pause",[this](const EngineEvent&){gameActive_=false;state_["game.paused"]=true;});
    events_.subscribe("game.resume",[this](const EngineEvent&){gameActive_=true;state_["game.paused"]=false;});
    events_.subscribe("game.quit",[this](const EngineEvent&){state_["game.quit"]=true;});
    events_.subscribe("game.over",[this](const EngineEvent&){state_["game.over"]=true;gameActive_=false;});
    events_.subscribe("scene.change",[this](const EngineEvent& event){
        if(!event.values.empty()) state_["scene.index"]=event.values[0];
        gameActive_=!event.values.empty()&&event.values[0]>0.0;
    });
    events_.subscribe("options.fullscreen.toggle",[this](const EngineEvent&){
        const auto it=state_.find("options.fullscreen");
        const double current=it!=state_.end()&&truthy(it->second)?1.0:0.0;
        state_["options.fullscreen"]=current==0.0?1.0:0.0;
    });
    events_.subscribe("options.fullscreen.set",[this](const EngineEvent& event){
        if(!event.values.empty()) state_["options.fullscreen"]=event.values[0]!=0.0?1.0:0.0;
    });
    events_.subscribe("options.save",[this](const EngineEvent&){
        std::string error;
        if(saveOptions(error)) state_["options.saved"]=true;
        else { state_["options.saved"]=false; state_["options.error"]=error; }
    });
    loadOptions();
    events_.subscribe("player.death",[this](const EngineEvent&){});
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
std::string NativeGameRuntime::settingsPath() const {
    const char* configured=std::getenv("ZBR_SETTINGS_PATH");
    if(configured&&*configured) return configured;
    const auto current=std::filesystem::current_path();
    if(current.filename()=="Bin"&&current.parent_path().filename()=="Game")
        return (current.parent_path()/"Config"/"settings.ini").string();
    if(current.filename()=="Game")
        return (current/"Config"/"settings.ini").string();
    return (current/"Game"/"Config"/"settings.ini").string();
}
void NativeGameRuntime::loadOptions() {
    std::ifstream file(settingsPath());
    if(!file) return;
    std::string line;
    while(std::getline(file,line)) {
        const auto separator=line.find('=');
        if(separator==std::string::npos) continue;
        const auto key=line.substr(0,separator);
        if(key!="volume"&&key!="fullscreen"&&key!="resolution"&&key!="quality"&&key!="mouseSensitivity") continue;
        try {
            std::size_t consumed=0;
            double value=std::stod(line.substr(separator+1),&consumed);
            if(consumed!=line.size()-separator-1||!std::isfinite(value)) continue;
            if(key=="volume") value=std::clamp(value,0.0,1.0);
            else if(key=="fullscreen") value=value!=0.0?1.0:0.0;
            else if(key=="resolution") value=std::clamp(value,0.0,10.0);
            else if(key=="quality") value=std::clamp(value,0.0,3.0);
            else if(key=="mouseSensitivity") value=std::clamp(value,1.0,20.0);
            state_["options."+key]=value;
        } catch(...) {}
    }
}
bool NativeGameRuntime::saveOptions(std::string& error) const {
    const std::filesystem::path path(settingsPath());
    std::error_code ec;
    if(!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path(),ec);
    if(ec) { error="Could not create options directory: "+ec.message(); return false; }
    auto temporary=path;
    temporary += ".tmp";
    std::ofstream file(temporary,std::ios::binary|std::ios::trunc);
    if(!file) { error="Could not open options file for writing."; return false; }
    const std::vector<std::string> keys={"volume","fullscreen","resolution","quality","mouseSensitivity"};
    for(const auto& key:keys) {
        const auto found=state_.find("options."+key);
        if(found==state_.end()) continue;
        double value=0.0;
        if(const auto number=std::get_if<double>(&found->second)) value=*number;
        else if(const auto boolean=std::get_if<bool>(&found->second)) value=*boolean?1.0:0.0;
        else continue;
        file<<key<<'='<<std::setprecision(17)<<value<<'\n';
    }
    file.flush();
    if(!file) { error="Could not flush options file."; file.close(); std::filesystem::remove(temporary,ec); return false; }
    file.close();
    std::filesystem::rename(temporary,path,ec);
    if(ec) {
        std::filesystem::remove(path,ec);
        ec.clear();
        std::filesystem::rename(temporary,path,ec);
    }
    if(ec) { error="Could not replace options file: "+ec.message(); std::filesystem::remove(temporary,ec); return false; }
    return true;
}
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
    auto arrayArg=[&](std::size_t i)->std::shared_ptr<ZsArray>{
        if(i>=args.size()) return {};
        if(const auto p=std::get_if<std::shared_ptr<ZsArray>>(&args[i])) return *p;
        return {};
    };
    auto mapArg=[&](std::size_t i)->std::shared_ptr<ZsMap>{
        if(i>=args.size()) return {};
        if(const auto p=std::get_if<std::shared_ptr<ZsMap>>(&args[i])) return *p;
        return {};
    };
    if(name=="array.new"&&args.empty()) return std::make_shared<ZsArray>();
    if(name=="array.push"&&args.size()==2) {
        auto target=arrayArg(0); if(!target) return false;
        target->values.push_back(args[1]); return static_cast<double>(target->values.size());
    }
    if(name=="array.get"&&args.size()==2) {
        auto target=arrayArg(0); double index{};
        if(!target||!n(1,index)||index<0||std::floor(index)!=index||index>=static_cast<double>(target->values.size())) return std::monostate{};
        return target->values[static_cast<std::size_t>(index)];
    }
    if(name=="array.set"&&args.size()==3) {
        auto target=arrayArg(0); double index{};
        if(!target||!n(1,index)||index<0||std::floor(index)!=index||index>=static_cast<double>(target->values.size())) return false;
        target->values[static_cast<std::size_t>(index)]=args[2]; return true;
    }
    if(name=="array.size"&&args.size()==1) {
        auto target=arrayArg(0); return target?ZsValue(static_cast<double>(target->values.size())):ZsValue(0.0);
    }
    if(name=="array.pop"&&args.size()==1) {
        auto target=arrayArg(0); if(!target||target->values.empty()) return std::monostate{};
        auto value=target->values.back(); target->values.pop_back(); return value;
    }
    if(name=="map.new"&&args.empty()) return std::make_shared<ZsMap>();
    if(name=="map.set"&&args.size()==3) {
        auto target=mapArg(0); std::string key;
        if(!target||!s(1,key)) return false;
        target->values[key]=args[2]; return args[2];
    }
    if((name=="map.get"||name=="map.has"||name=="map.erase")&&args.size()==2) {
        auto target=mapArg(0); std::string key;
        if(!target||!s(1,key)) return std::monostate{};
        auto found=target->values.find(key);
        if(name=="map.has") return found!=target->values.end();
        if(found==target->values.end()) return std::monostate{};
        if(name=="map.get") return found->second;
        target->values.erase(found); return true;
    }
    if(name=="input.set"&&args.size()==2) {
        std::string key; if(!s(0,key)||key.empty()) return false;
        input_[key]=args[1]; return true;
    }
    if(name=="log"||name=="print") {
        for(std::size_t i=0;i<args.size();++i) { if(i) std::cout<<' '; std::cout<<valueText(args[i]); }
        std::cout<<'\n'; return std::monostate{};
    }
    if(name=="player.create"&&args.empty()) {
        const Entity id=world_.createEntity();
        auto& p=players_[id];
        playerOrder_.push_back(id);
        p={{"health",100.0},{"maxHealth",100.0},{"armor",100.0},{"attackDamage",10.0},
           {"stamina",40.0},{"maxStamina",40.0},{"minStamina",8.0},
           {"currentNoise",0.0},{"smelliness",0.0},{"dead",false},{"bleeding",false},
           {"dummy",false},{"immortal",false},{"tireless",false},{"silent",false},
           {"unbleedable",false},{"movementState",0.0},{"verticalVelocity",0.0},
           {"walkingSpeed",6.0},{"sprintingSpeed",10.0},{"crouchingSpeed",2.0},
           {"standingHeight",2.4},{"crouchingHeight",1.2},{"currentHeight",2.0},
           {"jumpHeight",1.5},{"currentSpeed",6.0},{"bleedingDuration",0.0},
           {"nextBleed",0.0},{"averageBleedingDuration",60.0},{"bleedingVariation",10.0},
           {"bleedingDamage",1.0},{"bleedingRate",0.5},{"noiseReduction",2.0},
           {"standingNoise",0.0},{"walkingNoise",400.0},{"sprintingNoise",800.0},
           {"crouchingNoise",150.0},{"jumpingNoise",800.0},{"standingStaminaGain",6.0},
           {"walkingStaminaGain",4.0},{"sprintingStaminaGain",-7.0},{"crouchingStaminaGain",5.0},
           {"aerialStaminaGain",2.0},{"jumpingStaminaCost",10.0}};
        return static_cast<double>(id);
    }
    if(name=="player.destroy"&&args.size()==1) {
        Entity id{}; if(!entity(args,0,id)) return false;
        players_.erase(id);
        inventories_.erase(id);
        playerOrder_.erase(std::remove(playerOrder_.begin(),playerOrder_.end(),id),playerOrder_.end());
        return world_.destroyEntity(id);
    }
    if((name=="player.get"&&args.size()==2)||(name=="player.set"&&args.size()==3)) {
        Entity id{}; std::string key;
        if(!entity(args,0,id)||!s(1,key)) return std::monostate{};
        auto& p=players_[id];
        if(name=="player.get") { const auto it=p.find(key); return it==p.end()?ZsValue(std::monostate{}):it->second; }
        p[key]=args[2]; return args[2];
    }
    if(name=="inventory.init"&&args.size()==4) {
        Entity id{}; double primary{},secondary{},melee{};
        if(!entity(args,0,id)||!n(1,primary)||!n(2,secondary)||!n(3,melee)) return false;
        if(primary<1||primary>9||secondary<1||secondary>9||melee<1||melee>9) return false;
        InventoryRuntimeState inv;
        inv.primarySlot=static_cast<int>(primary);
        inv.secondarySlot=static_cast<int>(secondary);
        inv.meleeSlot=static_cast<int>(melee);
        inventories_[id]=std::move(inv);
        return true;
    }
    if(name=="item.create"&&args.size()==9) {
        double type{},definitionId{},amount{},capacity{},ammo{},totalAmmo{};
        std::string itemName,pickupSound;
        if(!n(0,type)||!n(1,definitionId)||!n(2,amount)||!n(4,capacity)||!n(5,ammo)||!n(6,totalAmmo)||
           !s(7,itemName)||!s(8,pickupSound)||type<0||type>5||std::floor(type)!=type||
           definitionId<0||amount<1||std::floor(amount)!=amount||nextItemId_==0) return 0.0;
        ItemRuntimeState item;
        item.id=nextItemId_++;
        item.type=static_cast<int>(type);
        item.definitionId=static_cast<int>(definitionId);
        item.amount=static_cast<int>(amount);
        item.handgun=truthy(args[3]);
        item.ammoBoxCapacity=capacity;
        item.ammo=ammo;
        item.totalAmmo=totalAmmo;
        item.name=itemName;
        item.pickupSound=pickupSound;
        items_[item.id]=item;
        return static_cast<double>(item.id);
    }
    if(name=="item.destroy"&&args.size()==1) {
        double raw{}; if(!n(0,raw)||raw<1||raw>4294967295.0) return false;
        const auto it=items_.find(static_cast<std::uint32_t>(raw));
        if(it==items_.end()) return false;
        it->second.alive=false; it->second.active=false; it->second.equipped=false;
        return true;
    }
    if((name=="item.get"&&args.size()==2)||(name=="item.set"&&args.size()==3)) {
        double raw{}; std::string field;
        if(!n(0,raw)||raw<1||raw>4294967295.0||!s(1,field)) return std::monostate{};
        const auto it=items_.find(static_cast<std::uint32_t>(raw));
        if(it==items_.end()) return std::monostate{};
        auto& item=it->second;
        if(name=="item.get") {
            if(field=="type") return static_cast<double>(item.type);
            if(field=="definitionId") return static_cast<double>(item.definitionId);
            if(field=="amount") return static_cast<double>(item.amount);
            if(field=="handgun") return item.handgun;
            if(field=="ammoBoxCapacity") return item.ammoBoxCapacity;
            if(field=="ammo") return item.ammo;
            if(field=="totalAmmo") return item.totalAmmo;
            if(field=="name") return item.name;
            if(field=="pickupSound") return item.pickupSound;
            if(field=="alive") return item.alive;
            if(field=="active") return item.active;
            if(field=="uiActive") return item.uiActive;
            if(field=="colliderEnabled") return item.colliderEnabled;
            if(field=="physicsEnabled") return item.physicsEnabled;
            if(field=="equipped") return item.equipped;
            return std::monostate{};
        }
        const auto& value=args[2];
        if(field=="type") { if(const auto v=std::get_if<double>(&value)) item.type=static_cast<int>(*v); else return false; }
        else if(field=="definitionId") { if(const auto v=std::get_if<double>(&value)) item.definitionId=static_cast<int>(*v); else return false; }
        else if(field=="amount") { if(const auto v=std::get_if<double>(&value)) item.amount=static_cast<int>(*v); else return false; }
        else if(field=="handgun") item.handgun=truthy(value);
        else if(field=="ammoBoxCapacity") { if(const auto v=std::get_if<double>(&value)) item.ammoBoxCapacity=*v; else return false; }
        else if(field=="ammo") { if(const auto v=std::get_if<double>(&value)) item.ammo=*v; else return false; }
        else if(field=="totalAmmo") { if(const auto v=std::get_if<double>(&value)) item.totalAmmo=*v; else return false; }
        else if(field=="name") { if(const auto v=std::get_if<std::string>(&value)) item.name=*v; else return false; }
        else if(field=="pickupSound") { if(const auto v=std::get_if<std::string>(&value)) item.pickupSound=*v; else return false; }
        else if(field=="alive") item.alive=truthy(value);
        else if(field=="active") item.active=truthy(value);
        else if(field=="uiActive") item.uiActive=truthy(value);
        else if(field=="colliderEnabled") item.colliderEnabled=truthy(value);
        else if(field=="physicsEnabled") item.physicsEnabled=truthy(value);
        else if(field=="equipped") item.equipped=truthy(value);
        else return false;
        return value;
    }
    if((name=="inventory.get"&&args.size()==2)||(name=="inventory.set"&&args.size()==3)) {
        Entity id{}; double slot{};
        if(!entity(args,0,id)||!n(1,slot)||slot<1||slot>9||std::floor(slot)!=slot||!inventories_.contains(id)) return 0.0;
        auto& inv=inventories_[id];
        const auto index=static_cast<std::size_t>(slot-1);
        if(name=="inventory.get") return static_cast<double>(inv.slots[index]);
        double raw{}; if(!n(2,raw)||raw<0||raw>4294967295.0||std::floor(raw)!=raw) return false;
        const auto itemId=static_cast<std::uint32_t>(raw);
        if(itemId&&(!items_.contains(itemId)||!items_[itemId].alive)) return false;
        inv.slots[index]=itemId;
        return true;
    }
    if(name=="inventory.selected"&&args.size()==1) {
        Entity id{}; if(!entity(args,0,id)||!inventories_.contains(id)) return 0.0;
        return static_cast<double>(inventories_[id].selectedSlot);
    }
    if(name=="inventory.select"&&args.size()==2) {
        Entity id{}; double slot{};
        if(!entity(args,0,id)||!n(1,slot)||slot<1||slot>9||std::floor(slot)!=slot||!inventories_.contains(id)) return false;
        inventories_[id].selectedSlot=static_cast<int>(slot); return true;
    }
    if((name=="inventory.primary"||name=="inventory.secondary"||name=="inventory.melee")&&args.size()==1) {
        Entity id{}; if(!entity(args,0,id)||!inventories_.contains(id)) return 0.0;
        const auto& inv=inventories_[id];
        if(name=="inventory.primary") return static_cast<double>(inv.primarySlot);
        if(name=="inventory.secondary") return static_cast<double>(inv.secondarySlot);
        return static_cast<double>(inv.meleeSlot);
    }
    if(name=="inventory.has_gun"&&args.size()==1) {
        Entity id{}; if(!entity(args,0,id)||!inventories_.contains(id)) return false;
        for(const auto itemId:inventories_[id].slots) {
            const auto it=items_.find(itemId);
            if(itemId&&it!=items_.end()&&it->second.alive&&it->second.type==0) return true;
        }
        return false;
    }
    if(name=="inventory.target_item"&&args.empty()) {
        const auto it=input_.find("targetItem");
        return it==input_.end()?ZsValue(0.0):it->second;
    }
    if((name=="input.get"||name=="input.consume"||name=="input.key_down"||name=="input.key_pressed")&&args.size()==1) {
        std::string key; if(!s(0,key)) return false;
        const std::string lookup=(name=="input.key_down"||name=="input.key_pressed")?"key."+key:key;
        const auto it=input_.find(lookup);
        if(it==input_.end()) return false;
        const ZsValue value=it->second;
        if(name=="input.consume"||name=="input.key_pressed") it->second=false;
        return value;
    }
    if(name=="array.new"&&args.empty()) return std::make_shared<ZsArray>();
    if(name=="array.push"&&args.size()==2) {
        const auto array=std::get_if<std::shared_ptr<ZsArray>>(&args[0]);
        if(!array||!*array) return false;
        (*array)->values.push_back(args[1]);
        return static_cast<double>((*array)->values.size());
    }
    if(name=="array.get"&&args.size()==2) {
        const auto array=std::get_if<std::shared_ptr<ZsArray>>(&args[0]);
        double index{}; if(!array||!*array||!n(1,index)||index<0||index>=static_cast<double>((*array)->values.size())||std::floor(index)!=index) return std::monostate{};
        return (*array)->values[static_cast<std::size_t>(index)];
    }
    if(name=="array.set"&&args.size()==3) {
        const auto array=std::get_if<std::shared_ptr<ZsArray>>(&args[0]);
        double index{}; if(!array||!*array||!n(1,index)||index<0||index>=static_cast<double>((*array)->values.size())||std::floor(index)!=index) return false;
        (*array)->values[static_cast<std::size_t>(index)]=args[2]; return true;
    }
    if(name=="array.size"&&args.size()==1) {
        const auto array=std::get_if<std::shared_ptr<ZsArray>>(&args[0]);
        return array&&*array?ZsValue(static_cast<double>((*array)->values.size())):ZsValue(0.0);
    }
    if(name=="array.pop"&&args.size()==1) {
        const auto array=std::get_if<std::shared_ptr<ZsArray>>(&args[0]);
        if(!array||!*array||(*array)->values.empty()) return std::monostate{};
        auto value=(*array)->values.back(); (*array)->values.pop_back(); return value;
    }
    if(name=="map.new"&&args.empty()) return std::make_shared<ZsMap>();
    if(name=="map.set"&&args.size()==3) {
        const auto map=std::get_if<std::shared_ptr<ZsMap>>(&args[0]);
        const auto key=std::get_if<std::string>(&args[1]);
        if(!map||!*map||!key) return false;
        (*map)->values[*key]=args[2]; return args[2];
    }
    if((name=="map.get"||name=="map.has"||name=="map.erase")&&args.size()==2) {
        const auto map=std::get_if<std::shared_ptr<ZsMap>>(&args[0]);
        const auto key=std::get_if<std::string>(&args[1]);
        if(!map||!*map||!key) return std::monostate{};
        const auto found=(*map)->values.find(*key);
        if(name=="map.has") return found!=(*map)->values.end();
        if(found==(*map)->values.end()) return std::monostate{};
        if(name=="map.get") return found->second;
        (*map)->values.erase(found); return true;
    }
    if(name=="controls.set"&&args.size()==2) {
        std::string action,key; if(!s(0,action)||!s(1,key)||action.empty()||key.empty()) return false;
        state_["controls."+action]=key; return true;
    }
    if(name=="controls.get"&&args.size()==1) {
        std::string action; if(!s(0,action)) return std::string{};
        const auto it=state_.find("controls."+action);
        return it==state_.end()?ZsValue(std::string{}):it->second;
    }
    if(name=="options.get"&&args.size()==1) {
        std::string key; if(!s(0,key)) return std::monostate{};
        const auto it=state_.find("options."+key);
        return it==state_.end()?ZsValue(std::monostate{}):it->second;
    }
    if(name=="options.set"&&args.size()==2) {
        std::string key; if(!s(0,key)||key.empty()) return false;
        if(const auto value=std::get_if<double>(&args[1])) {
            double adjusted=*value;
            if(key=="volume") adjusted=std::clamp(adjusted,0.0,1.0);
            else if(key=="fullscreen") adjusted=adjusted!=0.0?1.0:0.0;
            else if(key=="resolution") adjusted=std::clamp(adjusted,0.0,10.0);
            else if(key=="quality") adjusted=std::clamp(adjusted,0.0,3.0);
            else if(key=="mouseSensitivity") adjusted=std::clamp(adjusted,1.0,20.0);
            state_["options."+key]=adjusted;
        } else state_["options."+key]=args[1];
        return true;
    }
    if(name=="options.save"&&args.empty()) {
        std::string error;
        const bool saved=saveOptions(error);
        state_["options.saved"]=saved;
        if(!saved) state_["options.error"]=error;
        return saved;
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
    if(name=="format.integer"&&args.size()==1) {
        double value{}; if(!n(0,value)) return std::string{};
        return std::to_string(static_cast<long long>(value));
    }
    if(name=="format.fixed"&&args.size()==2) {
        double value{},precision{};
        if(!n(0,value)||!n(1,precision)||precision<0||precision>6) return std::string{};
        std::ostringstream out; out<<std::fixed<<std::setprecision(static_cast<int>(precision))<<value; return out.str();
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
        auto it=ui_.find(id);
        if(it==ui_.end()||!it->second.enabled) return false;
        const UiNodeState* node=&it->second;
        while(node) {
            if(!node->visible) return false;
            if(node->parent.empty()) break;
            const auto parent=ui_.find(node->parent);
            node=parent==ui_.end()?nullptr:&parent->second;
        }
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
        if(name=="ui.text") { node.text=valueText(args[1]); }
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
    if(name=="ui.change_value"&&args.size()==2) {
        std::string id; double value{};
        if(!s(0,id)||!n(1,value)) return false;
        auto& node=ui_[id];
        node.value=std::clamp(value,0.0,std::max(0.0,node.maximum));
        input_["ui.changed."+id]=true;
        return true;
    }
    if(name=="ui.node_count"&&args.empty()) return static_cast<double>(ui_.size());
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
    if(name=="player_manager.add_spawn"&&args.size()==4) {
        double x{},y{},z{},yaw{};
        if(!n(0,x)||!n(1,y)||!n(2,z)||!n(3,yaw)) return false;
        Transform spawn;
        spawn.position={x,y,z};
        spawn.rotation.y=yaw;
        spawnPoints_.push_back(spawn);
        return static_cast<double>(spawnPoints_.size());
    }
    if(name=="player_manager.clear_spawns"&&args.empty()) { spawnPoints_.clear(); return true; }
    if(name=="player_manager.spawn"&&args.size()==1) {
        Entity id{}; if(!entity(args,0,id)||spawnPoints_.empty()) return false;
        std::uniform_int_distribution<std::size_t> pick(0,spawnPoints_.size()-1);
        return world_.setTransform(id,spawnPoints_[pick(random_)]);
    }
    if(name=="player_manager.count"&&args.empty()) return static_cast<double>(playerOrder_.size());
    if(name=="player_manager.dead_count"&&args.empty()) return static_cast<double>(deadPlayers_);
    if(name=="player_manager.at"&&args.size()==1) {
        double index{}; if(!n(0,index)||index<0||std::floor(index)!=index||index>=static_cast<double>(playerOrder_.size())) return 0.0;
        return static_cast<double>(playerOrder_[static_cast<std::size_t>(index)]);
    }
    if(name=="player_manager.death"&&args.size()==1) {
        Entity id{}; if(!entity(args,0,id)) return false;
        ++deadPlayers_;
        events_.publish({"player.death",id,{}});
        if(!playerOrder_.empty()&&deadPlayers_>=playerOrder_.size()) events_.publish({"game.over",id,{}});
        return true;
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
