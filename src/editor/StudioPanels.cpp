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
#include <cstring>
#include <vector>

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

// ---------- Script Editor: realce Lua, indentação e caret próprios ----------
struct LuaTokenRun { int begin; int end; unsigned color; };
constexpr unsigned kLuaColorDefault = IM_COL32(206, 214, 228, 255);
constexpr unsigned kLuaColorKeyword = IM_COL32(234, 118, 172, 255);
constexpr unsigned kLuaColorNumber  = IM_COL32(174, 143, 250, 255);
constexpr unsigned kLuaColorString  = IM_COL32(150, 201, 110, 255);
constexpr unsigned kLuaColorComment = IM_COL32(114, 128, 150, 255);
constexpr unsigned kLuaColorGlobal  = IM_COL32(118, 177, 255, 255);
constexpr unsigned kLuaColorCaret   = IM_COL32(224, 234, 249, 230);

const char* kLuaKeywords[] = {"and","break","do","else","elseif","end","false","for",
    "function","goto","if","in","local","nil","not","or","repeat","return","then",
    "true","until","while"};
const char* kLuaGlobals[] = {"game","workspace","script","Instance","Vector3","Vector2",
    "Color","Enum","Ray","print","warn","error","task","math","string","table",
    "tonumber","tostring","typeof","type","pcall","assert","select","pairs","ipairs",
    "next","require","spawn","wait"};

bool LuaWordInList(const char* text, int begin, int end, const char* const* list, int count) {
    const int length = end - begin;
    for (int i = 0; i < count; ++i) {
        const int wordLength = (int)std::strlen(list[i]);
        if (wordLength == length && std::memcmp(text + begin, list[i], (size_t)length) == 0) return true;
    }
    return false;
}

// Tokenizador leve para a viewport do editor: roda uma vez por frame sobre o
// source (scripts típicos têm poucos KiB) e alimentaa o desenho por runs.
void TokenizeLua(const char* text, std::vector<LuaTokenRun>& runs) {
    runs.clear();
    const int len = (int)std::strlen(text);
    if (len <= 0) return;
    runs.reserve(len / 12 + 1);
    int i = 0, runStart = 0;
    unsigned cur = kLuaColorDefault;
    bool blockComment = false;
    const auto toColor = [&](int pos, unsigned color) {
        if (cur != color) {
            if (runStart < pos) runs.push_back({runStart, pos, cur});
            cur = color; runStart = pos;
        }
    };
    auto splitToken = [&](int j, unsigned color) {
        runs.push_back({i, j, color});
        cur = kLuaColorDefault; runStart = j; i = j;
    };
    while (i < len) {
        const unsigned char c = (unsigned char)text[i];
        if (blockComment) {
            if (c == ']' && i + 1 < len && text[i+1] == ']') {
                blockComment = false;
                i += 2;
                if (cur != kLuaColorDefault) { runs.push_back({runStart, i, cur}); cur = kLuaColorDefault; runStart = i; }
                continue;
            }
            ++i; continue;
        }
        if (c == '-' && i + 1 < len && text[i+1] == '-' && cur != kLuaColorComment) {
            if (i + 3 < len && text[i+2] == '[' && text[i+3] == '[') {
                blockComment = true;
                toColor(i, kLuaColorComment);
                i += 4; continue;
            }
            if (i > runStart) runs.push_back({runStart, i, cur});
            int j = i + 2;
            while (j < len && text[j] != '\n') ++j;
            runs.push_back({i, j, kLuaColorComment});
            cur = kLuaColorDefault; runStart = j; i = j;
            continue;
        }
        if (c == '"' || c == '\'') {
            toColor(i, kLuaColorString);
            int j = i + 1;
            while (j < len && text[j] != c && text[j] != '\n') {
                if (text[j] == '\\' && j + 1 < len) ++j;  // pula escapes
                ++j;
            }
            const int stop = std::min(j + ((j < len && text[j] == c) ? 1 : 0), len);
            splitToken(stop, kLuaColorString);
            continue;
        }
        if (c >= '0' && c <= '9') {
            toColor(i, kLuaColorNumber);
            int j = i + 1;
            while (j < len && ((text[j] >= '0' && text[j] <= '9') || (text[j] >= 'a' && text[j] <= 'f') ||
                               (text[j] >= 'A' && text[j] <= 'F') || text[j] == '.' || text[j] == '_')) ++j;
            splitToken(j, kLuaColorNumber);
            continue;
        }
        if (std::isalpha(c) || c == '_') {
            int j = i + 1;
            while (j < len && (std::isalnum((unsigned char)text[j]) || text[j] == '_')) ++j;
            const bool keyword = LuaWordInList(text, i, j, kLuaKeywords,
                                               (int)(sizeof(kLuaKeywords)/sizeof(kLuaKeywords[0])));
            const bool global = !keyword && LuaWordInList(text, i, j, kLuaGlobals,
                                                          (int)(sizeof(kLuaGlobals)/sizeof(kLuaGlobals[0])));
            const unsigned color = keyword ? kLuaColorKeyword : global ? kLuaColorGlobal : kLuaColorDefault;
            splitToken(j, color);
            continue;
        }
        if (c == '\n') toColor(i, kLuaColorDefault);  // nada atravessa uma linha
        ++i;  // caractere comum continua o run atual
    }
    if (runStart < len && (cur != kLuaColorDefault || runStart < i))
        runs.push_back({runStart, len, cur});
}

