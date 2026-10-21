#include "editor/EditorUI.h"
#include "scene/Scene.h"
#include "physics/PhysicsWorld.h"
#include "imgui.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace loblox {
namespace {
bool Matches(const char* text,const char* filter) {
    std::string a=text,b=filter;
    auto lower=[](unsigned char c){return std::tolower(c);};
    std::transform(a.begin(),a.end(),a.begin(),lower);
    std::transform(b.begin(),b.end(),b.begin(),lower);
    return a.find(b)!=std::string::npos;
}
bool Finite(const float* v) { return std::isfinite(v[0])&&std::isfinite(v[1])&&std::isfinite(v[2]); }
}
void EditorUI::drawInstanceProperties(Instance& object) {
    ImGui::TextWrapped("%s  /  %s",object.name().c_str(),object.className());
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##property_filter","Search properties",propertiesFilter_,sizeof(propertiesFilter_));
    ImGui::PushID(static_cast<int>(object.id()));
    const auto category=[&](const char* label) {
        return ImGui::CollapsingHeader(label,ImGuiTreeNodeFlags_DefaultOpen);
    };
    const auto table=[](const char* id) {
        if (!ImGui::BeginTable(id,2,ImGuiTableFlags_Resizable|ImGuiTableFlags_BordersInnerV|ImGuiTableFlags_RowBg)) return false;
        ImGui::TableSetupColumn("Property",ImGuiTableColumnFlags_WidthStretch,.38f);
        ImGui::TableSetupColumn("Value",ImGuiTableColumnFlags_WidthStretch,.62f);
        return true;
    };
    const auto row=[&](const char* label,const char* help="") {
        if (!Matches(label,propertiesFilter_)) return false;
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        if (*help && ImGui::IsItemHovered()) ImGui::SetTooltip("%s",help);
        ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1); return true;
    };
    if (category("Data") && table("Data values")) {
        if (row("Name","Enter commits; Esc cancels the text edit.")) {
            char text[129]; std::snprintf(text,sizeof(text),"%s",object.name().c_str());
            if (ImGui::InputText("##Name",text,sizeof(text),ImGuiInputTextFlags_EnterReturnsTrue)) {
                if (*text) object.setName(text); else setStatus("Name nao pode ser vazio.",true);
            }
        }
        if (row("ClassName")) ImGui::TextDisabled("%s",object.className());
        if (row("Stable ID")) ImGui::TextDisabled("%llu",static_cast<unsigned long long>(object.id()));
        if (row("Parent","Organizational parenting preserves world transforms.")) {
            if (ImGui::BeginCombo("##Parent",object.parent()?object.parent()->name().c_str():"Game (root)")) {
                for (const auto& candidate:scene_.objects()) {
                    if (!scene_.canParent(&object,candidate.get())) continue;
                    ImGui::PushID(static_cast<int>(candidate->id()));
                    const std::string label=candidate->name()+" #"+std::to_string(candidate->id());
                    if (ImGui::Selectable(label.c_str(),object.parent()==candidate.get())) {
                        std::string error; endGizmoSession(true,"property_parent");
                        if (!scene_.reparent(&object,candidate.get(),error)) setStatus(error,true);
                    }
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
        }
        ImGui::EndTable();
    }
    if (object.isPart()) {
        auto& part=static_cast<Part&>(object);
        if (category("Transform") && table("Transform values")) {
            const auto vector=[&](const char* name, Vector3 value,auto setter,bool size=false) {
                if (!row(name,"World coordinates. Enter commits; Esc cancels.")) return;
                float v[]{value.x,value.y,value.z};
                const std::string id="##"+std::string(name);
                if (ImGui::InputFloat3(id.c_str(),v,"%.3f",ImGuiInputTextFlags_EnterReturnsTrue)) {
                    if (!Finite(v) || (size && (v[0]<kMinPartSize||v[1]<kMinPartSize||v[2]<kMinPartSize)))
                        setStatus(std::string(name)+": valor invalido ou tamanho abaixo de 0.2.",true);
                    else { endGizmoSession(true,"property_transform"); setter(Vector3{v[0],v[1],v[2]}); physics_.applyChanges(&part); }
                }
            };
            vector("Position",part.position(),[&](Vector3 v){part.setPosition(v);});
            vector("Rotation",part.rotationEulerDegrees(),[&](Vector3 v){part.setRotationEulerDegrees(v);});
            vector("Size",part.size(),[&](Vector3 v){part.setSize(v);},true);
            ImGui::EndTable();
        }
        if (category("Appearance") && table("Appearance values")) {
            if (row("Color")) {
                const auto c=part.color(); float color[]{c.r/255.f,c.g/255.f,c.b/255.f};
                if (ImGui::ColorEdit3("##Color",color,ImGuiColorEditFlags_NoInputs))
                    part.setColor({static_cast<unsigned char>(std::lround(std::clamp(color[0],0.f,1.f)*255)),
                        static_cast<unsigned char>(std::lround(std::clamp(color[1],0.f,1.f)*255)),
                        static_cast<unsigned char>(std::lround(std::clamp(color[2],0.f,1.f)*255)),c.a});
            }
            if (row("Transparency","Stored as the color alpha channel.")) {
                float alpha=1-part.color().a/255.f;
                if (ImGui::SliderFloat("##Transparency",&alpha,0,1,"%.2f")) {
                    auto color=part.color(); color.a=static_cast<unsigned char>(std::lround((1-alpha)*255)); part.setColor(color);
                }
            }
            if (row("Material") && ImGui::BeginCombo("##Material",PartMaterialName(part.material()))) {
                for (int i=0;i<PartMaterialCount();++i) if(ImGui::Selectable(PartMaterialName(PartMaterialFromIndex(i)),part.material()==PartMaterialFromIndex(i))) part.setMaterial(PartMaterialFromIndex(i));
                ImGui::EndCombo();
            }
            ImGui::EndTable();
        }
        if (category("Behavior") && table("Behavior values")) {
            ImGui::BeginDisabled(part.instanceClass()==InstanceClass::Baseplate);
            if (row("Anchored")) { bool value=part.anchored(); if(ImGui::Checkbox("##Anchored",&value)) part.setAnchored(value); }
            if (row("CanCollide")) { bool value=part.canCollide(); if(ImGui::Checkbox("##CanCollide",&value)) part.setCanCollide(value); }
            ImGui::EndDisabled(); ImGui::EndTable();
        }
    }
    ImGui::PopID();
    if (ImGui::Button("Focus (F)",{-1,0})) focusSelection();
}
} // namespace loblox
