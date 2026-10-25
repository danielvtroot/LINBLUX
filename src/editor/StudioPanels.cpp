#include "ui/Localization.h"
#include "editor/EditorUI.h"
#include "ui/StudioFrontend.h"
#include "game/ImageAssets.h"
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
const char* kLuaGlobals[] = {"game","workspace","script","Instance","Vector3","Color3",
    "print","warn","error","task","math","string","table","utf8",
    "tonumber","tostring","type","assert","select","pairs","ipairs","next","require"};

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
    const int len=static_cast<int>(std::strlen(text));
    // Every byte belongs to a run, including whitespace and punctuation.
    // The former lexer dropped the prefix before identifiers, packing words.
    for(int i=0;i<len;) {
        int end=i+1; unsigned color=kLuaColorDefault;
        if(text[i]=='-' && i+1<len && text[i+1]=='-') {
            color=kLuaColorComment;end=i+2;
            if(end+1<len && text[end]=='[' && text[end+1]=='[') {
                end+=2;while(end+1<len && !(text[end]==']'&&text[end+1]==']'))++end;
                end=std::min(len,end+2);
            } else while(end<len && text[end]!='\n')++end;
        } else if(text[i]=='"' || text[i]=='\'') {
            color=kLuaColorString;
            while(end<len) { if(text[end]=='\\' && end+1<len){end+=2;continue;} if(text[end++]==text[i])break; }
        } else if(std::isdigit(static_cast<unsigned char>(text[i]))) {
            color=kLuaColorNumber;
            while(end<len && (std::isalnum(static_cast<unsigned char>(text[end])) || text[end]=='.'))++end;
        } else if(std::isalpha(static_cast<unsigned char>(text[i])) || text[i]=='_') {
            while(end<len && (std::isalnum(static_cast<unsigned char>(text[end])) || text[end]=='_'))++end;
            if(LuaWordInList(text,i,end,kLuaKeywords,IM_ARRAYSIZE(kLuaKeywords)))color=kLuaColorKeyword;
            else if(LuaWordInList(text,i,end,kLuaGlobals,IM_ARRAYSIZE(kLuaGlobals)))color=kLuaColorGlobal;
        }
        if(!runs.empty() && runs.back().color==color) runs.back().end=end;
        else runs.push_back({i,end,color});
        i=end;
    }
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
    std::size_t runCursor=0;
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
                while(runCursor<runs.size() && runs[runCursor].end<=p)++runCursor;
                for (auto index=runCursor;index<runs.size() && runs[index].begin<rowEnd;++index) {
                    const auto& run=runs[index];
                    const int s = std::max(run.begin, p), e = std::min(run.end, rowEnd);
                    if (e <= s) continue;
                    draw->AddText(pos, run.color, text + s, text + e);
                    pos.x += ImGui::CalcTextSize(text + s, text + e).x;
                }
            }
            if (caretVisible && !caretFound && caretByte >= p &&
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
    if(caretVisible && !caretFound && caretByte==len) {
        caretFound=true;caretPos={textOrigin.x,firstY+row*lineHeight};
    }
    if (caretVisible && caretFound)
        draw->AddRectFilled(caretPos, ImVec2(caretPos.x + 1.6f, caretPos.y + ImGui::GetFontSize()), caretColor);
    draw->PopClipRect();
}

// Indentação automática: o '\n' passa normalmente; quando ele chega ao
// callback de Edit, confirmamos que foi UMA inserção e replicamos a indentação
// da linha anterior (pastes ficam intactos; campos usam o mesmo canal do imgui
// para reconciliar undo/redo corretamente).
struct LuaEditCtx { bool singleNewline = false; int insertPos = 0; unsigned prevLen = 0; bool* dirty = nullptr; int* jumpLine=nullptr; int* jumpCursor=nullptr; };