// Desenha o texto colorido ma zona do InputTextMultiline (o texto nativo fica
// invisível via cor transparente; seleção nativa, undo e scroll continuam).
// Compatível com o wrap do ImGui quando a linha excede a largura do painel.
void DrawLuaSyntax(ImDrawList* draw, ImVec2 textOrigin, float lineHeight, float wrapWidth,
                   float scrollPixels, const char* text, int len,
                   const std::vector<LuaTokenRun>& runs, bool caretVisible, int caretByte,
                   ImVec2 clipMin, ImVec2 clipMax, unsigned caretColor) {
    ImFont* font = ImGui::GetFont();
    draw->PushClipRect(clipMin, clipMax, true);
    const float firstY = textOrigin.y - scrollPixels;
    const int firstRow = std::max(0, (int)std::floor(scrollPixels / lineHeight) - 1);
    const float bottom = clipMax.y + lineHeight * 2.0f;
    int row = 0;
    int i = 0;
    bool caretFound = false;
    ImVec2 caretPos{};
    while (i < len) {
        int lineEnd = i;
        while (lineEnd < len && text[lineEnd] != '\n') ++lineEnd;
        int p = i;
        if (p == lineEnd) {  // linha vazia: uma row que ainda completa o wrap
            const float y = firstY + (float)row * lineHeight;
            if (caretVisible && !caretFound && caretByte == p) {
                caretFound = true; caretPos = ImVec2(textOrigin.x, y);
            }
            ++row; i = lineEnd + 1; continue;
        }
        while (p < lineEnd) {
            int rowEnd = lineEnd;
            if (wrapWidth > 0) {
                const char* brk = font->CalcWordWrapPosition(1.0f, text + p, text + lineEnd, wrapWidth);
                if (brk != nullptr) {
                    rowEnd = (int)(brk - text);
                    if (rowEnd <= p) rowEnd = std::min(lineEnd, p + 1);
                }
            }
            const float y = firstY + (float)row * lineHeight;
            if (row >= firstRow && y <= bottom) {
                ImVec2 pos{textOrigin.x, y};
                for (const auto& run : runs) {
                    const int s = std::max(run.begin, p), e = std::min(run.end, rowEnd);
                    if (e <= s) continue;
                    draw->AddText(pos, run.color, text + s, text + e);
                    pos.x += ImGui::CalcTextSize(text + s, text + e).x;
                }
            }
            if (caretVisible && !caretFound &&
                (caretByte < rowEnd || (caretByte == rowEnd && rowEnd == lineEnd))) {
                caretFound = true;
                caretPos = ImVec2(textOrigin.x + ImGui::CalcTextSize(text + p, text + std::min(caretByte, rowEnd)).x, y);
            }
            ++row;
            p = rowEnd;
            if (p >= lineEnd) break;
        }
        if (lineEnd < len) { i = lineEnd + 1; } else { break; }
    }
    if (caretVisible && caretFound)
        draw->AddRectFilled(caretPos, ImVec2(caretPos.x + 1.6f, caretPos.y + ImGui::GetFontSize()), caretColor);
    draw->PopClipRect();
}

