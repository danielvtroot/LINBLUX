#include "ui/Components.h"
namespace loblox::ui {
std::string EscapeMarkup(const std::string& value) {
    std::string result;result.reserve(value.size());
    for(char c:value)switch(c){case '&':result+="&amp;";break;case '<':result+="&lt;";break;
        case '>':result+="&gt;";break;case '"':result+="&quot;";break;default:result+=c;}
    return result;
}
std::string ButtonMarkup(const ButtonSpec& spec) {
    std::string result="<button";
    auto attribute=[&](const char* key,const std::string& value){result+=" ";result+=key;result+="=\""+EscapeMarkup(value)+"\"";};
    if(!spec.id.empty())attribute("id",spec.id);
    attribute(spec.domain==CommandDomain::Experience?"data-experience":"data-action",spec.action);
    attribute("data-value",spec.value);attribute("class","ui-button "+spec.classes);
    attribute("title",spec.tooltip.empty()?spec.label:spec.tooltip);
    if(!spec.enabled)attribute("disabled","disabled");
    result+='>';
    if(!spec.icon.empty())result+="<img src=\"icon-"+EscapeMarkup(spec.icon)+"\"/>";
    result+=EscapeMarkup(spec.label);result+="</button>";return result;
}
}