int LuaEditCallback(ImGuiInputTextCallbackData* data) {
    auto* ctx = static_cast<LuaEditCtx*>(data->UserData);
    if(data->EventFlag==ImGuiInputTextFlags_CallbackAlways && ctx && ctx->jumpCursor && *ctx->jumpCursor>=0) {
        data->CursorPos=data->SelectionStart=data->SelectionEnd=
            std::clamp(*ctx->jumpCursor,0,data->BufTextLen);
        GImGui->InputTextState.CursorFollow=true;
        *ctx->jumpCursor=-1;
    }
    if(data->EventFlag==ImGuiInputTextFlags_CallbackAlways && ctx && ctx->jumpLine && *ctx->jumpLine>0) {
        int line=1,offset=0;
        while(offset<data->BufTextLen && line<*ctx->jumpLine)if(data->Buf[offset++]=='\n')++line;
        data->CursorPos=data->SelectionStart=data->SelectionEnd=offset;
        GImGui->InputTextState.CursorFollow=true;
        *ctx->jumpLine=0;return 0;
    }
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
    data->SelectionStart = data->SelectionEnd = data->CursorPos; // InsertChars already advanced the caret
    if (ctx->dirty) *ctx->dirty = true;
    return 0;
}
}
void EditorUI::drawInstanceProperties(Instance& object) {
    ImGui::TextWrapped(i18n::Label("%s  /  %s"),object.name().c_str(),object.className());
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint(i18n::Label("##property_filter"),"Search properties",propertiesFilter_,sizeof(propertiesFilter_));
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
        ImGui::TextUnformatted(i18n::Label(label));
        if (*help && ImGui::IsItemHovered()) ImGui::SetTooltip(i18n::Label("%s"),help);
        ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1); return true;
    };
    if (category("Data") && table("Data values")) {
        if (row("Name","Enter commits; Esc cancels the text edit.")) {
            char text[129]; std::snprintf(text,sizeof(text),"%s",object.name().c_str());
            if (ImGui::InputText(i18n::Label("##Name"),text,sizeof(text),ImGuiInputTextFlags_EnterReturnsTrue)) {
                if (*text) object.setName(text); else setStatus("Name nao pode ser vazio.",true);
            }
        }
        if (row("ClassName")) ImGui::TextDisabled(i18n::Label("%s"),object.className());
        if (row("Stable ID")) ImGui::TextDisabled(i18n::Label("%llu"),static_cast<unsigned long long>(object.id()));
        if (row("Parent","Organizational parenting preserves world transforms.")) {
            if (ImGui::BeginCombo(i18n::Label("##Parent"),object.parent()?object.parent()->name().c_str():"Game (root)")) {
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
                if (ImGui::ColorEdit3(i18n::Label("##Color"),color,ImGuiColorEditFlags_NoInputs))
                    part.setColor({static_cast<unsigned char>(std::lround(std::clamp(color[0],0.f,1.f)*255)),
                        static_cast<unsigned char>(std::lround(std::clamp(color[1],0.f,1.f)*255)),
                        static_cast<unsigned char>(std::lround(std::clamp(color[2],0.f,1.f)*255)),c.a});
            }
            if (row("Transparency","Stored as the color alpha channel.")) {
                float alpha=1-part.color().a/255.f;
                if (ImGui::SliderFloat(i18n::Label("##Transparency"),&alpha,0,1,"%.2f")) {
                    auto color=part.color(); color.a=static_cast<unsigned char>(std::lround((1-alpha)*255)); part.setColor(color);
                }
            }
            if (row("Material") && ImGui::BeginCombo(i18n::Label("##Material"),PartMaterialName(part.material()))) {
                for (int i=0;i<PartMaterialCount();++i) if(ImGui::Selectable(PartMaterialName(PartMaterialFromIndex(i)),part.material()==PartMaterialFromIndex(i))) part.setMaterial(PartMaterialFromIndex(i));
                ImGui::EndCombo();
            }
            ImGui::EndTable();
        }
        if (category("Decal & Texture") && table("Decal values")) {
            // Importador portátil: assets/<id>.<ext> relativos ao executável.
            static char importPath[512] = {};
            auto importPopup = [&]() {
                if (ImGui::BeginPopup("##import_image")) {
                    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
                    ImGui::InputTextWithHint(i18n::Label("##import_path"), "Path ou colhe o caminho (.png/.jpg)", importPath, sizeof(importPath));
                    if (ImGui::Button(i18n::Label("Importar")) && importPath[0]) {
                        std::string usable, err;
                        if (game::ImportImageTo(game::AssetsDir(), importPath, usable, err)) {
                            ImGui::CloseCurrentPopup();
                            setStatus("Imagem importada: " + usable);
                        } else setStatus(err, true);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(i18n::Label("Cancelar"))) ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                }
            };
            if (row("Decal Image", "Imagem aplicada-textura; ID estável, sem caminho absoluto.")) {
                const auto id = part.attributes().find("decal_image");
                if (ImGui::SmallButton(i18n::Label("Importar..."))) ImGui::OpenPopup("##import_image");
                ImGui::SameLine();
                ImGui::TextUnformatted(id != part.attributes().end() && std::get_if<std::string>(&id->second) ? std::get<std::string>(id->second).c_str() : "(nenhum)");
                importPopup();
            }
            if (row("Decal Face")) {
                int face = 0; const char* faces[6] = {"Front","Back","Left","Right","Top","Bottom"};
                if (const auto it = part.attributes().find("decal_face"); it != part.attributes().end())
                    if (const auto* txt = std::get_if<std::string>(&it->second))
                        for (int f = 0; f < 6; ++f) if (*txt == faces[f]) face = f;
                if (ImGui::BeginCombo(i18n::Label("##decal_face"), faces[face])) {
                    for (int f = 0; f < 6; ++f) if (ImGui::Selectable(faces[f])) part.setAttribute("decal_face", std::string(faces[f]));
                    ImGui::EndCombo();
                }
            }
            if (row("Decal Transparency")) {
                double t = 0.0;
                if (const auto it = part.attributes().find("decal_transparency"); it != part.attributes().end())
                    if (const auto* v = std::get_if<double>(&it->second)) t = *v;
                float transparency=static_cast<float>(t);
                if (ImGui::SliderFloat(i18n::Label("##decal_transparency"), &transparency, 0, 1, "%.2f"))
                    part.setAttribute("decal_transparency", static_cast<double>(transparency));
            }
            if (row("Decal Rotation")) {
                static const char* turns[4] = {"0","90","180","270"};
                double deg = 0.0;
                if (const auto it = part.attributes().find("decal_rotation"); it != part.attributes().end())
                    if (const auto* v = std::get_if<double>(&it->second)) deg = *v;
                int idx = (int)((int)std::lround(deg)) % 360;
                if (idx < 0) { idx += 360; }
                idx = idx / 90;
                if (ImGui::BeginCombo(i18n::Label("##decal_rotation"), turns[idx])) {
                    for (int t = 0; t < 4; ++t) if (ImGui::Selectable(turns[t])) part.setAttribute("decal_rotation", (double)(t * 90));
                    ImGui::EndCombo();
                }
            }
            if (row("Decal Tint")) {
                std::string hex = "FFFFFF";
                if (const auto it = part.attributes().find("decal_tint"); it != part.attributes().end())
                    if (const auto* v = std::get_if<std::string>(&it->second)) hex = *v;
                const auto channel = [&hex](std::size_t offset) {
                    const int byte = hex.size() >= offset + 2 ? std::stoi(hex.substr(offset, 2), nullptr, 16) : 255;
                    return std::clamp(byte, 0, 255) / 255.0f;
                };
                float tintValue[3] = {channel(0), channel(2), channel(4)};
                if (ImGui::ColorEdit3(i18n::Label("##decal_tint"), tintValue)) {
                    char hexOut[8]; std::snprintf(hexOut,sizeof(hexOut),"%02X%02X%02X",
                        (int)std::lround(tintValue[0]*255),(int)std::lround(tintValue[1]*255),(int)std::lround(tintValue[2]*255));
                    part.setAttribute("decal_tint", std::string(hexOut));
                }
            }
            ImGui::EndTable();
            static std::vector<game::ImageAsset> assetsCache;
            static double lastScan = -1.0;
            const double now = ImGui::GetTime();
            if (lastScan < 0.0 || now - lastScan > 2.0) { assetsCache = game::ListImages(game::AssetsDir()); lastScan = now; }
            ImGui::TextDisabled(i18n::Label("Imagens em %s: %zu"), game::AssetsDir().c_str(), assetsCache.size());
        }
        if (category("Behavior") && table("Behavior values")) {
            ImGui::BeginDisabled(part.instanceClass()==InstanceClass::Baseplate);
            if (row("Anchored")) { bool value=part.anchored(); if(ImGui::Checkbox(i18n::Label("##Anchored"),&value)) part.setAnchored(value); }
            if (row("CanCollide")) { bool value=part.canCollide(); if(ImGui::Checkbox(i18n::Label("##CanCollide"),&value)) part.setCanCollide(value); }
            if (row("Damage","Contact damage; solid Bullet contact, cooldown 0.75 seconds, runtime only.")) {
                float amount=0;const auto it=part.attributes().find("Damage");
                if(it!=part.attributes().end())if(const auto* n=std::get_if<double>(&it->second))amount=float(*n);
                if(ImGui::InputFloat(i18n::Label("##Damage"),&amount,1,10,"%.1f",ImGuiInputTextFlags_EnterReturnsTrue)) {
                    if(std::isfinite(amount)&&amount>=0&&amount<=100000)part.setAttribute("Damage",double(amount));
                    else setStatus("Damage deve ser finito, entre 0 e 100000.",true);
                }
            }
            ImGui::EndDisabled(); ImGui::EndTable();
        }
    }
    if (IsScriptClass(object.instanceClass()) && category("Script") && table("Script values")) {
        auto& script=static_cast<Script&>(object);
        if (row("Enabled")) ImGui::Checkbox(i18n::Label("##Enabled"),&script.enabled);
        if (row("RunContext")) ImGui::TextWrapped(i18n::Label("%s"),script.instanceClass()==InstanceClass::ModuleScript?"Module: on require":
            script.instanceClass()==InstanceClass::LocalScript?"Local player (in process)":"Authoritative (in process)");
        if (row("Source")) { ImGui::Text(i18n::Label("%zu bytes"),script.source.size());if(ImGui::SmallButton(i18n::Label("Open Script Editor")))openScript(script.id()); }
        if (row("Last runtime")) {
            const auto& messages=runtime_.scripts().messages();const scripting::Message* latest=nullptr;
            for(auto it=messages.rbegin();it!=messages.rend();++it)if(it->script==script.name()){latest=&*it;break;}
            if(latest)ImGui::TextWrapped(i18n::Label("%s: %s"),latest->error?"Error":"Output",latest->text.c_str());
            else ImGui::TextWrapped(i18n::Label("No output/error in this session"));
        }
        ImGui::EndTable();
    }
    ImGui::PopID();
    if (ImGui::Button(i18n::Label("Focus (F)"),{-1,0})) focusSelection();
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
    auto* node=scene_.find(id);
    if(!node||!IsScriptClass(node->instanceClass()))return;
    auto& draft=scriptDrafts_[id];
    if(draft.text.empty()&&draft.baseline.empty()) {
        draft.text=static_cast<Script&>(*node).source;
        draft.baseline=draft.text;
    }
    if(std::find(openDocuments_.begin(),openDocuments_.end(),id)==openDocuments_.end())
        openDocuments_.push_back(id);
    activeScript_=id;activeDocument_=id;draft.restoreFocus=true;
    showScriptEditor_=frontend_?false:true;
    scene_.select(node);
}