// Indentação automática: o '\n' passa normalmente; quando ele chega ao
// callback de Edit, confirmamos que foi UMA inserção e replicamos a indentação
// da linha anterior (pastes ficam intactos; campos usam o mesmo canal do imgui
// para reconciliar undo/redo corretamente).
struct LuaEditCtx { bool singleNewline = false; int insertPos = 0; unsigned prevLen = 0; bool* dirty = nullptr; };

int LuaEditCallback(ImGuiInputTextCallbackData* data) {
    auto* ctx = static_cast<LuaEditCtx*>(data->UserData);
    if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter) {
        if (data->EventChar == '\n') {
            ctx->singleNewline = true;
            ctx->insertPos = data->CursorPos;
            ctx->prevLen = (unsigned)data->BufTextLen;
        }
        return 0;
    }
    if (ctx == nullptr || !ctx->singleNewline) return 0;
    ctx->singleNewline = false;
    const int len = data->BufTextLen;
    if (len < 0 || (unsigned)len != ctx->prevLen + 1 || data->CursorPos != ctx->insertPos + 1 ||
        data->CursorPos > len || data->Buf[ctx->insertPos] != '\n') return 0;
    int lineStart = ctx->insertPos;
    while (lineStart > 0 && data->Buf[lineStart-1] != '\n') --lineStart;
    int indent = 0;
    while (lineStart + indent < ctx->insertPos && data->Buf[lineStart+indent] == ' ') ++indent;
    if (indent > 96) indent = 96;
    if (indent <= 0 || data->BufSize < len + indent + 1 || ctx->insertPos >= len) return 0;
    char spaces[97];
    std::memset(spaces, ' ', (size_t)indent); spaces[indent] = 0;
    data->InsertChars(data->CursorPos, spaces);
    data->BufDirty = true;
    data->CursorPos = data->SelectionStart = data->SelectionEnd = data->CursorPos + indent;
    if (ctx->dirty) *ctx->dirty = true;
    return 0;
}
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
            if (row("Damage","Contact damage; solid Bullet contact, cooldown 0.75 seconds, runtime only.")) {
                float amount=0;const auto it=part.attributes().find("Damage");
                if(it!=part.attributes().end())if(const auto* n=std::get_if<double>(&it->second))amount=float(*n);
                if(ImGui::InputFloat("##Damage",&amount,1,10,"%.1f",ImGuiInputTextFlags_EnterReturnsTrue)) {
                    if(std::isfinite(amount)&&amount>=0&&amount<=100000)part.setAttribute("Damage",double(amount));
                    else setStatus("Damage deve ser finito, entre 0 e 100000.",true);
                }
            }
            ImGui::EndDisabled(); ImGui::EndTable();
        }
    }
    if (IsScriptClass(object.instanceClass()) && category("Script") && table("Script values")) {
        auto& script=static_cast<Script&>(object);
        if (row("Enabled")) ImGui::Checkbox("##Enabled",&script.enabled);
        if (row("RunContext")) ImGui::TextWrapped("%s",script.instanceClass()==InstanceClass::ModuleScript?"Module: on require":
            script.instanceClass()==InstanceClass::LocalScript?"Local player (in process)":"Authoritative (in process)");
        if (row("Source")) { ImGui::Text("%zu bytes",script.source.size());if(ImGui::SmallButton("Open Script Editor"))openScript(script.id()); }
        if (row("Last runtime")) {
            const auto& messages=runtime_.scripts().messages();const scripting::Message* latest=nullptr;
            for(auto it=messages.rbegin();it!=messages.rend();++it)if(it->script==script.name()){latest=&*it;break;}
            if(latest)ImGui::TextWrapped("%s: %s",latest->error?"Error":"Output",latest->text.c_str());
            else ImGui::TextWrapped("No output/error in this session");
        }
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
                bool typedNewline=false;
                LuaEditCtx editCtx;editCtx.dirty=&typedNewline;
                // O texto nativo fica invisível (canal alpha 0): o realce do
                // TokenizerLua desenha tudo por cima; seleção/undo/scroll nativos.
                ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(0,0,0,0));
                if(ImGui::InputTextMultiline("##source",&draft.text,{-1,height},
                        ImGuiInputTextFlags_AllowTabInput|
                        ImGuiInputTextFlags_CallbackCharFilter|ImGuiInputTextFlags_CallbackEdit,
                        LuaEditCallback,&editCtx))draft.dirty=true;
                if(typedNewline)draft.dirty=true;
                ImGui::PopStyleColor();
                const auto* state=ImGui::GetInputTextState(ImGui::GetItemID());
                const float lineHeight=ImGui::GetTextLineHeight();
                const ImVec2 fieldMin=ImGui::GetItemRectMin(),fieldMax=ImGui::GetItemRectMax();
                const float scrollPixels=state?state->Scroll.y*lineHeight:0;
                std::vector<LuaTokenRun> luaRuns;TokenizeLua(draft.text.c_str(),luaRuns);
                int caretByte=0;bool caretVisible=false;
                if(state){caretByte=state->GetCursorPos();caretVisible=state->ID==ImGui::GetItemID();}
                auto* draw=ImGui::GetWindowDrawList();
                DrawLuaSyntax(draw,
                    ImVec2(fieldMin.x+ImGui::GetStyle().FramePadding.x,fieldMin.y+ImGui::GetStyle().FramePadding.y),
                    lineHeight,std::max(1.f,ImGui::GetItemRectSize().x-ImGui::GetStyle().FramePadding.x*2),
                    scrollPixels,draft.text.c_str(),(int)draft.text.size(),luaRuns,
                    caretVisible,caretByte,fieldMin,fieldMax,kLuaColorCaret);
                // Erros de runtime deste script: barra vermelha na margem + contagem.
                int scriptErrors=0;
                for(const auto& msg:runtime_.scripts().messages())
                    if(msg.error&&msg.script==object->name())++scriptErrors;
                draw->PushClipRect(origin,{origin.x+40,origin.y+height},true);
                const auto count=1+std::count(draft.text.begin(),draft.text.end(),'\n');
                for(int i=std::max(0,int(scrollPixels/lineHeight));i<count&&(i*lineHeight-scrollPixels)<height;++i){
                    char number[16];std::snprintf(number,sizeof(number),"%d",i+1);
                    draw->AddText({origin.x+3,origin.y+ImGui::GetStyle().FramePadding.y+i*lineHeight-scrollPixels},IM_COL32(130,145,160,255),number);
                }
                if(scriptErrors>0)
                    draw->AddRectFilled(origin,ImVec2(origin.x+2.5f,origin.y+height),IM_COL32(198,58,70,235));
                draw->PopClipRect();
                if(scriptErrors>0)ImGui::TextColored(ImVec4(1,.45f,.4f,1),"Erros de runtime: %d (veja o Output)",scriptErrors);
                else ImGui::TextDisabled("Tab indents | Ctrl+Z/Y undo/redo | Enter indenta a linha | so executa no Play");
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
    int messageIndex=0;
    for(const auto& message:runtime_.scripts().messages()) {
        ImGui::PushID(++messageIndex);
        ImGui::PushStyleColor(ImGuiCol_Text,message.error?ImVec4(1,.4f,.35f,1):ImVec4(.7f,.85f,.75f,1));
        ImGui::TextWrapped("[%s] %s",message.script.c_str(),message.text.c_str());
        ImGui::PopStyleColor();
        if(ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Clique para abrir o script");
            if(ImGui::IsItemClicked(0))
                for(const auto& pair:scriptDrafts_)
                    if(auto* node=scene_.find(pair.first);
                       !pair.second.text.empty()&&IsScriptClass(node?
                        node->instanceClass():InstanceClass::Script)&&message.script==node->name()) {
                        openScript(pair.first);break;
                    }
        }
        ImGui::PopID();
    }
    ImGui::End();
}
} // namespace loblox
