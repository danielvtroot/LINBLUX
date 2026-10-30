#include "objects/Instance.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace loblox {
bool ModelFrame::valid() const {
    const auto finite=[](float v){return std::isfinite(v)&&std::fabs(v)<=1000000;};
    const float norm=rotation.x*rotation.x+rotation.y*rotation.y+rotation.z*rotation.z+rotation.w*rotation.w;
    return finite(position.x)&&finite(position.y)&&finite(position.z)&&finite(rotation.x)&&finite(rotation.y)&&finite(rotation.z)&&finite(rotation.w)&&
        std::fabs(norm-1)<.001f&&std::isfinite(scale)&&scale>=.001f&&scale<=1000;
}

const char* InstanceClassName(InstanceClass instanceClass) {
    switch (instanceClass) {
        case InstanceClass::Workspace:
            return "Workspace";
        case InstanceClass::Lighting:
            return "Lighting";
        case InstanceClass::Part:
            return "Part";
        case InstanceClass::Baseplate:
            return "Baseplate";
        case InstanceClass::SpawnLocation:
            return "SpawnLocation";
        case InstanceClass::Folder: return "Folder";
        case InstanceClass::Model: return "Model";
        case InstanceClass::Script: return "Script";
        case InstanceClass::LocalScript: return "LocalScript";
        case InstanceClass::ModuleScript: return "ModuleScript";
        case InstanceClass::ServerScriptService: return "ServerScriptService";
        case InstanceClass::ServerStorage: return "ServerStorage";
        case InstanceClass::ReplicatedStorage: return "ReplicatedStorage";
        case InstanceClass::Decal: return "Decal";
        case InstanceClass::Texture: return "Texture";
        case InstanceClass::Attachment: return "Attachment";
    }
    return "Instance";
}

bool InstanceClassFromName(const std::string& name, InstanceClass& out) {
    for (auto type : {InstanceClass::Script,InstanceClass::LocalScript,InstanceClass::ModuleScript,
        InstanceClass::ServerScriptService,InstanceClass::ServerStorage,InstanceClass::ReplicatedStorage})
        if (name == InstanceClassName(type)) { out=type; return true; }
    if (name == "Attachment") { out=InstanceClass::Attachment;return true; }
    if (name == "Decal") { out=InstanceClass::Decal;return true; }
    if (name == "Texture") { out=InstanceClass::Texture;return true; }
    if (name == "Folder") { out = InstanceClass::Folder; return true; }
    if (name == "Model") { out = InstanceClass::Model; return true; }
    if (name == "Workspace") {
        out = InstanceClass::Workspace;
        return true;
    }
    if (name == "Lighting") {
        out = InstanceClass::Lighting;
        return true;
    }
    if (name == "Part") {
        out = InstanceClass::Part;
        return true;
    }
    if (name == "Baseplate") {
        out = InstanceClass::Baseplate;
        return true;
    }
    if (name == "SpawnLocation") {
        out = InstanceClass::SpawnLocation;
        return true;
    }
    return false;
}

Instance::Instance(InstanceId id, std::string name)
    : id_(id), name_(std::move(name)) {
    if (name_.empty()) {
        name_ = "Instance";
    }
}

bool IsScriptClass(InstanceClass type) {
    return type==InstanceClass::Script || type==InstanceClass::LocalScript || type==InstanceClass::ModuleScript;
}
bool IsRootServiceClass(InstanceClass type) {
    return type==InstanceClass::Workspace || type==InstanceClass::Lighting ||
        type==InstanceClass::ServerScriptService || type==InstanceClass::ServerStorage || type==InstanceClass::ReplicatedStorage;
}

namespace {
// Unicode White_Space plus invisible separators that cannot form a useful label.
bool White(unsigned c) {
    return c<=32 || c==127 || c==0x85 || c==0xa0 || c==0x1680 ||
        (c>=0x2000&&c<=0x200b) || c==0x2028 || c==0x2029 || c==0x202f || c==0x205f || c==0x3000 || c==0xfeff;
}
bool Next(const std::string& text,size_t& i,unsigned& c) {
    const auto b=static_cast<unsigned char>(text[i++]);c=b;
    if(b<128)return true;
    const int count=b>=0xc2&&b<=0xdf?1:b>=0xe0&&b<=0xef?2:b>=0xf0&&b<=0xf4?3:-1;
    if(count<0||i+count>text.size())return false;
    c=b&((1u<<(6-count))-1);
    for(int n=0;n<count;++n){const auto d=static_cast<unsigned char>(text[i++]);if((d&0xc0)!=0x80)return false;c=(c<<6)|(d&63);}
    return c>=(count==1?0x80u:count==2?0x800u:0x10000u)&&c<=0x10ffff&&!(c>=0xd800&&c<=0xdfff);
}
}
bool ValidPartName(const std::string& name) {
    if(name.empty()||name.size()>128)return false;
    for(size_t i=0;i<name.size();){unsigned c;if(!Next(name,i,c)||White(c))return false;}
    return true;
}
std::string NewPartName(const std::string& name) {
    if(name.size()>128)return {};
    std::string result;bool capital=false;
    for(size_t i=0;i<name.size();){const auto begin=i;unsigned c;if(!Next(name,i,c))return {};
        if(c<32||c==127)return {}; // tabs/newlines are errors, never silently cleaned
        if(White(c)){capital=true;continue;}
        if(capital&&c>='a'&&c<='z')result+=char(c-'a'+'A');else result.append(name,begin,i-begin);
        capital=false;
    }
    return ValidPartName(result)?result:std::string{};
}
bool Instance::trySetName(const std::string& name,std::string& error) {
    error.clear();
    if(name==name_)return true; // unchanged legacy labels remain readable/saveable
    if(isPart()&&!ValidPartName(name)) {error="Nomes de peças não podem conter espaços. Use letras, números ou _.";return false;}
    if(name.empty()||name.size()>128){error="O nome deve conter de 1 a 128 bytes.";return false;}
    if(isPart()&&parent_)for(const auto* sibling:parent_->children())
        if(sibling!=this&&sibling->isPart()&&sibling->name()==name){error="Já existe uma peça com esse nome neste contêiner.";return false;}
    name_=name;return true;
}
void Instance::setName(const std::string& name) {
    std::string ignored;trySetName(name,ignored);
}
bool Instance::setAttribute(const std::string& name, Attribute value) {
    if (name.empty() || name.size()>64 || (!attributes_.count(name) && attributes_.size()>=64)) return false;
    if (const auto* n=std::get_if<double>(&value); n && !std::isfinite(*n)) return false;
    if (const auto* s=std::get_if<std::string>(&value); s && s->size()>4096) return false;
    attributes_[name]=std::move(value); return true;
}

bool Instance::isAncestorOf(const Instance* other) const {
    for (const Instance* node = other; node != nullptr; node = node->parent_) {
        if (node == this) {
            return true;
        }
    }
    return false;
}

void Instance::attachChild(Instance* child) {
    if (child == nullptr || child->parent_ == this) {
        return;
    }
    if (child->parent_ != nullptr) {
        child->parent_->detachChild(child);
    }
    child->parent_ = this;
    children_.push_back(child);
}

void Instance::detachChild(Instance* child) {
    const auto it = std::find(children_.begin(), children_.end(), child);
    if (it != children_.end()) {
        children_.erase(it);
    }
    if (child != nullptr && child->parent_ == this) {
        child->parent_ = nullptr;
    }
}

}  // namespace loblox