void EditorUI::openScriptAtLine(InstanceId id,int line) {
    openScript(id);
    auto it=scriptDrafts_.find(id);
    if(it!=scriptDrafts_.end()) {
        it->second.line=std::max(1,line);
        it->second.pendingLine=std::max(1,line);
        it->second.pendingCursor=-1;
        it->second.restoreFocus=true;
    }
}

void EditorUI::openScriptEditor() {
    if(activeDocument_!=0&&scriptDrafts_.count(activeDocument_))return;
    if(auto* selected=scene_.selection();selected&&IsScriptClass(selected->instanceClass())) {
        openScript(selected->id());return;
    }
    if(!openDocuments_.empty()) {
        if(scriptDrafts_.count(openDocuments_.back())) {
            activateDocument(openDocuments_.back());return;
        }
    }
    for(const auto& object:scene_.objects()) {
        if(IsScriptClass(object->instanceClass())) {openScript(object->id());return;}
    }
    setStatus("Crie ou selecione um Script para abrir no editor central.",true);
}

void EditorUI::activateDocument(InstanceId id) {
    if(id==0) {
        activeDocument_=0;activeScript_=0;showScriptEditor_=false;
        return;
    }
    auto* node=scene_.find(id);
    if(!node||!IsScriptClass(node->instanceClass()))return;
    if(std::find(openDocuments_.begin(),openDocuments_.end(),id)==openDocuments_.end())
        openDocuments_.push_back(id);
    activeDocument_=activeScript_=id;
    scriptDrafts_[id].restoreFocus=true;
    showScriptEditor_=frontend_?false:true;
    scene_.select(node);
}

