#pragma once
#include "scene/Scene.h"
#include <algorithm>
#include <cctype>
#include <set>
#include <unordered_map>
namespace loblox {
// Visible hierarchy snapshot. IDs remain safe after removal; presentation must
// resolve them against Scene. No DOM, graphics or selection ownership here.
class HierarchyModel {
public:
    struct Row { InstanceId id; unsigned depth; };
    void rebuild(const Scene& scene,const std::set<InstanceId>& collapsed,std::string query) {
        rows_.clear();
        auto lower=[](std::string value){for(auto& c:value)c=char(std::tolower(static_cast<unsigned char>(c)));return value;};
        query=lower(query);
        std::unordered_map<InstanceId,bool> relevant;
        std::function<bool(const Instance*)> match=[&](const Instance* node){
            bool found=query.empty()||lower(node->name()+" "+node->className()).find(query)!=std::string::npos;
            for(auto* child:node->children())found=match(child)||found;
            relevant[node->id()]=found;return found;
        };
        std::function<void(const Instance*,unsigned)> append=[&](const Instance* node,unsigned depth){
            if(!relevant[node->id()])return;
            rows_.push_back({node->id(),depth});
            if(!query.empty()||!collapsed.count(node->id()))for(auto* child:node->children())append(child,depth+1);
        };
        for(auto type:{InstanceClass::Workspace,InstanceClass::Lighting,InstanceClass::ServerScriptService,InstanceClass::ServerStorage,InstanceClass::ReplicatedStorage})
            if(auto* root=scene.service(type)){match(root);append(root,0);}
    }
    const std::vector<Row>& rows()const{return rows_;}
    std::vector<InstanceId> range(InstanceId anchor,InstanceId target)const {
        auto first=std::find_if(rows_.begin(),rows_.end(),[&](const Row& r){return r.id==anchor;});
        auto last=std::find_if(rows_.begin(),rows_.end(),[&](const Row& r){return r.id==target;});
        if(first==rows_.end()||last==rows_.end())return {};
        if(first>last)std::swap(first,last);
        std::vector<InstanceId> result;for(;first<=last;++first)result.push_back(first->id);return result;
    }
private:
    std::vector<Row> rows_;
};
}
