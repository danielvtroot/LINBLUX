#include "editor/EditorUI.h"
#include "scene/Scene.h"
#include "physics/PhysicsWorld.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"
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
                        else revealedSelection_=0;
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
    if (IsScriptClass(object.instanceClass()) && category("Script") && table("Script values")) {
        auto& script=static_cast<Script&>(object);
        if (row("Enabled")) ImGui::Checkbox("##Enabled",&script.enabled);
        if (row("RunContext")) ImGui::TextWrapped("%s",script.instanceClass()==InstanceClass::ModuleScript?"Module: on require":
            script.instanceClass()==InstanceClass::LocalScript?"Local player (in process)":"Authoritative (in process)");
        if (row("Source")) { ImGui::Text("%zu bytes",script.source.size());if(ImGui::SmallButton("Open Script Editor"))openScript(script.id()); }
        ImGui::EndTable();
    }
    ImGui::PopID();
    if (ImGui::Button("Focus (F)",{-1,0})) focusSelection();
}

void EditorUI::insertScript(InstanceClass type) {
    endGizmoSession(true,"insert_script");
    auto* parent=scene_.selection();
    if (!parent || parent==scene_.lighting() || IsScriptClass(parent->instanceClass()))
        parent=scene_.ensureService(type==InstanceClass::ModuleScript?InstanceClass::ReplicatedStorage:InstanceClass::ServerScriptService);
    if (auto* script=scene_.createScript(type,parent)) { scene_.select(script);openScript(script->id()); }
    else setStatus("Esse Parent nao aceita scripts.",true);
}
void EditorUI::openScript(InstanceId id) {
    auto* node=scene_.find(id);if(!node || !IsScriptClass(node->instanceClass()))return;
    if (!scriptDrafts_.count(id)) scriptDrafts_[id].text=static_cast<Script&>(*node).source;
    activeScript_=requestedScript_=id;showScriptEditor_=true;scene_.select(node);
}
bool EditorUI::commitScriptDrafts(std::string& error) {
    for(const auto& pair:scriptDrafts_) if (pair.second.dirty) {
        if (pair.second.text.size()>262144) { error="Script excede 256 KiB; reduza o source antes de salvar/Play.";return false; }
        if (!scene_.find(pair.first)) continue;
        if (!IsScriptClass(scene_.find(pair.first)->instanceClass())) {error="Script asset mudou de classe";return false;}
    }
    for(const auto& pair:scriptDrafts_) if(pair.second.dirty) {
        if(auto* node=scene_.find(pair.first))static_cast<Script&>(*node).source=pair.second.text;
    }
    return true;
}
void EditorUI::drawScriptEditor() {
    if (!ImGui::FindWindowByName("Script Editor")) {
        if(auto* view=ImGui::FindWindowByName("Viewport");view && view->DockId) {
            ImGuiID upper;auto lower=ImGui::DockBuilderSplitNode(view->DockId,ImGuiDir_Down,.5f,nullptr,&upper);
            ImGui::DockBuilderDockWindow("Viewport",upper);ImGui::DockBuilderDockWindow("Script Editor",lower);
            ImGui::DockBuilderFinish(ImGui::GetID("LobloxDockspace"));
        }
    }
    ImGui::SetNextWindowSize({750,440},ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("Script Editor",&showScriptEditor_)){ImGui::End();return;}
    if(ImGui::Button("New Script"))insertScript();
    ImGui::SameLine();
    if(ImGui::Button("New Module"))insertScript(InstanceClass::ModuleScript);
    ImGui::SameLine();
    if(ImGui::Button("Save All + Project"))saveCurrentProject();
    ImGui::SameLine();
    if(ImGui::Button("Play"))startPlay();
    ImGui::SameLine();if(ImGui::Button("Output"))showOutput_=true;
    if(ImGui::BeginTabBar("Scripts",ImGuiTabBarFlags_FittingPolicyScroll)) {
        for(auto it=scriptDrafts_.begin();it!=scriptDrafts_.end();) {
            auto* object=scene_.find(it->first);
            if(!object || !IsScriptClass(object->instanceClass())){it=scriptDrafts_.erase(it);continue;}
            auto& draft=it->second;const auto label=object->name()+(draft.dirty?" *":"")+"###script"+std::to_string(it->first);
            const bool active=ImGui::BeginTabItem(label.c_str(),nullptr,requestedScript_==it->first?ImGuiTabItemFlags_SetSelected:0);
            if(requestedScript_==it->first)requestedScript_=0;
            if(active) {
                activeScript_=it->first;
                ImGui::PushID(static_cast<int>(it->first));
                if(ImGui::SmallButton("Reveal in Explorer"))scene_.select(object);
                ImGui::SameLine(); if(ImGui::SmallButton("Revert source")){draft.text=static_cast<Script&>(*object).source;draft.dirty=false;}
                ImGui::SameLine();ImGui::TextDisabled("%zu / 262144 bytes | Lua 5.4",draft.text.size());
                ImGui::SetNextItemWidth(180);ImGui::InputTextWithHint("##find","Find text",draft.search,sizeof(draft.search));
                if(*draft.search) {
                    const auto found=draft.text.find(draft.search);
                    ImGui::SameLine();if(found==std::string::npos)ImGui::TextDisabled("Not found");
                    else ImGui::Text("Line %zu",1+static_cast<std::size_t>(std::count(draft.text.begin(),draft.text.begin()+found,'\n')));
                }
                const auto origin=ImGui::GetCursorScreenPos();const float height=std::max(100.f,ImGui::GetContentRegionAvail().y-22);
                ImGui::Dummy({42,height});ImGui::SameLine();
                if(ImGui::InputTextMultiline("##source",&draft.text,{-1,height},ImGuiInputTextFlags_AllowTabInput))draft.dirty=true;
                const auto* state=ImGui::GetInputTextState(ImGui::GetItemID());
                const float scroll=state?state->Scroll.y:0, line=ImGui::GetTextLineHeight();
                auto* draw=ImGui::GetWindowDrawList();draw->PushClipRect(origin,{origin.x+40,origin.y+height},true);
                const auto count=1+std::count(draft.text.begin(),draft.text.end(),'\n');
                for(int i=std::max(0,int(scroll/line));i<count && (i*line-scroll)<height;++i) {
                    char number[16];std::snprintf(number,sizeof(number),"%d",i+1);
                    draw->AddText({origin.x+3,origin.y+ImGui::GetStyle().FramePadding.y+i*line-scroll},IM_COL32(130,145,160,255),number);
                }
                draw->PopClipRect();ImGui::TextDisabled("Tab indents | Ctrl+Z/Y text undo/redo | Enter inserts line | code executes only in Play");
                ImGui::PopID();ImGui::EndTabItem();
            }
            ++it;
        }
        ImGui::EndTabBar();
    }
    if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S,false))saveCurrentProject();
    ImGui::End();
}
void EditorUI::drawOutput() {
    if (!ImGui::FindWindowByName("Output")) {
        if(auto* editor=ImGui::FindWindowByName("Script Editor");editor && editor->DockId)
            ImGui::SetNextWindowDockID(editor->DockId,ImGuiCond_FirstUseEver);
    }
    ImGui::SetNextWindowSize({740,230},ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("Output",&showOutput_)){ImGui::End();return;}
    ImGui::Text("Lua: %zu KiB / 8192 | %zu / 128 tasks",runtime_.scripts().memoryUsed()/1024,runtime_.scripts().taskCount());
    ImGui::Separator();
    for(const auto& message:runtime_.scripts().messages()) {
        ImGui::PushStyleColor(ImGuiCol_Text,message.error?ImVec4(1,.4f,.35f,1):ImVec4(.7f,.85f,.75f,1));
        ImGui::TextWrapped("[%s] %s",message.script.c_str(),message.text.c_str());ImGui::PopStyleColor();
    }
    ImGui::End();
}
} // namespace loblox