void EditorUI::closeDocument(InstanceId id) {
    auto it=std::find(openDocuments_.begin(),openDocuments_.end(),id);
    if(it==openDocuments_.end())return;
    const int index=static_cast<int>(std::distance(openDocuments_.begin(),it));
    const bool active=activeDocument_==id;
    openDocuments_.erase(it);
    scriptDrafts_.erase(id);
    if(!active)return;
    InstanceId next=0;
    if(!openDocuments_.empty()) {
        const int nextIndex=std::min(index,static_cast<int>(openDocuments_.size())-1);
        next=openDocuments_[nextIndex];
    }
    activateDocument(next);
}

void EditorUI::pruneScriptDocuments() {
    for(auto it=openDocuments_.begin();it!=openDocuments_.end();) {
        auto* node=scene_.find(*it);
        if(node&&IsScriptClass(node->instanceClass())) {++it;continue;}
        const InstanceId id=*it;it=openDocuments_.erase(it);scriptDrafts_.erase(id);
        if(activeDocument_==id) {activeDocument_=0;activeScript_=0;showScriptEditor_=false;}
    }
    if(activeDocument_!=0&&!scriptDrafts_.count(activeDocument_)) {
        activeDocument_=0;activeScript_=0;showScriptEditor_=false;
    }
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

void EditorUI::drawCentralScriptWorkspace() {
    auto* object=scene_.find(activeDocument_);
    if(!object||!IsScriptClass(object->instanceClass())){activeDocument_=0;return;}
    auto& draft=scriptDrafts_[activeDocument_];
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(10,8));
    ImGui::SetCursorPos(ImVec2(10,8));
    ImGui::PushID(static_cast<int>(activeDocument_));
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(i18n::Label("SCRIPT"));
    ImGui::SameLine();
    ImGui::TextDisabled(i18n::Label("%s"),object->name().c_str());
    if(draft.dirty){ImGui::SameLine();ImGui::TextColored(ImVec4(1,.72f,.35f,1),"modified");}
    const float closeWidth=ImGui::CalcTextSize("Close tab").x+ImGui::GetStyle().FramePadding.x*2;
    ImGui::SameLine(std::max(ImGui::GetCursorPosX(),ImGui::GetWindowContentRegionMax().x-closeWidth));
    if(ImGui::SmallButton(i18n::Label("Close tab")))closeDocument(activeDocument_);
    drawScriptEditorBody(*object,draft,true);
    ImGui::PopID();
    ImGui::PopStyleVar();
}

