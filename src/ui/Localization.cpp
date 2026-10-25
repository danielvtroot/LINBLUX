#include "ui/Localization.h"
#include "game/ImageAssets.h"
#include "nlohmann/json.hpp"
#include <fstream>
#include <map>
#include <regex>
namespace loblox::i18n {
namespace {
const std::map<std::string,std::string>& catalog() {
    static const auto entries=[] {
        std::map<std::string,std::string> values;
        auto path=game::BuiltinAssetPath("ui/locales/pt-BR.json");
        if(path.empty())path=game::BuiltinAssetPath("assets/ui/locales/pt-BR.json");
        try{std::ifstream file(path);auto j=nlohmann::json::parse(file);for(auto it=j.begin();it!=j.end();++it)values[it.key()]=it.value().get<std::string>();}catch(...){}
        return values;
    }();return entries;
}
}
std::string Text(const std::string& source) {
    auto found=catalog().find(source);if(found!=catalog().end())return found->second;
    const auto a=source.find_first_not_of(" \n\t\r"),b=source.find_last_not_of(" \n\t\r");
    if(a==std::string::npos)return source;
    found=catalog().find(source.substr(a,b-a+1));
    return found==catalog().end()?source:source.substr(0,a)+found->second+source.substr(b+1);
}
const char* Label(const char* source) {auto found=catalog().find(source);return found==catalog().end()?source:found->second.c_str();}
std::string Markup(const std::string& source) {
    std::string out;size_t pos=0;
    // Only display text and user-facing attributes are translated. IDs, data
    // attributes, resource paths, Lua and serialized API names stay untouched.
    while(pos<source.size()) {
        if(source[pos]!='<'){const auto end=source.find('<',pos);out+=Text(source.substr(pos,end-pos));if(end==std::string::npos)break;pos=end;}
        else {
            const auto end=source.find('>',pos);if(end==std::string::npos){out+=source.substr(pos);break;}
            std::string tag=source.substr(pos,end-pos+1);
            for(const char* attr:{"title=\"","placeholder=\""}) {
                size_t start=0;while((start=tag.find(attr,start))!=std::string::npos){start+=std::char_traits<char>::length(attr);const auto stop=tag.find('"',start);if(stop==std::string::npos)break;auto value=Text(tag.substr(start,stop-start));tag.replace(start,stop-start,value);start+=value.size()+1;}
            }
            out+=tag;pos=end+1;
        }
    }return out;
}
}