void EditorUI::drawScriptEditorBody(Instance& object,ScriptDraft& draft,bool central) {
    const auto& io=ImGui::GetIO();
    const bool windowFocused=ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    if(ImGui::SmallButton(i18n::Label("Reveal in Explorer")))scene_.select(&object);
    ImGui::SameLine();
    if(ImGui::SmallButton(i18n::Label("Save project")))saveCurrentProject();
    ImGui::SameLine();
    if(ImGui::SmallButton(i18n::Label("Revert source"))) {
        draft.text=static_cast<Script&>(object).source;draft.baseline=draft.text;
        draft.dirty=false;draft.undo.clear();draft.redo.clear();
        draft.pendingCursor=0;draft.restoreFocus=true;
    }
    ImGui::SameLine();ImGui::TextDisabled(i18n::Label("%zu / 262144 bytes | Lua 5.4"),draft.text.size());
    ImGui::SetNextItemWidth(180);ImGui::InputTextWithHint(i18n::Label("##find"),"Find text",draft.search,sizeof(draft.search));
    if(*draft.search) {
        const auto found=draft.text.find(draft.search);
        ImGui::SameLine();if(found==std::string::npos)ImGui::TextDisabled(i18n::Label("Not found"));
        else {
            const int line=1+static_cast<int>(std::count(draft.text.begin(),draft.text.begin()+found,'\n'));
            ImGui::Text(i18n::Label("Line %d"),line);ImGui::SameLine();
            if(ImGui::SmallButton(i18n::Label("Go to match"))) {
                draft.line=line;draft.pendingLine=line;
                draft.pendingCursor=static_cast<int>(found);
                draft.restoreFocus=true;
            }
        }
    }
    ImGui::SetNextItemWidth(80);
    ImGui::InputInt(i18n::Label("Line"),&draft.line,0,0);ImGui::SameLine();
    if(ImGui::SmallButton(i18n::Label("Go"))) {
        draft.pendingLine=std::max(1,draft.line);
        draft.pendingCursor=-1;
        draft.restoreFocus=true;
    }
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // native monospace for code and gutter
    const auto origin=ImGui::GetCursorScreenPos();
    const float height=std::max(120.f,ImGui::GetContentRegionAvail().y-24);
    ImGui::Dummy({42,height});ImGui::SameLine();
    bool typedNewline=false;
    LuaEditCtx editCtx;editCtx.dirty=&typedNewline;editCtx.jumpLine=&draft.pendingLine;
    editCtx.jumpCursor=&draft.pendingCursor;
    // Tudo é desenhado pelo tokenizer; o widget nativo cuida de seleção,
    // cursor, teclado e acessibilidade. O undo por documento é próprio para
    // sobreviver à troca de abas (o ImGui mantém apenas um estado ativo).
    ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(0,0,0,0));
    const bool restoreFocus=draft.restoreFocus||draft.pendingLine>0||draft.pendingCursor>=0;
    if(restoreFocus) {
        ImGui::SetKeyboardFocusHere();
        if(draft.pendingLine<=0&&draft.pendingCursor<0)
            ImGui::SetNextWindowScroll(ImVec2(0,draft.scrollY));
    }
    const std::string beforeText=draft.text;
    const int beforeCursor=draft.cursor;
    const bool changed=ImGui::InputTextMultiline("##source",&draft.text,{-1,height},
            ImGuiInputTextFlags_AllowTabInput|ImGuiInputTextFlags_NoUndoRedo|
            ImGuiInputTextFlags_CallbackCharFilter|ImGuiInputTextFlags_CallbackEdit|ImGuiInputTextFlags_CallbackAlways,
            LuaEditCallback,&editCtx);
    ImGui::PopStyleColor();
    auto* state=ImGui::GetInputTextState(ImGui::GetItemID());
    const float lineHeight=ImGui::GetTextLineHeight();
    const ImVec2 fieldMin=ImGui::GetItemRectMin(),fieldMax=ImGui::GetItemRectMax();
    scriptSourceControl_={fieldMin.x,fieldMin.y,fieldMax.x-fieldMin.x,fieldMax.y-fieldMin.y};
    int caretByte=beforeCursor;bool caretVisible=false;
    if(state) {
        caretByte=state->GetCursorPos();
        caretVisible=state->ID==ImGui::GetItemID();
    }
    draft.cursor=caretByte;
    const auto& children=ImGui::GetCurrentWindow()->DC.ChildWindows;
    if(!children.empty())draft.scrollY=children.back()->Scroll.y;
    else if(state)draft.scrollY=state->Scroll.y;
    const float scrollPixels=draft.scrollY;
    const float scrollX=state?state->Scroll.x:0;
    if(changed||typedNewline) {
        const double now=ImGui::GetTime();
        if(now-draft.lastEditTime>0.75) {
            draft.undo.push_back({beforeText,beforeCursor});
            if(draft.undo.size()>200)draft.undo.erase(draft.undo.begin());
        }
        draft.redo.clear();draft.lastEditTime=now;
        draft.dirty=draft.text!=draft.baseline;
    }
    if(windowFocused&&io.KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_Z,false)) {
        if(!draft.undo.empty()) {
            draft.redo.push_back({draft.text,caretByte});
            const auto restore=draft.undo.back();draft.undo.pop_back();
            draft.text=restore.text;draft.cursor=restore.cursor;draft.pendingCursor=restore.cursor;
            draft.dirty=draft.text!=draft.baseline;draft.restoreFocus=true;
            ImGui::ClearActiveID();
        }
    } else if(windowFocused&&io.KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_Y,false)) {
        if(!draft.redo.empty()) {
            draft.undo.push_back({draft.text,caretByte});
            const auto restore=draft.redo.back();draft.redo.pop_back();
            draft.text=restore.text;draft.cursor=restore.cursor;draft.pendingCursor=restore.cursor;
            draft.dirty=draft.text!=draft.baseline;draft.restoreFocus=true;
            ImGui::ClearActiveID();
        }
    }
    static std::vector<LuaTokenRun> luaRuns;TokenizeLua(draft.text.c_str(),luaRuns);
    auto* draw=ImGui::GetWindowDrawList();
    DrawLuaSyntax(draw,
        ImVec2(fieldMin.x+ImGui::GetStyle().FramePadding.x-scrollX,fieldMin.y+ImGui::GetStyle().FramePadding.y),
        lineHeight,0, // native InputTextMultiline is not word-wrapped
        scrollPixels,draft.text.c_str(),(int)draft.text.size(),luaRuns,
        caretVisible,caretByte,fieldMin,fieldMax,kLuaColorCaret);
    int scriptErrors=0;
    for(const auto& msg:runtime_.scripts().messages())
        if(msg.error&&msg.scriptId==object.id())++scriptErrors;
    draw->PushClipRect(origin,{origin.x+40,origin.y+height},true);
    const auto count=1+std::count(draft.text.begin(),draft.text.end(),'\n');
    for(int i=std::max(0,int(scrollPixels/lineHeight));i<count&&(i*lineHeight-scrollPixels)<height;++i){
        char number[16];std::snprintf(number,sizeof(number),"%d",i+1);
        draw->AddText({origin.x+3,origin.y+ImGui::GetStyle().FramePadding.y+i*lineHeight-scrollPixels},IM_COL32(130,145,160,255),number);
    }
    if(scriptErrors>0)
        draw->AddRectFilled(origin,ImVec2(origin.x+2.5f,origin.y+height),IM_COL32(198,58,70,235));
    draw->PopClipRect();
    ImGui::PopFont();
    if(scriptErrors>0)ImGui::TextColored(ImVec4(1,.45f,.4f,1),"Erros de runtime: %d (veja o Output)",scriptErrors);
    else ImGui::TextDisabled(i18n::Label("Tab indents | Ctrl+Z/Y undo/redo | Ctrl+S save | Ctrl+W close tab | so executa no Play"));
    if(restoreFocus)draft.restoreFocus=false;
    if(windowFocused&&io.KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_S,false))saveCurrentProject();
    if(central&&windowFocused&&io.KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_W,false))closeDocument(activeDocument_);
}

void EditorUI::drawScriptEditor() {
    if (!ImGui::FindWindowByName(i18n::Label("Script Editor"))) {
        if(auto* view=ImGui::FindWindowByName(i18n::Label("Viewport"));view && view->DockId) {
            ImGuiID upper;auto lower=ImGui::DockBuilderSplitNode(view->DockId,ImGuiDir_Down,.5f,nullptr,&upper);
            ImGui::DockBuilderDockWindow(i18n::Label("Viewport"),upper);ImGui::DockBuilderDockWindow(i18n::Label("Script Editor"),lower);
            ImGui::DockBuilderFinish(ImGui::GetID("LinbluxDockspace"));
        }
    }
    ImGui::SetNextWindowSize({750,440},ImGuiCond_FirstUseEver);
    if(frontend_)frontend_->prepareAdvanced();
    if(!ImGui::Begin(i18n::Label("Script Editor"),&showScriptEditor_,frontend_?(ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoDocking):0)){ImGui::End();return;}
    if(ImGui::Button(i18n::Label("New Script")))insertScript();
    ImGui::SameLine();
    if(ImGui::Button(i18n::Label("New Module")))insertScript(InstanceClass::ModuleScript);
    ImGui::SameLine();
    if(ImGui::Button(i18n::Label("Save All + Project")))saveCurrentProject();
    ImGui::SameLine();
    if(ImGui::Button(i18n::Label("Play")))startPlay();
    ImGui::SameLine();if(ImGui::Button(i18n::Label("Output")))showOutput_=true;
    if(ImGui::BeginTabBar("Scripts",ImGuiTabBarFlags_FittingPolicyScroll)) {
        for(auto it=scriptDrafts_.begin();it!=scriptDrafts_.end();) {
            auto* object=scene_.find(it->first);
            if(!object || !IsScriptClass(object->instanceClass())){it=scriptDrafts_.erase(it);continue;}
            auto& draft=it->second;const auto label=object->name()+(draft.dirty?" *":"")+"###script"+std::to_string(it->first);
            const bool active=ImGui::BeginTabItem(label.c_str(),nullptr,requestedScript_==it->first?ImGuiTabItemFlags_SetSelected:0);
            if(requestedScript_==it->first)requestedScript_=0;
            if(active) {
                activeScript_=it->first;activeDocument_=it->first;
                ImGui::PushID(static_cast<int>(it->first));
                drawScriptEditorBody(*object,draft,false);
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
    if (!ImGui::FindWindowByName(i18n::Label("Output"))) {
        if(auto* editor=ImGui::FindWindowByName(i18n::Label("Script Editor"));editor && editor->DockId)
            ImGui::SetNextWindowDockID(editor->DockId,ImGuiCond_FirstUseEver);
    }
    ImGui::SetNextWindowSize({740,230},ImGuiCond_FirstUseEver);
    if(frontend_)frontend_->prepareAdvanced();
    if(!ImGui::Begin(i18n::Label("Output"),&showOutput_,frontend_?(ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoDocking):0)){ImGui::End();return;}
    ImGui::Text(i18n::Label("Lua: %zu KiB / 8192 | %zu / 128 tasks"),runtime_.scripts().memoryUsed()/1024,runtime_.scripts().taskCount());
    ImGui::Separator();
    int messageIndex=0;
    outputMessageRects_.clear();
    for(const auto& message:runtime_.scripts().messages()) {
        ImGui::PushID(++messageIndex);
        ImGui::PushStyleColor(ImGuiCol_Text,message.error?ImVec4(1,.4f,.35f,1):ImVec4(.7f,.85f,.75f,1));
        ImGui::TextWrapped(i18n::Label("[%s] %s"),message.script.c_str(),message.text.c_str());
        const ImVec2 messageMin=ImGui::GetItemRectMin(), messageMax=ImGui::GetItemRectMax();
        outputMessageRects_.push_back({messageMin.x,messageMin.y,messageMax.x-messageMin.x,messageMax.y-messageMin.y});
        ImGui::PopStyleColor();
        if(ImGui::IsItemHovered()) {
            ImGui::SetTooltip(message.error&&message.line>0?"Clique para abrir o script na linha %d":"Clique para abrir o script",message.line);
            if(ImGui::IsItemClicked(0)) {
                if(auto* node=scene_.find(message.scriptId);node&&IsScriptClass(node->instanceClass()))
                    openScriptAtLine(node->id(),message.line);
                else
                    for(const auto& node:scene_.objects())
                        if(IsScriptClass(node->instanceClass()) && message.script==node->name()) {
                            openScriptAtLine(node->id(),message.line);break;
                        }
            }
        }
        ImGui::PopID();
    }
    ImGui::End();
}
} // namespace loblox
