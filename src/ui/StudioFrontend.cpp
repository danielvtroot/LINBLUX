#include "ui/StudioFrontend.h"
#include "ui/EditorActions.h"
#include "ui/ExperienceUI.h"
#include "ui/Localization.h"
#include "ui/ImageSafety.h"
#include "ui/RmlFrontendSupport.h"
#include "editor/EditorUI.h"
#include "rendering/EditorCamera.h"
#include "scene/Scene.h"
#include "objects/Lighting.h"
#include "objects/Workspace.h"
#include "game/ImageAssets.h"
#include "game/Spawn.h"
#include "core/Diagnostics.h"
#include "core/NumberFormat.h"
#include "RmlUi_Renderer_GL3.h"
#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <RmlUi/Core/Elements/ElementFormControlSelect.h>
#include "imgui.h"
#include "rlImGui.h"
#include "rlgl.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <map>

namespace loblox {
namespace {
std::string Escape(const std::string& s) {std::string out;for(char c:s)out+=c=='&'?"&amp;":c=='<'?"&lt;":c=='>'?"&gt;":c=='\"'?"&quot;":std::string(1,c);return out;}
// Exibicao numerica do Properties: sem notacao cientifica e sem residuos de
// ponto flutuante (-9.7e-12 vira "0"). O valor interno nunca e alterado; a
// edicao por texto continua enviando o numero exato digitado.
std::string Num(float v) {return numfmt::Text(v, 4);}
std::string Hex(Color c) {char s[8];std::snprintf(s,sizeof(s),"#%02x%02x%02x",c.r,c.g,c.b);return s;}
std::string Button(const std::string& action,const std::string& label,const std::string& icon="",const std::string& value="",const std::string& scope="cmd") {
    return "<button id=\""+scope+"-"+action+"-"+Escape(value)+"\" data-action=\""+action+"\" data-value=\""+Escape(value)+"\">"+(icon.empty()?"":"<img src=\"icon-"+icon+"\"/>")+Escape(i18n::Text(label))+"</button>";
}
bool Within(Vector2 p,Rectangle r){return r.width>0 && CheckCollisionPointRec(p,r);}
std::filesystem::path LayoutPath() {
    if(const char* p=std::getenv("XDG_CONFIG_HOME"))return std::filesystem::path(p)/"linblux/ui-layout.json";
    if(const char* p=std::getenv("HOME"))return std::filesystem::path(p)/".config/linblux/ui-layout.json";
    return {};
}
}

struct StudioFrontend::Impl final:Rml::EventListener {
    EditorUI& ui;EditorActions actions;ui::RmlPlatform system;std::unique_ptr<ui::StudioRenderer> renderer;
    Rml::Context* context=nullptr;Rml::ElementDocument* document=nullptr;
    Rectangle view{},advancedRect{};float left=204,right=292,bottom=330,propertySplit=.52f,dpi=1;
    std::string tab="Home",advanced,search,page="game",dialog,focusValue,split,propertySearch;
    bool pauseWasOpen=false;float menuProgress=0;
    std::string selectedRigDecal,selectedDecalRig;
    std::string tooltip;double tooltipSince=0;bool contextOpen=false;
    std::string treeMarkup,propertyMarkup,ribbonMarkup,advancedMarkup,documentMarkup;size_t treeHash=0;
    std::map<std::pair<std::string,std::string>,std::string> styles;
    std::vector<game::ImageAsset> images;double imageScan=-10;std::string imageQuery;
    std::vector<game::SoundAsset> audioFiles;double audioScan=-10;std::string audioQuery;
    std::set<InstanceId> collapsed;InstanceId dragNode=0,dragTarget=0;Vector2 dragStart{};
    bool dragging=false,mouseOwned=false,escapeConsumed=false,dialogOpen=false,treeKeyboard=false,restoreFocus=false;
    bool initialized=false,syncing=false,keyboardOwned=false;float shownHealth=100,healthTrail=100;int w=0,h=0;
    // Diagnostic geometry for automated tests: LOBLOX_TRACE_UI prints the
    // element boxes with their absolute offsets after every window resize, so
    // a harness can target a control by id instead of a guessed coordinate.
    int tracedW=0,tracedH=0;
    void traceBoxes() {
        if(!std::getenv("LOBLOX_TRACE_UI"))return;
        tracedW=GetScreenWidth();tracedH=GetScreenHeight();
        std::fprintf(stderr,"RML layout %dx%d\n",tracedW,tracedH);
        std::function<void(Rml::Element*,int)> trace=[&](Rml::Element* e,int depth) {
            if(depth>3)return;
            const auto size=e->GetBox().GetSize(Rml::BoxArea::Border);
            const auto offset=e->GetAbsoluteOffset(Rml::BoxArea::Border);
            std::fprintf(stderr,"RML box %d %s #%s .%s: %.1fx%.1f at %.1f,%.1f display=%s width=%s\n",
                depth,e->GetTagName().c_str(),e->GetId().c_str(),e->GetClassNames().c_str(),
                size.x,size.y,offset.x,offset.y,e->GetProperty("display")->ToString().c_str(),
                e->GetProperty("width")->ToString().c_str());
            for(int i=0;i<e->GetNumChildren();++i)trace(e->GetChild(i),depth+1);
        };
        trace(document,0);
    }
    game::AudioSystem previewAudio;int previewVoice=0;std::string previewRoot;
    std::vector<game::ImageUsage> imageUses;
    std::vector<EditorCommand> commands;
    std::unique_ptr<ExperienceUI> experiences;
    const bool timing=std::getenv("LOBLOX_UI_TIMINGS")!=nullptr;
    double presentationTime=0,sceneTime=0,updateTime=0,renderTime=0,frameTime=0;int timingFrames=0;
    explicit Impl(EditorUI& editor):ui(editor),actions(editor){}
    Rml::Element* el(const std::string& id)const{return document?document->GetElementById(id):nullptr;}
    void style(const std::string& id,const std::string& key,const std::string& value) {auto* e=el(id);if(!e)return;auto& cached=styles[{id,key}];if(cached!=value){e->SetProperty(key,value);cached=value;}}
    void display(const std::string& id,bool show,const char* layout=nullptr) {
        auto* e=el(id);if(!e)return;
        const std::pair<std::string,std::string> key{id,"display"};
        auto it=styles.find(key);
        if(!show) {
            if(it==styles.end()||it->second!="none") {e->SetProperty("display","none");styles[key]="none";}
            return;
        }
        // Revealing an element must not turn a styled flex container into a
        // block. Drop our inline override and let the RCSS layout apply.
        if(it!=styles.end()&&it->second=="none") {e->RemoveProperty("display");styles.erase(it);}
        if(layout) {
            auto& cached=styles[key];
            if(cached!=layout) {e->SetProperty("display",layout);cached=layout;}
        }
    }
    void text(const std::string& id,const std::string& value){if(auto* e=el(id);e&&e->GetInnerRML()!=Escape(i18n::Text(value)))e->SetInnerRML(Escape(i18n::Text(value)));}
    void html(const std::string& id,const std::string& value,std::string& old){if(value!=old){el(id)->SetInnerRML(i18n::Markup(value));old=value;}}
    void rect(const std::string& id,Rectangle r){style(id,"left",Num(r.x)+"px");style(id,"top",Num(r.y)+"px");style(id,"width",Num(r.width)+"px");style(id,"height",Num(r.height)+"px");}
    std::string attr(Rml::Element* e,const char* name)const{return e?e->GetAttribute<Rml::String>(name,""):"";}
    Rml::Element* ancestor(Rml::Element* e,const char* name)const{while(e&&e!=document){if(e->HasAttribute(name))return e;e=e->GetParentNode();}return nullptr;}
    std::string value(Rml::Element* e)const {if(auto* form=dynamic_cast<Rml::ElementFormControl*>(e))return form->GetValue();return "";}
    Rml::Element* formOwner(Rml::Element* element)const {
        // Native selects focus their internal selectvalue/selectarrow nodes.
        // The owning form remains the keyboard consumer throughout that focus.
        for(auto* e=element;e&&e!=document;e=e->GetParentNode())
            if(dynamic_cast<Rml::ElementFormControl*>(e))return e;
        return nullptr;
    }
    bool editing()const {return context&&formOwner(context->GetFocusElement());}
    void experienceInput() {
        const auto& io=ImGui::GetIO();mouseOwned=keyboardOwned=true;escapeConsumed=false;
        if(!IsWindowFocused()){context->ProcessMouseLeave();context->ProcessMouseButtonUp(0,0);return;}
        const int mods=(io.KeyCtrl?Rml::Input::KM_CTRL:0)|(io.KeyShift?Rml::Input::KM_SHIFT:0)|(io.KeyAlt?Rml::Input::KM_ALT:0);
        context->ProcessMouseMove(int(io.MousePos.x*dpi),int(io.MousePos.y*dpi),mods);
        for(int i=0;i<3;++i){if(ImGui::IsMouseClicked(i))context->ProcessMouseButtonDown(i,mods);if(ImGui::IsMouseReleased(i))context->ProcessMouseButtonUp(i,mods);}
        if(io.MouseWheel!=0)context->ProcessMouseWheel(-io.MouseWheel,mods);
        for(auto c:io.InputQueueCharacters)context->ProcessTextInput(Rml::Character(c));
        const std::pair<ImGuiKey,Rml::Input::KeyIdentifier> keys[]={
            {ImGuiKey_Backspace,Rml::Input::KI_BACK},{ImGuiKey_Delete,Rml::Input::KI_DELETE},
            {ImGuiKey_LeftArrow,Rml::Input::KI_LEFT},{ImGuiKey_RightArrow,Rml::Input::KI_RIGHT},
            {ImGuiKey_UpArrow,Rml::Input::KI_UP},{ImGuiKey_DownArrow,Rml::Input::KI_DOWN},
            {ImGuiKey_Home,Rml::Input::KI_HOME},{ImGuiKey_End,Rml::Input::KI_END},{ImGuiKey_Tab,Rml::Input::KI_TAB},
            {ImGuiKey_Enter,Rml::Input::KI_RETURN},{ImGuiKey_A,Rml::Input::KI_A},{ImGuiKey_C,Rml::Input::KI_C},
            {ImGuiKey_V,Rml::Input::KI_V},{ImGuiKey_X,Rml::Input::KI_X},{ImGuiKey_Z,Rml::Input::KI_Z},{ImGuiKey_Y,Rml::Input::KI_Y}};
        for(const auto& key:keys){if(ImGui::IsKeyPressed(key.first))context->ProcessKeyDown(key.second,mods);if(ImGui::IsKeyReleased(key.first))context->ProcessKeyUp(key.second,mods);}
        if(ImGui::IsKeyPressed(ImGuiKey_Escape,false))experiences->enqueue(experiences->loading()?"cancel-load":"cancel-confirm");
    }
    Rectangle bounds(const std::string& id)const {auto* e=el(id);if(!e||!e->IsVisible(true))return {};auto p=e->GetAbsoluteOffset(Rml::BoxArea::Border);auto s=e->GetBox().GetSize(Rml::BoxArea::Border);return {p.x,p.y,s.x,s.y};}
    void loadLayout(){try{std::ifstream file(LayoutPath());nlohmann::json j;if(file>>j){left=j.value("left",left);right=j.value("right",right);bottom=j.value("bottom",bottom);propertySplit=j.value("properties",propertySplit);ui.showToolbox_=j.value("showToolbox",true);ui.showExplorer_=j.value("showExplorer",true);ui.showProperties_=j.value("showProperties",true);}}catch(...) {}}
    void saveLayout(){try {const auto path=LayoutPath();if(path.empty())return;std::filesystem::create_directories(path.parent_path());auto temp=path;temp+=".tmp";{std::ofstream file(temp);file<<nlohmann::json{{"left",left},{"right",right},{"bottom",bottom},{"properties",propertySplit},{"showToolbox",ui.showToolbox_},{"showExplorer",ui.showExplorer_},{"showProperties",ui.showProperties_}};if(!file)return;}std::filesystem::rename(temp,path);}catch(...) {}}
    void layout() {
        w=GetScreenWidth();h=GetScreenHeight();dpi=float(GetRenderWidth())/std::max(1,w);
        context->SetDimensions({GetRenderWidth(),GetRenderHeight()});
        style("studio","width",std::to_string(w)+"px");style("studio","height",std::to_string(h)+"px");
        style("studio","transform-origin","0px 0px");style("studio","transform",dpi==1?"none":"scale("+Num(dpi)+")");
        document->SetClass("compact",w<1400);
        const bool immersive=ui.gameImmersive();
        display("header",!immersive);display("tabs",!immersive);display("ribbon",!immersive);display("status",!immersive);
        if(immersive) {
            for(const char* id:{"toolbox","explorer","properties","viewport-label","left-divider","right-divider","property-divider","advanced","bottom-divider"})display(id,false);
            view={0,0,float(w),float(h)};
            document->SetClass("player-compact",view.width<950);
            advancedRect={0,0,0,0};
            rect("game-layer",view);display("game-layer",true);
            return;
        }
        left=std::isfinite(left)?std::clamp(left,180.f,float(w)*.25f):222;
        right=std::isfinite(right)?std::clamp(right,270.f,float(w)*.34f):326;
        bottom=std::isfinite(bottom)?std::clamp(bottom,180.f,float(h)*.52f):290;
        propertySplit=std::isfinite(propertySplit)?std::clamp(propertySplit,.25f,.7f):.48f;
        const float l=ui.showToolbox_?left+5:0,r=(ui.showExplorer_||ui.showProperties_)?right+5:0;
        constexpr float panelTop=144, statusHeight=23;
        const float usable=h-panelTop-statusHeight,exHeight=ui.showExplorer_&&ui.showProperties_?usable*propertySplit:usable;
        display("toolbox",ui.showToolbox_);display("explorer",ui.showExplorer_);display("properties",ui.showProperties_);display("viewport-label",true);
        rect("toolbox",{0,panelTop,left,usable});rect("explorer",{float(w)-right,panelTop,right,exHeight});
        rect("properties",{float(w)-right,ui.showExplorer_?panelTop+5+exHeight:panelTop,right,ui.showExplorer_?usable-exHeight-5:usable});
        style("toolbox-content","height",Num(usable-94)+"px");
        style("tree","height",Num(exHeight-63)+"px");
        const float propHeight=ui.showExplorer_?usable-exHeight-5:usable;
        style("property-body","height",Num(std::max(30.f,propHeight-91))+"px");
        rect("left-divider",{left,panelTop,5,usable});display("left-divider",ui.showToolbox_);
        rect("right-divider",{float(w)-right-5,panelTop,5,usable});display("right-divider",r>0);
        rect("property-divider",{float(w)-right,panelTop+exHeight,right,5});display("property-divider",ui.showExplorer_&&ui.showProperties_);
        const bool hasAdvanced=!advanced.empty()&&!ui.runtime_.active();
        const float bh=hasAdvanced?bottom+5:0;
        view={l,panelTop+26,std::max(16.f,w-l-r),std::max(16.f,h-panelTop-26-statusHeight-bh)};
        document->SetClass("player-compact",view.width<950);
        rect("viewport-label",{l,panelTop,view.width,26});
        rect("game-layer",view);display("game-layer",ui.runtime_.active());
        advancedRect={l,float(h)-statusHeight-bottom+29,view.width,bottom-29};
        rect("advanced",{l,float(h)-statusHeight-bottom,view.width,bottom});display("advanced",hasAdvanced);
        rect("bottom-divider",{l,float(h)-statusHeight-5-bottom,view.width,5});display("bottom-divider",hasAdvanced);
    }
    void showAdvanced(const std::string& name) {
        if(name!="assets"&&name!="audio"){previewAudio.stopAll();previewVoice=0;}
        if(name=="audio"){audioScan=-10;advancedMarkup.clear();}
        if(name=="script") {advanced.clear();advancedMarkup.clear();ui.showScriptEditor_=false;ui.openScriptEditor();return;}
        advanced=name;advancedMarkup.clear();ui.showScriptEditor_=false;ui.showAnimationEditor_=name=="animation";ui.showRigEditor_=name=="rig";ui.showGameSettings_=false;ui.showOutput_=name=="output";
        if(name=="animation") {auto& a=ui.scene_.gameAssets();if(!a.animations.empty())ui.openAnimation(a.findAnimation(a.player.walkAnimation)?a.player.walkAnimation:a.animations.front().id);}
    }
    void invalidateImages() {images.clear();imageScan=-10;propertyMarkup.clear();advancedMarkup.clear();}
    void invalidateAudio() {audioFiles.clear();audioScan=-10;advancedMarkup.clear();propertyMarkup.clear();}
    static std::string Lower(std::string text) {for(auto& c:text)c=char(std::tolower(static_cast<unsigned char>(c)));return text;}
    static std::string ImageTypeLabel(const std::string& filename) {
        auto ext=std::filesystem::path(filename).extension().string();
        if(ext.size()>1)ext=ext.substr(1);
        for(auto& c:ext)c=char(std::toupper(static_cast<unsigned char>(c)));
        return ext.empty()?"IMAGEM":ext;
    }
    bool ImageMatches(const game::ImageAsset& image) const {
        if(imageQuery.empty())return true;
        return Lower(image.name+" "+image.filename+" "+image.id).find(Lower(imageQuery))!=std::string::npos;
    }
    // Seletor compacto de imagem: miniatura, nome amigável, busca, importação e
    // remoção da imagem aplicada. O ID técnico aparece só no title do item; a
    // aplicação usa o mesmo comando do painel de recursos.
    std::string ImagePicker(const std::string& current,const std::string& apply,const std::string& clearValue,const std::string& queryId) {
        if(GetTime()-imageScan>2){images=game::ListImages(game::AssetsDir());imageScan=GetTime();}
        const auto assets=game::AssetsDir();
        const auto currentFile=std::filesystem::path(game::NormalizeImageReference(current)).filename().string();
        std::string s="<div class=\"image-picker\"><div class=\"image-current\">";
        if(current.empty())s+="<span class=\"image-current-name muted\">Nenhuma imagem aplicada</span>";
        else {
            const auto path=game::ResolveImagePath(assets,current);
            if(!path.empty())s+="<img class=\"image-current-thumb\" src=\""+Escape(std::filesystem::absolute(path).string())+"\"/>";
            s+="<span class=\"image-current-name\">"+Escape(game::ImageDisplayName(assets,current))+"</span>";
            if(path.empty())s+="<span class=\"image-missing\" title=\"O arquivo não está em assets/. Importe a imagem novamente ou remova a referência.\">Ausente</span>";
            s+=Button("clear-image","Remover imagem","delete",clearValue);
        }
        s+="</div><input id=\""+queryId+"\" class=\"image-query\" placeholder=\"Pesquisar imagem por nome\" value=\""+Escape(imageQuery)+"\"/>";
        s+="<div class=\"image-options\">";
        bool any=false;
        for(const auto& image:images) {
            if(!ImageMatches(image))continue;
            any=true;
            const auto path=std::filesystem::absolute(std::filesystem::path(assets)/image.filename).string();
            const bool selected=!currentFile.empty()&&currentFile==image.filename;
            s+=std::string("<button id=\"")+queryId+"-"+Escape(image.filename)+"\" class=\"image-option"+(selected?" selected":"")+"\" data-action=\""+apply+"\" data-value=\""+Escape(path)+"\" title=\""+Escape("ID: "+image.id)+"\"><img src=\""+Escape(path)+"\"/><span class=\"image-option-name\">"+Escape(image.name)+"</span><span class=\"image-option-type\">"+ImageTypeLabel(image.filename)+"</span></button>";
        }
        if(!any)s+=std::string("<div class=\"image-empty\">")+(images.empty()?"Nenhuma imagem importada.":"Nenhuma imagem encontrada.")+"</div>";
        s+="</div><div class=\"image-picker-actions\">"+Button("import-dialog","Importar imagem","folder","",queryId)+"</div></div>";
        return s;
    }
    void documents() {
        display("editor-camera-modes",!ui.runtime_.active()&&ui.activeDocument_==0);
        el("editor-camera-fps")->SetClass("active",ui.camera_.navigationMode()==EditorCamera::NavigationMode::FPS);
        el("editor-camera-focus")->SetClass("active",ui.camera_.navigationMode()==EditorCamera::NavigationMode::Focus);
        ui.pruneScriptDocuments();
        std::string s=std::string("<div class=\"document-tab ")+(ui.activeDocument_==0?"active":"")+"\" id=\"document-tab-0\"><button id=\"document-0\" class=\"doc-activate\" data-action=\"activate-document\" data-value=\"0\" title=\"3D place viewport\"><img src=\"icon-workspace\"/><span class=\"doc-label\">Place · Perspective</span></button></div>";
        for(const InstanceId id:ui.openDocuments_) {
            auto* node=ui.scene_.find(id);if(!node||!IsScriptClass(node->instanceClass()))continue;
            const auto it=ui.scriptDrafts_.find(id);const bool dirty=it!=ui.scriptDrafts_.end()&&it->second.dirty;
            const std::string sid=std::to_string(id);
            s+=std::string("<div class=\"document-tab ")+(ui.activeDocument_==id?"active":"")+"\" id=\"document-tab-"+sid+"\"><button id=\"document-"+sid+"\" class=\"doc-activate\" data-action=\"activate-document\" data-value=\""+sid+"\" title=\""+Escape(node->name())+"\"><img src=\"icon-script\"/><span class=\"doc-label\">"+Escape(node->name())+"</span>"+(dirty?"<span class=\"doc-dirty\" title=\"Unsaved changes\"/>":"")+"</button><button id=\"close-document-"+sid+"\" class=\"doc-close\" data-action=\"close-document\" data-value=\""+sid+"\" title=\"Close script\">×</button></div>";
        }
        html("document-list",s,documentMarkup);
        text("viewport-hint",ui.activeDocument_==0?"RMB + WASD · Q / E":"Ctrl+S save · Ctrl+W close");
    }
    void prompt(const std::string& kind) {
        if(auto* confirm=el("dialog-confirm")){confirm->SetClass("image-locked",false);confirm->RemoveAttribute("disabled");}
        if(experiences&&kind=="import-image"){experiences->chooseAsset();return;}
        if(experiences&&kind=="open"){experiences->enqueue("import");return;}
        if(experiences&&kind=="save-as"&&experiences->handlesSave()){experiences->enqueue("export-current");return;}
        auto* selected=ui.scene_.selection();
        if((kind=="color"||kind=="material")&&(!selected||!selected->isPart())){ui.setStatus("Selecione uma Peça primeiro.",true);return;}
        if(kind.rfind("color:",0)==0&&!selected)return;
        ui.endGizmoSession(true,"dialog");ui.cancelRigGizmo();ui.camera_.releaseCapture();dialog=kind;dialogOpen=true;
        text("dialog-title",kind=="rename"?"Renomear objeto":kind=="open"?"Abrir projeto":kind=="import-image"?"Importar imagem":kind=="import-sound"?"Importar áudio":"Salvar projeto como");
        text("dialog-description",kind=="rename"?"O nome pode ser alterado a qualquer momento; o ID estável do objeto é preservado.":kind=="import-image"?"PNG ou JPEG · qualquer proporção dentro do orçamento de memória de imagem. Original preservado.":kind=="import-sound"?"MP3, WAV ou OGG · o arquivo é copiado para a experiência; o original é preservado. Informe o caminho completo do arquivo.":"Arquivos de projeto usam o formato JSON transacional existente.");text("dialog-error","");
        auto* field=dynamic_cast<Rml::ElementFormControl*>(el("dialog-value"));
        const auto* node=ui.scene_.selection();const auto* rig=ui.scene_.gameAssets().findRig(ui.selectedRig_);
        std::string initial=kind=="rename"?(node?node->name():rig?rig->name:""):(kind=="import-image"||kind=="import-sound")?"":ui.projectPath_;
        el("appearance-controls")->SetInnerRML("");
        if(kind=="color"||kind.rfind("color:",0)==0) {
            const auto property=kind=="color"?std::string("Color"):kind.substr(6);
            dialog="property:"+property;initial=kind=="color"?Hex(static_cast<Part*>(selected)->color()):value(el("prop-"+property));text("dialog-title",property);text("dialog-description","Escolha uma cor ou digite #RRGGBB. A transparência é preservada.");
            std::string colors="<div class=\"palette\">";for(const char* color:{"#f2f2f2","#b0b3b9","#363a44","#111318","#e56e70","#ed964d","#f2d477","#78c195","#4f9dcc","#4969b7","#9a7ccc","#d69cba","#a57d59","#d5b482","#9bc2b8","#afc3d8"})colors+="<button data-action=\"choose-color\" data-value=\""+std::string(color)+"\" style=\"background-color:"+color+";\"/>";colors+="</div>";el("appearance-controls")->SetInnerRML(colors);
        }else if(kind=="material") {
            dialog="property:Material";initial=std::to_string(int(static_cast<Part*>(selected)->material()));text("dialog-title","Material da peça");text("dialog-description","Selecione um material compatível com a engine.");
            std::string options="<select data-action=\"choose-material\">";
            for(int i=0;i<PartMaterialCount();++i)options+="<option value=\""+std::to_string(i)+"\" "+(initial==std::to_string(i)?"selected=\"selected\"":"")+">"+PartMaterialName(PartMaterialFromIndex(i))+"</option>";
            el("appearance-controls")->SetInnerRML(options+"</select>");
        }
        field->SetDisabled(false);field->SetValue(initial);focusValue=initial;display("dialog-overlay",true,"flex");field->Focus();
    }
    void applyPlayerSetting(const std::string& setting,const std::string& v) {
        if(!ui.gameMenuOpen())return;
        auto next=ui.runtime_.settings();
        try {
            size_t count=0;
            auto number=[&]{const float result=std::stof(v,&count);if(count!=v.size()||!std::isfinite(result))throw std::runtime_error("Enter a finite number");return result;};
            if(setting=="cameraMode")next.cameraMode=static_cast<game::CameraMode>(int(number()));
            else if(setting=="sensitivity")next.sensitivity=number();
            else if(setting=="sensitivityPercent")next.sensitivity=.0001f+(number()-1.f)/99.f*.0299f;
            else if(setting=="cameraDistance")next.cameraDistance=number();
            else if(setting=="enableShiftLock")next.enableShiftLock=v=="true";
            else if(setting=="allowFirstPerson")next.allowFirstPerson=v=="true";
            else return;
            std::string error;if(!next.validate(error))throw std::runtime_error(error);
            ui.runtime_.settings()=next;
            if(setting=="cameraDistance")ui.runtime_.playerCamera().setDistance(next.cameraDistance);
            text("player-setting-error","");
        }catch(const std::exception& e){text("player-setting-error",e.what());}
    }
    void commit(Rml::Element* e) {
        if(!e)return;
        if(e->GetId().rfind("image-query",0)==0){imageQuery=value(e);propertyMarkup.clear();advancedMarkup.clear();return;}
        if(e->GetId()=="audio-search"){audioQuery=value(e);advancedMarkup.clear();return;}
        if(e->HasAttribute("data-rig-property")) {
            if(ui.runtime_.active())return;
            auto* rig=ui.scene_.gameAssets().findRig(ui.selectedRig_);
            auto* decal=rig&&selectedDecalRig==rig->id?rig->findDecal(selectedRigDecal):nullptr;if(!decal)return;
            auto next=*decal;const auto property=attr(e,"data-rig-property"),v=value(e);
            try {
                if(property=="Name")next.name=v;
                else if(property=="Image")next.surface.image=game::NormalizeImageReference(v);
                else if(property=="Face")next.surface.face=std::stoi(v);
                else if(property=="Transparency") {size_t n=0;next.surface.transparency=std::stof(v,&n);if(n!=v.size())throw std::runtime_error("Enter a finite transparency");}
                if(next.name.empty()||next.name.size()>128||!next.surface.valid())throw std::runtime_error("Invalid decal name, face or transparency (0..1)");
                *decal=std::move(next);treeHash=0;
            }catch(const std::exception& error){ui.setStatus(error.what(),true);}
            return;
        }
        const auto action=attr(e,"data-action"),property=attr(e,"data-property"),setting=attr(e,"data-setting");
        std::string v=value(e);if(attr(e,"type")=="checkbox")v=e->HasAttribute("checked")?"true":"false";
        if(action=="player-volume"){if(!ui.gameMenuOpen()||!IsAudioDeviceReady())return;try{SetMasterVolume(std::clamp(std::stof(v),0.f,1.f));}catch(...){}return;}
        if(action=="choose-material"){static_cast<Rml::ElementFormControl*>(el("dialog-value"))->SetValue(v);return;}
        if(!property.empty())commands.push_back({"property:"+property,v,e->GetAttribute<InstanceId>("data-node",0)});
        else if(!action.empty())commands.push_back({action,v});
        else if(!setting.empty())applyPlayerSetting(setting,v);
    }
    void ProcessEvent(Rml::Event& event)override {
        // Form controls emit change while their options/values are installed.
        // A presentation refresh is never an editor command.
        if(syncing)return;
        auto* target=event.GetTargetElement();const auto type=event.GetType();
        if(type=="click"&&experiences)if(auto* command=ancestor(target,"data-experience")) {
            experiences->enqueue(attr(command,"data-experience"),attr(command,"data-value"));event.StopPropagation();return;
        }
        if(type=="focus") {if(auto* form=formOwner(target))focusValue=value(form);return;}
        if(type=="blur") {if(auto* form=formOwner(target);form&&!restoreFocus&&value(form)!=focusValue)commit(form);restoreFocus=false;return;}
        if(type=="change") {
            if(target->GetId()=="tree-search") {search=value(target);treeHash=0;return;}
            if(target->GetId()=="property-search") {propertySearch=value(target);filterProperties();return;}
            if(target->GetId()=="tool-search"||target->GetId()=="tool-category") {filterTools();return;}
            if(target->GetId().rfind("image-query",0)==0) {imageQuery=value(target);propertyMarkup.clear();advancedMarkup.clear();return;}
            if(target->GetId()=="audio-search") {audioQuery=value(target);advancedMarkup.clear();return;}
            if(attr(target,"type")=="checkbox"||target->GetTagName()=="select"||attr(target,"type")=="range")commit(target);
            if(target->GetId()=="prop-Item.Prototype"){focusValue=value(target);target->Blur();propertyMarkup.clear();}
            return;
        }
        if(type=="mousedown") {
            if(experiences&&experiences->overlayActive()&&event.GetParameter<int>("button",-1)==1)
                if(auto* card=ancestor(target,"data-experience-card")){experiences->enqueue("card-menu",attr(card,"data-experience-card"));event.StopPropagation();return;}
            bool form=false;for(auto* e=target;e&&e!=document;e=e->GetParentNode())if(e->GetTagName()=="input"||e->GetTagName()=="select"||e->GetTagName()=="textarea"){form=true;break;}
            if(editing()&&!form)context->GetRootElement()->Focus();
            if(auto* handle=ancestor(target,"data-split")) {split=attr(handle,"data-split");mouseOwned=true;}
            if(auto* row=ancestor(target,"data-node-row")) {dragNode=row->GetAttribute<InstanceId>("data-node-row",0);dragStart=GetMousePosition();treeKeyboard=true;}
            if(ancestor(target,"data-rig"))treeKeyboard=true;
            return;
        }
        if(type=="dblclick") {
            if(auto* row=ancestor(target,"data-node-row")) {const auto id=row->GetAttribute<InstanceId>("data-node-row",0);if(auto* node=ui.scene_.find(id);node&&IsScriptClass(node->instanceClass()))commands.push_back({"open-script","",id});}
            else if(ancestor(target,"data-rig"))showAdvanced("rig");
            return;
        }
        if(type=="contextmenu")return;
        if(type!="click"||dragging)return;
        if(auto* row=ancestor(target,"data-rig-body")) {
            if(ui.runtime_.active())return;
            ui.selectRig(attr(row,"data-rig"),row->GetAttribute<int>("data-rig-body",-1));
            selectedRigDecal=attr(row,"data-rig-decal");selectedDecalRig=ui.selectedRig_;treeHash=0;return;
        }
        if(auto* button=ancestor(target,"data-player-page")) {
            if(!ui.runtime_.active())return;
            const auto chosen=attr(button,"data-player-page");
            if(ui.gameMenuOpen()&&page==chosen)commands.push_back({"resume",""});
            else {if(!ui.gameMenuOpen()){actions.dispatch({"pause",""});pauseWasOpen=true;}page=chosen;el("pause-content")->SetScrollTop(0);}
            return;
        }
        if(auto* button=ancestor(target,"data-player-step")) {
            const auto setting=attr(button,"data-player-step");const auto& p=ui.runtime_.settings();
            if(setting=="cameraMode") {
                const int mode=(int(p.cameraMode)+std::stoi(attr(button,"data-value"))+3)%3;
                applyPlayerSetting(setting,std::to_string(mode));
            }else if(setting=="enableShiftLock")applyPlayerSetting(setting,p.enableShiftLock?"false":"true");
            else if(setting=="allowFirstPerson")applyPlayerSetting(setting,p.allowFirstPerson?"false":"true");
            return;
        }
        if(auto* button=ancestor(target,"data-tab")){tab=attr(button,"data-tab");ribbonMarkup.clear();return;}
        if(auto* button=ancestor(target,"data-page")) {
            page=attr(button,"data-page");el("pause-content")->SetScrollTop(0);
            const Rml::Property from(.85f,Rml::Unit::NUMBER);
            el("pause-content")->Animate("opacity",Rml::Property(1.f,Rml::Unit::NUMBER),.09f,Rml::Tween{},1,false,0,&from);
            return;
        }
        if(auto* button=ancestor(target,"data-advanced")){showAdvanced(attr(button,"data-advanced"));return;}
        if(auto* fold=ancestor(target,"data-fold")) {auto id=fold->GetAttribute<InstanceId>("data-fold",0);if(collapsed.erase(id)==0)collapsed.insert(id);treeHash=0;return;}
        if(auto* rig=ancestor(target,"data-rig")) {selectedRigDecal.clear();commands.push_back({"rig-select",attr(rig,"data-rig")});return;}
        auto* button=ancestor(target,"data-action");
        if(!button) {if(auto* row=ancestor(target,"data-node-row"))commands.push_back({"select",ImGui::GetIO().KeyCtrl?"toggle":"",row->GetAttribute<InstanceId>("data-node-row",0)});return;}
        if(button->GetTagName()=="input"||button->GetTagName()=="select")return;
        const auto action=attr(button,"data-action"),v=attr(button,"data-value");
        if(action=="rig-decal-add"||action=="rig-decal-delete"||
            (action=="apply-image"&&!ui.scene_.selection()&&ui.selectedRigBody_>=0)) {
            if(ui.runtime_.active())return;
            auto* rig=ui.scene_.gameAssets().findRig(ui.selectedRig_);if(!rig)return;
            try {
                if(action=="rig-decal-delete"){rig->eraseDecal(selectedRigDecal);selectedRigDecal.clear();}
                else {
                    auto* decal=action=="apply-image"&&selectedDecalRig==rig->id?rig->findDecal(selectedRigDecal):nullptr;
                    if(!decal)decal=&rig->addDecal(ui.selectedRigBody_);
                    if(action=="apply-image")decal->surface.image=game::ImageAssetReference(v);
                    selectedRigDecal=decal->id;
                    selectedDecalRig=rig->id;
                }
                treeHash=0;
            }catch(const std::exception& error){ui.setStatus(error.what(),true);}
            contextOpen=false;display("context-menu",false);return;
        }
        if(contextOpen&&button->GetParentNode()==el("context-menu")){display("context-menu",false);contextOpen=false;}
        if(action=="rename"){prompt("rename");return;}
        if(action=="open-dialog"||action=="save-dialog"){prompt(action=="open-dialog"?"open":"save-as");return;}
        if(action=="import-dialog"){prompt("import-image");return;}
        if(action=="import-sound"){
            if(experiences){experiences->chooseAudio();return;}
            prompt("import-sound");return;
        }
        if(action=="clear-image") {
            if(ui.runtime_.active())return;
            if(v=="rig-body") {
                auto* rig=ui.scene_.gameAssets().findRig(ui.selectedRig_);if(!rig||ui.selectedRigBody_<0)return;
                rig->bodies[ui.selectedRigBody_].image.clear();ui.setStatus("Imagem removida do membro; rig e animações preservados.");
            }else if(v=="surface") {
                auto* object=ui.scene_.selection();if(!object||!IsSurfaceClass(object->instanceClass()))return;
                auto& surface=static_cast<Surface&>(*object);auto value=surface.properties();value.image.clear();surface.setProperties(value);
            }else if(v=="rig") {
                auto* rig=ui.scene_.gameAssets().findRig(ui.selectedRig_);
                auto* decal=rig&&selectedDecalRig==rig->id?rig->findDecal(selectedRigDecal):nullptr;
                if(!decal)return;
                decal->surface.image.clear();ui.setStatus("Imagem removida do adesivo; o arquivo continua na biblioteca.");
            } else {
                auto* part=ui.scene_.selection();if(!part||!part->isPart())return;
                // Remover o decal/textura da peça não apaga a imagem: o arquivo
                // permanece disponível para outros objetos.
                part->removeAttribute(v=="texture"?"texture_image":"decal_image");
                ui.setStatus(v=="texture"?"Textura removida da peça; a imagem continua na biblioteca.":"Adesivo removido da peça; a imagem continua na biblioteca.");
            }
            invalidateImages();treeHash=0;return;
        }
        if(action=="image-use"||action=="image-unlink") {
            if(ui.runtime_.active())return;
            const auto index=std::stoul(v);if(index>=imageUses.size())return;const auto use=imageUses[index];
            if(action=="image-unlink") {
                std::string error;const bool ok=use.kind=="presentation"?experiences&&experiences->clearPresentationImage(use,error):game::SetImageUsage(ui.scene_,use,"",error);
                ui.setStatus(ok?"Associação removida; a imagem continua na biblioteca.":error,!ok);
            }else if(!use.rig.empty()) {
                ui.selectRig(use.rig,use.body);selectedRigDecal=use.decal;selectedDecalRig=use.rig;
            }else if(use.object)ui.scene_.select(ui.scene_.find(use.object));
            else ui.setStatus("Use Substituir imagem ou Remover associação para alterar a apresentação.");
            dialogOpen=false;context->GetRootElement()->Focus();invalidateImages();treeHash=0;return;
        }
        if(action=="apply-body-image") {
            if(ui.runtime_.active())return;auto* rig=ui.scene_.gameAssets().findRig(ui.selectedRig_);
            if(!rig||ui.selectedRigBody_<0)return;
            if(game::ResolveImagePath(game::AssetsDir(),v).empty())return;
            rig->bodies[ui.selectedRigBody_].image=game::ImageAssetReference(v);invalidateImages();treeHash=0;return;
        }
        if(action=="preview-sound"||action=="pause-sound"||action=="stop-sound"){
            if(action=="stop-sound"){previewAudio.stopAll();previewVoice=0;}
            else if(action=="pause-sound"){
                if(!previewVoice||!previewAudio.hasVoice(previewVoice))ui.setStatus("Nenhuma prévia ativa.",true);
                else if(previewAudio.isPaused(previewVoice))previewAudio.resume(previewVoice);
                else previewAudio.pause(previewVoice);
            }
            else {previewAudio.stopAll();previewVoice=0;
                // O valor é um ID estável da biblioteca; só a reprodução usa a
                // referência assets/<arquivo>. Arquivos não catalogados chegam
                // como referência e continuam suportados pela prévia.
                const auto* sound=ui.scene_.gameAssets().findSound(v);
                const std::string reference=sound?sound->reference:(v.rfind("assets/",0)==0?v:std::string());
                if(reference.empty()){ui.setStatus("Áudio não encontrado na biblioteca.",true);return;}
                if(!previewAudio.deviceReady()){ui.setStatus("Nenhum dispositivo de áudio disponível para prévia.",true);return;}
                const auto path=std::filesystem::path(game::AssetsDir())/std::filesystem::path(reference).filename();
                if(!std::filesystem::is_regular_file(path)){ui.setStatus("Arquivo de áudio ausente: importe novamente.",true);return;}
                std::string error;
                previewVoice=previewAudio.play(reference,sound?sound->volume:1,sound?sound->loop:false,error);
                if(!previewVoice)ui.setStatus(error,true);else{previewRoot=game::ProjectAssetRoot();ui.setStatus("Prévia: "+(sound?sound->name:reference));}}
            return;
        }
        if(action=="catalog-sound"){
            if(ui.runtime_.active())return;
            const auto reference=game::NormalizeImageReference(v);
            const auto filename=std::filesystem::path(reference).filename().string();
            if(filename.empty()||!game::IsAudioReference(filename)){ui.setStatus("Arquivo de áudio inválido.",true);return;}
            if(auto* existing=ui.scene_.gameAssets().findSound(std::filesystem::path(filename).stem().string());existing&&game::NormalizeImageReference(existing->reference)==reference){
                ui.setStatus("Este áudio já está na biblioteca.",true);return;
            }
            double seconds=0;std::string error;
            const auto path=std::filesystem::path(game::AssetsDir())/filename;
            if(!game::ProbeAudio(path.string(),seconds,error)){ui.setStatus(error,true);return;}
            auto& sound=ui.scene_.gameAssets().addSound(reference,std::filesystem::path(filename).stem().string());
            sound.durationSeconds=seconds;invalidateAudio();ui.setStatus("Áudio adicionado à biblioteca: "+sound.name);
            return;
        }
        if(action=="script-examples"){
            const std::pair<const char*,const char*> examples[]={
                {"musica_de_fundo.lua","Música de fundo"},
                {"musica_de_fundo_playlist.lua","Playlist de música"},
                {"som_ao_tocar.lua","Som ao tocar"},
                {"som_interacao.lua","Som de interação"},
                {"som_arma.lua","Som de arma"},
                {"som_recarga.lua","Som de recarga"},
            };
            std::string menu;
            for(const auto& example:examples)
                menu+=Button("script-example",example.second,"script",example.first,"example");
            el("context-menu")->SetInnerRML(i18n::Markup(menu));
            auto b=bounds("script-examples");
            rect("context-menu",{std::min(b.x,float(w)-260),b.y+28,256,float(6+std::size(examples)*25)});
            display("context-menu",true);contextOpen=true;treeKeyboard=true;return;
        }
        if(action=="audio-examples"){
            // Reutiliza os exemplos já existentes em assets/examples/lua/.
            // Abre o popup no mesmo estilo do menu de exemplos de scripts.
            const std::pair<const char*,const char*> examples[]={
                {"musica_de_fundo.lua","Música de fundo · loop"},
                {"musica_de_fundo_playlist.lua","Playlist · sequência de músicas"},
                {"som_ao_tocar.lua","Som ao tocar uma Part"},
                {"som_interacao.lua","Som de interação"},
                {"som_arma.lua","Som de disparo (arma)"},
                {"som_recarga.lua","Som de recarga"},
            };
            std::string menu;
            for(const auto& example:examples)
                menu+=Button("script-example",example.second,"script",example.first,"example");
            el("context-menu")->SetInnerRML(i18n::Markup(menu));
            auto b=bounds("audio-examples");
            rect("context-menu",{std::min(b.x,float(w)-280),b.y+28,276,float(6+std::size(examples)*25)});
            display("context-menu",true);contextOpen=true;treeKeyboard=true;
            // Garante que o editor de script estará visível ao carregar o exemplo
            return;
        }
        if(action=="script-example"){
            commands.push_back({action,v});
            advanced="script";advancedMarkup.clear();dialogOpen=false;
            display("context-menu",false);contextOpen=false;return;
        }
        if(action=="audio-copy"||action=="audio-insert"){
            const auto* sound=ui.scene_.gameAssets().findSound(v);
            if(!sound){ui.setStatus("Áudio não encontrado.",true);return;}
            const std::string snippet="linblux.audio.play(\""+sound->id+"\", { volume = "+Num(sound->volume)+", loop = "+(sound->loop?"true":"false")+" })";
            if(action=="audio-copy"){
                SetClipboardText(snippet.c_str());
                ui.setStatus("Código copiado: "+sound->name+". Cole em um Script.");
                return;
            }
            if(ui.activeDocument_==0||ui.scriptDrafts_.find(ui.activeDocument_)==ui.scriptDrafts_.end()){
                actions.dispatch({"script",""});
            }
            auto it=ui.scriptDrafts_.find(ui.activeDocument_);
            if(it==ui.scriptDrafts_.end()){ui.setStatus("Abra ou crie um Script para inserir o código.",true);return;}
            auto& draft=it->second;
            if(draft.text.empty()&&draft.baseline.empty()){if(auto* node=ui.scene_.find(ui.activeDocument_);node&&IsScriptClass(node->instanceClass())){draft.text=static_cast<Script*>(node)->source;draft.baseline=draft.text;}}
            const int at=std::clamp(draft.cursor,0,int(draft.text.size()));
            std::string inserted=snippet;
            if(at>0&&draft.text[size_t(at-1)]!='\n')inserted="\n"+inserted;
            if(at<int(draft.text.size())&&draft.text[size_t(at)]!='\n')inserted+="\n";
            draft.undo.push_back({draft.text,draft.cursor});
            if(draft.undo.size()>200)draft.undo.erase(draft.undo.begin());
            draft.text.insert(size_t(at),inserted);draft.cursor=at+int(inserted.size());draft.pendingCursor=draft.cursor;
            draft.restoreFocus=true;draft.dirty=draft.text!=draft.baseline;
            ui.showScriptEditor_=true;ui.setStatus("Código inserido no Script "+ui.scene_.find(ui.activeDocument_)->name()+".");
            return;
        }
        if(action=="rename-sound"||action=="remove-sound"){
            previewAudio.stopAll();previewVoice=0;auto* sound=ui.scene_.gameAssets().findSound(v);if(!sound){ui.setStatus("Áudio sem metadados: importe novamente para catalogar.",true);return;}
            dialogOpen=true;text("dialog-error","");el("appearance-controls")->SetInnerRML("");auto* field=static_cast<Rml::ElementFormControlInput*>(el("dialog-value"));
            if(auto* button=el("dialog-confirm")){button->RemoveAttribute("disabled");button->SetClass("image-locked",false);}
            if(action=="rename-sound"){dialog="rename-sound:"+sound->id;text("dialog-title","Renomear áudio");text("dialog-description","O ID estável e o arquivo permanecem iguais.");field->SetDisabled(false);field->SetValue(sound->name);focusValue=sound->name;}
            else {dialog="remove-sound";text("dialog-title","Excluir áudio: "+sound->name);std::string message="O arquivo gerenciado será excluído do disco; o original externo será preservado. A experiência será salva antes da exclusão.";
                const auto uses=game::DescribeSoundUsage(ui.scene_,v);for(const auto& use:uses)message+="\nEm uso: "+use.where+" · "+use.property;
                text("dialog-description",message);field->SetDisabled(true);field->SetValue(v);if(!uses.empty())el("dialog-confirm")->SetAttribute("disabled","");}
            display("dialog-overlay",true,"flex");context->GetRootElement()->Focus();if(action=="rename-sound")field->Focus();return;
        }
        if(action=="rename-image"||action=="replace-image"||action=="delete-image") {
            if(ui.runtime_.active())return;
            ui.endGizmoSession(true,"dialog");ui.cancelRigGizmo();ui.camera_.releaseCapture();
            dialogOpen=true;text("dialog-error","");el("appearance-controls")->SetInnerRML("");
            auto* field=dynamic_cast<Rml::ElementFormControl*>(el("dialog-value"));
            const auto displayName=game::ImageDisplayName(game::AssetsDir(),v);
            if(action=="delete-image") {
                imageUses=game::DescribeImageUsage(ui.scene_,v);
                if(experiences){const auto presentation=experiences->presentationReferences(v);imageUses.insert(imageUses.end(),presentation.begin(),presentation.end());}
                std::string report=imageUses.empty()?"Nenhuma referência encontrada. A cena será salva e a imagem será excluída apenas desta experiência. O arquivo original não será apagado.":"A imagem está em uso. Localize ou remova as associações abaixo antes de excluir; remover associação preserva o arquivo.";
                std::string rows="<div class=\"image-usages\">";
                for(std::size_t i=0;i<imageUses.size();++i)rows+="<div class=\"image-usage\"><span>"+Escape(imageUses[i].where+" · "+imageUses[i].property)+"</span>"+Button("image-use","Localizar","focus",std::to_string(i),"usage")+Button("image-unlink","Remover associação","delete",std::to_string(i),"usage")+"</div>";
                rows+="</div>";el("appearance-controls")->SetInnerRML(i18n::Markup(rows));
                if(auto* confirm=el("dialog-confirm")){confirm->SetClass("image-locked",!imageUses.empty());if(!imageUses.empty())confirm->SetAttribute("disabled","");else confirm->RemoveAttribute("disabled");}
                dialog="delete-image:"+v;text("dialog-title","Excluir imagem");text("dialog-description",report);
                field->SetDisabled(true);field->SetValue(displayName);display("dialog-overlay",true,"flex");context->GetRootElement()->Focus();return;
            }
            field->SetDisabled(false);
            if(auto* confirm=el("dialog-confirm")){confirm->SetClass("image-locked",false);confirm->RemoveAttribute("disabled");}
            if(action=="rename-image") {
                dialog="rename-image:"+v;text("dialog-title","Renomear imagem");
                text("dialog-description","Só o nome exibido muda: o arquivo e o ID técnico continuam iguais, então nenhuma referência quebra. Nomes repetidos são permitidos.");
                field->SetValue(displayName);focusValue=displayName;
            } else {
                dialog="replace-image:"+v;text("dialog-title","Substituir imagem");
                text("dialog-description","Informe o caminho de um PNG/JPEG. Peças, adesivos, ícone, banner e rosto passam a usar a nova imagem; o arquivo antigo continua na biblioteca até ser excluído.");
                field->SetValue("");focusValue="";
            }
            display("dialog-overlay",true,"flex");field->Focus();return;
        }
        if(action=="color-dialog"||action=="material-dialog"){prompt(action=="color-dialog"?"color":"material");return;}
        if(action=="edit-color"){prompt("color:"+v);return;}
        if(action=="choose-color"){static_cast<Rml::ElementFormControl*>(el("dialog-value"))->SetValue(v);return;}
        if(action=="insert-menu") {std::string menu;for(const auto& item:std::vector<std::pair<std::string,std::string>>{{"part","Part"},{"folder","Pasta"},{"model","Modelo"},{"attachment","Attachment"},{"script","Script"},{"local-script","LocalScript"},{"module","Módulo"},{"r6","Personagem R6"},{"server-storage","ServerStorage"},{"replicated-storage","ReplicatedStorage"}})menu+=Button(item.first,item.second);el("context-menu")->SetInnerRML(i18n::Markup(menu));auto b=bounds("new-object");rect("context-menu",{b.x-176,b.y+23,196,262});display("context-menu",true);contextOpen=true;return;}
        if(action=="cancel-dialog") {dialogOpen=false;context->GetRootElement()->Focus();return;}
        if(action=="confirm-dialog") {const auto v=value(el("dialog-value"));const auto id=ui.scene_.selection()?ui.scene_.selection()->id():0;
            if(actions.dispatch({dialog,v,id})) {if(dialog=="import-sound"||dialog.rfind("rename-sound:",0)==0)invalidateAudio();if(dialog=="import-image"||dialog.rfind("rename-image:",0)==0||dialog.rfind("replace-image:",0)==0||dialog.rfind("delete-image:",0)==0)invalidateImages();dialogOpen=false;context->GetRootElement()->Focus();treeHash=0;}else text("dialog-error",ui.status_);return;}
        if(action=="close-advanced"){showAdvanced("");return;}
        if(action=="toggle-toolbox"){ui.showToolbox_=!ui.showToolbox_;saveLayout();return;}
        if(action=="toggle-explorer"){ui.showExplorer_=!ui.showExplorer_;saveLayout();return;}
        if(action=="toggle-properties"){ui.showProperties_=!ui.showProperties_;saveLayout();return;}
        if(action=="reset-layout"){left=204;right=292;bottom=330;propertySplit=.52f;ui.showToolbox_=ui.showExplorer_=ui.showProperties_=true;saveLayout();return;}
        if(action.rfind("show:",0)==0){showAdvanced(action.substr(5)=="assets"?"assets":action.substr(5));return;}
        commands.push_back({action,v,action=="open-script"?InstanceId(std::stoull(v)):0});display("context-menu",false);contextOpen=false;
    }
    void filterTools() {
        auto query=value(el("tool-search")),category=value(el("tool-category"));std::transform(query.begin(),query.end(),query.begin(),[](unsigned char c){return char(std::tolower(c));});
        auto* grid=el("object-grid");for(int i=0;i<grid->GetNumChildren();++i){auto* e=grid->GetChild(i);const bool match=(category=="all"||attr(e,"data-category")==category)&&attr(e,"data-search").find(query)!=std::string::npos;e->SetProperty("display",match?"inline-block":"none");}
    }
    void filterProperties() {
        std::string query=propertySearch;std::transform(query.begin(),query.end(),query.begin(),[](unsigned char c){return char(std::tolower(c));});
        auto* body=el("property-body");for(int i=0;i<body->GetNumChildren();++i){auto* row=body->GetChild(i);if(row->IsClassSet("property")){std::string name=attr(row,"data-property-name");std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return char(std::tolower(c));});row->SetProperty("display",query.empty()||name.find(query)!=std::string::npos?"flex":"none");}}
    }
    void ribbon() {
        for(const char* name:{"Home","Model","Avatar","Script","View","Game"})display(std::string("ribbon-")+name,tab==name,"flex");
        for(int i=0;i<el("tabs")->GetNumChildren();++i){auto* e=el("tabs")->GetChild(i);if(e->HasAttribute("data-tab"))e->SetClass("active",attr(e,"data-tab")==tab);}
        for(int i=0;i<4;++i)el("cmd-tool-"+std::to_string(i))->SetClass("active",i==int(ui.tool_));
        const bool active=ui.runtime_.active();
        el("show-collision-shapes")->SetClass("active",ui.showCollisionShapes_);
        if(active&&!el("play")->HasAttribute("disabled"))el("play")->SetAttribute("disabled","");else if(!active)el("play")->RemoveAttribute("disabled");
        if(!active&&!el("stop")->HasAttribute("disabled"))el("stop")->SetAttribute("disabled","");else if(active)el("stop")->RemoveAttribute("disabled");
        for(const char* name:{"script","animation","rig","settings","output","assets","audio"})el(std::string("advanced-")+name)->SetClass("active",advanced==name);
    }
    void tree() {
        size_t hash=ui.scene_.objects().size()+size_t(ui.scene_.selection()?ui.scene_.selection()->id():0)*399;
        for(auto id:ui.scene_.selectedIds())hash=hash*399+id;
        for(const auto& n:ui.scene_.objects())hash^=std::hash<std::string>{}(n->name())+(n->id()*911)+(n->parent()?n->parent()->id()*235:0);
        for(const auto& rig:ui.scene_.gameAssets().rigs) {
            hash^=std::hash<std::string>{}(rig.id+rig.name+ui.selectedRig_+selectedRigDecal+std::to_string(ui.selectedRigBody_));
            for(const auto& body:rig.bodies){hash^=std::hash<std::string>{}(body.image);for(const auto& d:body.decals)hash^=std::hash<std::string>{}(d.id+d.name);}
        }
        if(treeHash==hash && hash!=0)return;
        treeHash=hash;
        std::string s;auto matches=[&](const Instance* node){std::string name=node->name()+" "+node->className(),q=search;std::transform(name.begin(),name.end(),name.begin(),::tolower);std::transform(q.begin(),q.end(),q.begin(),::tolower);return q.empty()||name.find(q)!=std::string::npos;};
        std::function<bool(const Instance*)> relevant=[&](const Instance* n){if(matches(n))return true;for(auto* c:n->children())if(relevant(c))return true;return false;};
        std::function<void(const Instance*,int)> row=[&](const Instance* node,int depth){if(!search.empty()&&!relevant(node))return;
            const auto id=std::to_string(node->id());const auto type=node->instanceClass();
            const std::string icon=type==InstanceClass::Workspace?"workspace":type==InstanceClass::Lighting?"lighting":type==InstanceClass::Folder?"folder":type==InstanceClass::Model?"model":type==InstanceClass::ModuleScript?"module":type==InstanceClass::LocalScript?"local-script":type==InstanceClass::SpawnLocation?"spawn":IsSurfaceClass(type)?"image":IsScriptClass(type)?"script":IsRootServiceClass(type)?"storage":"part";
            s+="<div id=\"node-"+id+"\" class=\"tree-row"+(ui.scene_.selected(node->id())?" selected":"")+"\" data-node-row=\""+id+"\" title=\""+Escape(node->name()+" · "+node->className())+"\" style=\"padding-left:"+std::to_string(3+depth*14)+"px\">";
            if(!node->children().empty())s+="<button class=\"fold\" data-fold=\""+id+"\"><img src=\"icon-chevron-"+(collapsed.count(node->id())?"right":"down")+"\"/></button>";
            else s+="<span class=\"leaf\"/>";
            s+="<img src=\"icon-"+icon+"\"/><span class=\"node-name\">"+Escape(node->name())+"</span></div>";
            if(!collapsed.count(node->id())||!search.empty())for(auto* c:node->children())row(c,depth+1);
        };
        for(const auto& n:ui.scene_.objects())if(!n->parent())row(n.get(),0);
        for(const auto& rig:ui.scene_.gameAssets().rigs) {
            const auto rid=Escape(rig.id);const bool active=ui.selectedRig_==rig.id;
            s+="<div id=\"rig-"+rid+"\" class=\"tree-row"+std::string(active&&ui.selectedRigBody_<0?" selected":"")+"\" data-rig=\""+rid+"\"><span class=\"leaf\"/><img src=\"icon-rig\"/><span class=\"node-name\">"+Escape(rig.name)+"</span></div>";
            if(active)for(int b=0;b<game::kBodyCount;++b) {
                const auto attributes=" data-rig=\""+rid+"\" data-rig-body=\""+std::to_string(b)+"\"";
                s+="<div id=\"rig-body-"+rid+"-"+std::to_string(b)+"\" class=\"tree-row"+std::string(ui.selectedRigBody_==b&&selectedRigDecal.empty()?" selected":"")+"\""+attributes+" style=\"padding-left:25px\"><img src=\"icon-part\"/><span class=\"node-name\">"+Escape(game::BodyDisplayName(rig.bodies[b].name))+"</span></div>";
                if(!rig.bodies[b].image.empty())s+="<div id=\"rig-image-"+rid+"-"+std::to_string(b)+"\" class=\"tree-row\""+attributes+" style=\"padding-left:43px\"><img src=\"icon-image\"/><span class=\"node-name\">Imagem do membro</span></div>";
                for(const auto& d:rig.bodies[b].decals)s+="<div id=\"rig-decal-"+rid+"-"+Escape(d.id)+"\" class=\"tree-row"+std::string(ui.selectedRigBody_==b&&selectedRigDecal==d.id?" selected":"")+"\""+attributes+" data-rig-decal=\""+Escape(d.id)+"\" style=\"padding-left:43px\"><img src=\"icon-image\"/><span class=\"node-name\">"+Escape(d.name)+"</span></div>";
            }
        }
        html("tree",s,treeMarkup);
    }
    void properties() {
        // A field blur may change markup during a button's mouse-down. Keep
        // that DOM alive until mouse-up, otherwise its click is discarded.
        if(editing()||ImGui::IsMouseDown(0))return;
        auto* node=ui.scene_.selection();std::string s;
        if(!node)if(auto* rig=ui.scene_.gameAssets().findRig(ui.selectedRig_);rig&&ui.selectedRigBody_>=0) {
            auto& body=rig->bodies[ui.selectedRigBody_];
            if(selectedDecalRig!=rig->id)selectedRigDecal.clear();
            game::RigDecal* decal=nullptr;for(auto& candidate:body.decals)if(candidate.id==selectedRigDecal)decal=&candidate;
            if(!decal)selectedRigDecal.clear();
            text("property-title",decal?decal->name+" · Adesivo":game::BodyDisplayName(body.name)+" · corpo R6");
            auto field=[&](const char* key,const std::string& v){s+="<div class=\"property\"><label>"+std::string(key)+"</label><input id=\"rig-prop-"+key+"\" data-rig-property=\""+key+"\" value=\""+Escape(v)+"\"/></div>";};
            if(decal) {
                s=Button("rig-decal-add","Adicionar adesivo","image")+Button("import-dialog","Importar imagem","folder")+Button("show:assets","Recursos de imagem","image")+"<div class=\"property-group\">ADESIVO</div>";field("Name",decal->name);
                s+="<div class=\"property\"><label>Face</label><select id=\"rig-prop-Face\" data-rig-property=\"Face\">";
                const char* faces[]={"Frente (-Z)","Trás (+Z)","Esquerda (-X)","Direita (+X)","Topo (+Y)","Base (-Y)"};
                for(int i=0;i<6;++i)s+="<option value=\""+std::to_string(i)+"\" "+(decal->surface.face==i?"selected=\"selected\"":"")+">"+faces[i]+"</option>";
                s+="</select></div><div class=\"property\"><label>Imagem</label><span class=\"readonly\">"+Escape(decal->surface.image.empty()?"Nenhuma imagem":game::ImageDisplayName(game::AssetsDir(),decal->surface.image))+"</span></div>";
                s+=ImagePicker(decal->surface.image,"apply-image","rig","image-query");
                field("Transparency",Num(decal->surface.transparency));
                s+=Button("rig-decal-delete","Remover adesivo","delete")+Button("show:rig","Editar corpo","rig");
                s+="<div class=\"property-help\">ID: rig://"+Escape(rig->id)+"/"+Escape(decal->id)+"<br/>Membro: "+Escape(game::BodyDisplayName(body.name))+"</div>";
            }else s="<div class=\"property-help\">Selecione um adesivo para editar face e imagem. A frente é o eixo local -Z.</div>"+Button("rig-decal-add","Adicionar adesivo","image")+Button("import-dialog","Importar imagem","folder")+Button("show:assets","Recursos de imagem","image")+Button("show:rig","Editar corpo","rig");
            if(!decal)s+=ImagePicker(body.image,"apply-body-image","rig-body","image-query-body");
            html("property-body",s,propertyMarkup);return;
        }
        if(!node) {auto* rig=ui.scene_.gameAssets().findRig(ui.selectedRig_);text("property-title",rig?rig->name:"No selection");s=rig?Button("show:rig","Edit character","rig")+Button("show:animation","Animate","animation")+Button("rename","Rename")+Button("duplicate","Duplicate")+Button("delete","Delete"):"<div class=\"empty-state\">Select an object to edit properties.</div>";html("property-body",s,propertyMarkup);return;}
        const auto id=std::to_string(node->id());
        el("property-title")->SetInnerRML(Escape(node->name())+"<span>"+Escape(node->className())+" · #"+id+"</span>");
        auto group=[&](const std::string& name){s+="<div class=\"property-group\">"+name+"</div>";};
        auto input=[&](const std::string& name,const std::string& v,const std::string& caption=""){s+="<div class=\"property\" data-property-name=\""+name+"\"><label>"+Escape(caption.empty()?name:caption)+"</label><input id=\"prop-"+name+"\" title=\""+Escape(caption.empty()?name:caption)+" · Enter to commit / Esc to cancel\" data-node=\""+id+"\" data-property=\""+name+"\" value=\""+Escape(v)+"\"/>";
            if(v.size()==7&&v[0]=='#')s+="<button id=\"color-"+name+"\" class=\"color-button\" data-action=\"edit-color\" data-value=\""+name+"\" style=\"background-color:"+v+"\" title=\"Choose color\"/>";
            s+="</div>";
        };
        auto check=[&](const std::string& name,bool checked){s+="<div class=\"property\" data-property-name=\""+name+"\"><label>"+name+"</label><input id=\"prop-"+name+"\" type=\"checkbox\" data-node=\""+id+"\" data-property=\""+name+"\" "+(checked?"checked=\"checked\"":"")+"/></div>";};
        // Sons de objeto guardam a referência estável assets/<arquivo>, nunca
        // um caminho absoluto. O seletor mostra o nome amigável e envia o ID
        // da biblioteca para sound-assign; um valor ausente é preservado e
        // fica visível como "referência não catalogada".
        auto soundSelect=[&](const char* field,const char* caption) {
            std::string reference;
            const auto found=node->attributes().find(field);
            if(found!=node->attributes().end())if(const auto* text=std::get_if<std::string>(&found->second))reference=game::NormalizeImageReference(*text);
            const game::SoundAsset* selected=nullptr;
            if(!reference.empty())for(const auto& sound:ui.scene_.gameAssets().sounds)if(game::NormalizeImageReference(sound.reference)==reference){selected=&sound;break;}
            s+="<div class=\"property\" data-property-name=\""+std::string(field)+"\"><label>"+Escape(caption)+"</label><select id=\"prop-"+field+"\" data-action=\"sound-assign:"+field+"\"><option value=\"\">Nenhum</option>";
            for(const auto& sound:ui.scene_.gameAssets().sounds)
                s+="<option value=\""+Escape(sound.id)+"\" "+(selected==&sound?"selected=\"selected\"":"")+">"+Escape(sound.name+" · "+sound.filename)+"</option>";
            if(!reference.empty()&&!selected)s+="<option value=\"\" selected=\"selected\">Referência não catalogada · "+Escape(reference)+"</option>";
            s+="</select></div>";
        };
        group("DATA");input("Name",node->name());
        s+="<div class=\"property\" data-property-name=\"ClassName\"><label>ClassName</label><span class=\"readonly\">"+Escape(node->className())+"</span></div>";
        if(node->parent()){s+="<div class=\"property\" data-property-name=\"Parent\"><label>Parent</label><select data-property=\"Parent\" data-node=\""+id+"\">";for(const auto& parent:ui.scene_.objects())if(!IsScriptClass(parent->instanceClass()) && parent.get()!=node && !node->isAncestorOf(parent.get()))s+="<option value=\""+std::to_string(parent->id())+"\" "+(parent.get()==node->parent()?"selected=\"selected\"":"")+">"+Escape(parent->name())+"</option>";s+="</select></div>";}
        if(node->isPart()) {
            auto& p=*static_cast<Part*>(node);group("TRANSFORM");
            for(auto v:{std::pair<std::string,Vector3>{"Position",p.position()},{"Rotation",p.rotationEulerDegrees()},{"Size",p.size()}})
                for(int axis=0;axis<3;++axis)input(v.first+"."+std::to_string(axis),Num((&v.second.x)[axis]),v.first+" "+char('X'+axis));
            group("APPEARANCE");input("Color",Hex(p.color()));input("Transparency",Num(1-p.color().a/255.f));s+="<div class=\"property\" data-property-name=\"Material\"><label>Material</label><select data-property=\"Material\" data-node=\""+id+"\">";
            for(int i=0;i<PartMaterialCount();++i) {
                s+="<option value=\""+std::to_string(i)+"\" "+(i==int(p.material())?"selected=\"selected\"":"")+">"+PartMaterialName(PartMaterialFromIndex(i))+"</option>";
            }
            s+="</select></div>";group("SOM DO OBJETO");soundSelect("Sound","Som ao tocar");soundSelect("SoundInteract","Som de interação");group("PHYSICS");check("Anchored",p.anchored());check("CanCollide",p.canCollide());
            const auto& body=p.bodyProperties();input("Density",Num(body.density));input("Friction",Num(body.friction));
            input("Restitution",Num(body.restitution));input("LinearDamping",Num(body.linearDamping),"Linear damping");input("AngularDamping",Num(body.angularDamping),"Angular damping");
            group("SPAWN");
            const bool legacySpawn=p.instanceClass()==InstanceClass::SpawnLocation;
            const bool spawnEnabled=legacySpawn||game::IsSpawnPart(p);
            if(legacySpawn)s+="<div class=\"property\" data-property-name=\"Spawn\"><label>Spawn point</label><span class=\"readonly\">Legacy SpawnLocation</span></div>";
            else check("Spawn",spawnEnabled);
            if(spawnEnabled)input("SpawnPriority",Num(static_cast<float>(game::SpawnPriority(p))),"Spawn priority");
            s+="<div class=\"property-help\">Higher priority wins; ties use the oldest instance.</div>";
            group("SURFACES");s+=Button("show:assets","Importar / aplicar imagem","image");
            auto surface=[&](const char* prefix,const char* label,const char* applyAction,const char* queryId) {
                const std::string imageField=std::string(prefix)+"_image";
                std::string image;const auto imageIt=p.attributes().find(imageField);
                if(imageIt!=p.attributes().end())if(const auto* value=std::get_if<std::string>(&imageIt->second))image=*value;
                s+="<div class=\"property\" data-property-name=\""+imageField+"\"><label>"+std::string(label)+"</label><span class=\"readonly\">"+Escape(image.empty()?"Nenhuma imagem":game::ImageDisplayName(game::AssetsDir(),image))+"</span></div>";
                s+=ImagePicker(image,applyAction,prefix,queryId);
            };
            surface("decal","Adesivo","apply-image","image-query");
            surface("texture","Textura","apply-texture","image-query-texture");
            for(const char* prefix:{"decal","texture"}) {
                const std::string field=std::string(prefix)+"_face";
                const std::string caption=std::string(prefix)=="decal"?"Face do adesivo":"Face da textura";
                std::string face="Front";const auto it=p.attributes().find(field);if(it!=p.attributes().end())if(const auto* v=std::get_if<std::string>(&it->second))face=*v;
                s+="<div class=\"property\" data-property-name=\""+field+"\"><label title=\""+Escape(caption)+"\">"+Escape(caption)+"</label><select data-node=\""+id+"\" data-property=\""+field+"\">";
                for(const auto& option:std::vector<std::pair<const char*,const char*>>{{"Front","Frente"},{"Back","Trás"},{"Left","Esquerda"},{"Right","Direita"},{"Top","Topo"},{"Bottom","Base"}}) {
                    s+="<option value=\""+std::string(option.first)+"\" "+(face==option.first?"selected=\"selected\"":"")+">"+option.second+"</option>";
                }
                s+="</select></div>";
            }
            const std::pair<const char*,const char*> surfaceFields[]={
                {"decal_transparency","Transparência do adesivo"},{"decal_rotation","Rotação do adesivo"},
                {"texture_transparency","Transparência da textura"},
                {"texture_repeat_u","Repetição U"},{"texture_repeat_v","Repetição V"},
                {"texture_offset_u","Deslocamento U"},{"texture_offset_v","Deslocamento V"}};
            for(const auto& entry:surfaceFields) {
                std::string v=std::string(entry.first).find("repeat")!=std::string::npos?"1":"0";
                auto it=p.attributes().find(entry.first);if(it!=p.attributes().end()){if(auto* text=std::get_if<std::string>(&it->second))v=*text;else if(auto* number=std::get_if<double>(&it->second))v=Num(*number);}
                input(entry.first,v,entry.second);
            }
            s+=Button("attachment","Criar Attachment","model","","property");
        } else if(node->instanceClass()==InstanceClass::Model) {
            group("Item da experiência");const auto marker=node->attributes().find("_linblux_item_prototype");
            const bool prototype=marker!=node->attributes().end()&&std::holds_alternative<bool>(marker->second)&&std::get<bool>(marker->second);
            s+="<div class=\"property\"><label>Protótipo de item</label><input id=\"prop-Item.Prototype\" type=\"checkbox\" data-node=\""+id+"\" data-property=\"Item.Prototype\" "+(prototype?"checked=\"checked\"":"")+"/></div>";
            if(prototype){const auto type=node->attributes().find("item_type"),icon=node->attributes().find("item_icon");
                input("Item.Type",type!=node->attributes().end()&&std::holds_alternative<std::string>(type->second)?std::get<std::string>(type->second):"","Tipo de item");
                const auto path=icon!=node->attributes().end()&&std::holds_alternative<std::string>(icon->second)?std::get<std::string>(icon->second):"";
                s+=ImagePicker(path,"apply-item-icon","item","image-query-item")+Button("clear-item-icon","Remover ícone","delete","","item");
            }
            ModelFrame frame;std::string error;
            if(ui.scene_.modelFrame(node->id(),frame,error)){
                group("Transformação do Modelo");const auto angles=EulerDegreesZYXFromQuat(frame.rotation);
                for(int axis=0;axis<3;++axis){input("Model.Position."+std::to_string(axis),Num((&frame.position.x)[axis]),"Posição "+std::string(1,char('X'+axis)));
                    input("Model.Rotation."+std::to_string(axis),Num((&angles.x)[axis]),"Rotação "+std::string(1,char('X'+axis)));}
                input("Model.Scale",Num(frame.scale),"Escala uniforme");group("Pivô sem mover as peças");
                for(int axis=0;axis<3;++axis){input("Pivot.Position."+std::to_string(axis),Num((&frame.position.x)[axis]),"Pivô "+std::string(1,char('X'+axis)));}
                group("SOM DA ARMA");soundSelect("SoundFire","Som do disparo");soundSelect("SoundReload","Som de recarga");soundSelect("SoundInteract","Som de interação");
                s+=Button("attachment","Criar Attachment","model","","property")+Button("ungroup","Desagrupar modelo","model");
            }
        } else if(node->instanceClass()==InstanceClass::Attachment) {
            const auto& local=static_cast<const Attachment&>(*node).localFrame();const auto angles=EulerDegreesZYXFromQuat(local.rotation);
            group("Junta fixa · duas peças");
            const auto target=static_cast<const Attachment&>(*node).jointTarget();
            s+="<div class=\"property\"><label>Conectar a</label><select id=\"prop-Attachment.Link\" data-node=\""+id+"\" data-property=\"Attachment.Link\"><option value=\"0\""+(target?std::string():" selected=\"selected\"")+">Sem junta</option>";
            for(const auto& candidate:ui.scene_.objects())if(candidate->instanceClass()==InstanceClass::Attachment&&candidate.get()!=node&&candidate->parent()->isPart()&&candidate->parent()!=node->parent()){
                const auto cid=std::to_string(candidate->id());s+="<option value=\""+cid+"\""+(target==candidate->id()?" selected=\"selected\"":"")+">"+Escape(candidate->parent()->name()+" / "+candidate->name())+"</option>";
            }s+="</select></div>";
            group("Attachment · transformação local");
            for(int axis=0;axis<3;++axis){input("Attachment.Position."+std::to_string(axis),Num((&local.position.x)[axis]),"Posição local "+std::string(1,char('X'+axis)));
                input("Attachment.Rotation."+std::to_string(axis),Num((&angles.x)[axis]),"Rotação local "+std::string(1,char('X'+axis)));}
            ModelFrame world;std::string error;if(ui.scene_.attachmentFrame(node->id(),world,error)){
                group("Transformação global · somente leitura");
                s+="<div class=\"property\"><label>Posição global</label><span>"+Num(world.position.x)+", "+Num(world.position.y)+", "+Num(world.position.z)+"</span></div>";}
        } else if(IsSurfaceClass(node->instanceClass())) {
            const auto& surface=static_cast<const Surface&>(*node);const auto& value=surface.properties();group("Superfície");
            s+=ImagePicker(value.image,"apply-surface-image","surface","image-query-surface");
            input("Surface.Face",std::to_string(value.face),"Face (0–5)");input("Surface.Transparency",Num(value.transparency),"Transparência");
            input("Surface.Rotation",Num(value.rotation),"Rotação");input("Surface.OffsetU",Num(value.offsetU),"Deslocamento U");input("Surface.OffsetV",Num(value.offsetV),"Deslocamento V");
            input("Surface.TileU",Num(value.studsPerTileU),"Repetição U");input("Surface.TileV",Num(value.studsPerTileV),"Repetição V");
            s+=Button("delete","Remover componente","delete");
        } else if(node==ui.scene_.lighting()) {
            const auto* l=ui.scene_.lighting();group("PROCEDURAL ATMOSPHERE");check("SkyEnabled",l->skyEnabled());input("Brightness",Num(l->brightness()));input("SunIntensity",Num(l->sunIntensity()));
            input("CloudCoverage",Num(l->cloudDensity()));input("SkyRotation",Num(l->skyRotation()));input("FogDensity",Num(l->fogDensity()));
            input("SkyZenithColor",Hex(l->skyZenithColor()));input("SkyHorizonColor",Hex(l->skyHorizonColor()));input("SkyGroundColor",Hex(l->skyGroundColor()));
            input("SunColor",Hex(l->sunColor()));input("SunSize",Num(l->sunSize()));input("FogColor",Hex(l->fogColor()));
            input("CloudScale",Num(l->cloudScale()));input("CloudSpeed",Num(l->cloudSpeed()));input("CloudSoftness",Num(l->cloudSoftness()));
            for(int axis=0;axis<3;++axis){const auto direction=l->sunDirection();input("SunDirection."+std::to_string(axis),Num((&direction.x)[axis]),"Sun direction "+std::string(1,char('X'+axis)));}
        }
        else if(node==ui.scene_.workspace()){group("WORLD");input("VoidHeight",Num(ui.scene_.workspace()->fallenPartsDestroyHeight()));}
        else if(IsScriptClass(node->instanceClass())) {group("SCRIPT");check("Enabled",static_cast<Script*>(node)->enabled);s+=Button("open-script","Open source","script",id);}
        bool attributesOpen=false;
        for(const auto& a:node->attributes()) {
            if(a.first=="spawn"||a.first=="Spawn"||a.first=="spawn_priority"||a.first=="SpawnPriority")continue;
            // Imagens de superfície têm seletor visual próprio; não repetir o
            // caminho técnico como campo de texto avançado.
            if(a.first.rfind("decal_",0)==0||a.first.rfind("texture_",0)==0)continue;
            // Sons têm seletor visual próprio por campo estável.
            if(a.first=="Sound"||a.first=="SoundFire"||a.first=="SoundReload"||a.first=="SoundInteract")continue;
            if(!attributesOpen){group("ATTRIBUTES");attributesOpen=true;}
            std::string v;
            if(const auto* text=std::get_if<std::string>(&a.second))v=*text;
            else if(const auto* number=std::get_if<double>(&a.second))v=Num(*number);
            else if(const auto* flag=std::get_if<bool>(&a.second))v=*flag?"true":"false";
            input("Attribute:"+a.first,v,a.first);
        }
        s+=Button("focus","Focus object","focus");html("property-body",s,propertyMarkup);filterProperties();
    }
    void input() {
        escapeConsumed=false;keyboardOwned=editing()||dialogOpen;const auto& io=ImGui::GetIO();const Vector2 mouse{io.MousePos.x,io.MousePos.y};
        mouseOwned=false;
        if(ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopup)){context->ProcessMouseLeave();mouseOwned=true;return;}
        if(!IsWindowFocused()){context->ProcessMouseLeave();context->ProcessMouseButtonUp(0,0);context->ProcessMouseButtonUp(1,0);split.clear();dragNode=0;dragging=false;return;}
        if(!editing()&&!dialogOpen&&!ui.runtime_.active()&&io.KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_G,false)){
            commands.push_back({io.KeyShift?"ungroup":"group",""});keyboardOwned=true;}
        const bool capture=ui.gameplayCapture_||ui.camera_.cursorCaptured();
        if(!capture)context->ProcessMouseMove(int(mouse.x*dpi),int(mouse.y*dpi),0);
        auto* hover=context->GetHoverElement();bool controls=ancestor(hover,"data-action")||ancestor(hover,"data-experience")||ancestor(hover,"data-page")||ancestor(hover,"data-player-page")||ancestor(hover,"data-player-step")||ancestor(hover,"data-split");
        const bool transforming=ui.transform_.active()||ui.rigGizmo_.dragging();
        mouseOwned=!capture&&(dialogOpen || (!transforming&&(!Within(mouse,view) || (ui.gameMenuOpen()) || controls || dragging || !split.empty())));
        if(experiences&&experiences->blocksStudioInput()){mouseOwned=true;keyboardOwned=true;}
        const int mods=(io.KeyCtrl?Rml::Input::KM_CTRL:0)|(io.KeyShift?Rml::Input::KM_SHIFT:0)|(io.KeyAlt?Rml::Input::KM_ALT:0);
        if(!capture) {
            if(ImGui::IsMouseClicked(0)) {
                // RML controls and embedded editors share one keyboard owner.
                // Clicking empty world space or an embedded editor blurs RML.
                if(editing()&&(Within(mouse,advancedRect)||Within(mouse,view))&&!controls)context->GetRootElement()->Focus();
                if(Within(mouse,view)||Within(mouse,advancedRect))treeKeyboard=false;
                if(contextOpen&&!Within(mouse,bounds("context-menu"))){contextOpen=false;display("context-menu",false);}
                context->ProcessMouseButtonDown(0,mods);
            }
            if(ImGui::IsMouseClicked(1)) {
                auto* row=ancestor(hover,"data-node-row");auto* rig=ancestor(hover,"data-rig");
                if(rig&&rig->HasAttribute("data-rig-body")&&!ui.runtime_.active()) {
                    ui.selectRig(attr(rig,"data-rig"),rig->GetAttribute<int>("data-rig-body",-1));
                    selectedRigDecal=attr(rig,"data-rig-decal");selectedDecalRig=ui.selectedRig_;
                    std::string menu=Button("rig-decal-add","Add Decal","image","","context");
                    if(!selectedRigDecal.empty())menu+=Button("rig-decal-delete","Delete Decal","delete","","context");
                    el("context-menu")->SetInnerRML(i18n::Markup(menu));rect("context-menu",{std::min(mouse.x,float(w)-190),std::min(mouse.y,float(h)-80),184,60});
                    display("context-menu",true);contextOpen=true;treeKeyboard=true;rig=nullptr;
                }
                if(row||rig) {
                    if(row){const auto id=row->GetAttribute<InstanceId>("data-node-row",0);if(!ui.scene_.selected(id))actions.dispatch({"select","",id});}else actions.dispatch({"rig-select",attr(rig,"data-rig")});
                    std::string menu;for(const auto* cmd:{"rename","duplicate","copy","paste","group","ungroup","folder","model","script","delete","focus"})if(row||std::string(cmd)=="rename"||std::string(cmd)=="duplicate"||std::string(cmd)=="delete"||std::string(cmd)=="focus")menu+=Button(cmd,std::string(cmd)=="group"?"Agrupar seleção":std::string(cmd)=="ungroup"?"Desagrupar modelo":cmd,"","","context");
                    el("context-menu")->SetInnerRML(i18n::Markup(menu));rect("context-menu",{std::min(mouse.x,float(w)-190),std::max(4.f,std::min(mouse.y,float(h)-285)),184,row?281.f:106.f});display("context-menu",true);contextOpen=true;treeKeyboard=true;
                }
            }
            if(io.MouseWheel!=0 && mouseOwned)context->ProcessMouseWheel(-io.MouseWheel,mods);
        }
        if(!split.empty()&&ImGui::IsMouseDown(0)){if(split=="left")left=mouse.x;else if(split=="right")right=w-mouse.x;else if(split=="properties")propertySplit=(mouse.y-144)/std::max(1.f,h-167.f);else bottom=h-23-mouse.y;}
        if(dragNode && ImGui::IsMouseDown(0) && Vector2Distance(mouse,dragStart)>7)dragging=true;
        if(dragging) {
            auto* target=ancestor(hover,"data-node-row");const auto id=target?target->GetAttribute<InstanceId>("data-node-row",0):0;
            if(id!=dragTarget){if(auto* old=el("node-"+std::to_string(dragTarget)))old->SetClass("drop-target",false);dragTarget=id;if(target)target->SetClass("drop-target",true);}
        }
        if(ImGui::IsMouseReleased(0)) {
            if(dragging) {if(auto* row=ancestor(hover,"data-node-row")){const auto target=row->GetAttribute<InstanceId>("data-node-row",0);if(target!=dragNode)actions.dispatch({"reparent",std::to_string(target),dragNode});}}
            context->ProcessMouseButtonUp(0,mods);if(!split.empty())saveLayout();split.clear();dragNode=0;dragging=false;
            if(auto* target=el("node-"+std::to_string(dragTarget)))target->SetClass("drop-target",false);
            dragTarget=0;
        }
        auto* focused=context->GetFocusElement();
        if(auto* form=formOwner(focused))focused=form;
        keyboardOwned=keyboardOwned||editing()||dialogOpen;
        // Player buttons participate in RmlUi's native Tab/Enter navigation.
        // Only the paused context receives these events; gameplay gets none.
        const bool playerPaused=ui.gameMenuOpen();
        if(playerPaused) {
            if(ImGui::IsKeyPressed(ImGuiKey_Tab,false)) {
                // Trap navigation in the pause subtree instead of reaching
                // Studio controls that remain visible around a docked game.
                Rml::ElementList candidates;
                std::function<void(Rml::Element*)> collect=[&](Rml::Element* e){
                    if(!e->IsVisible(true))return;
                    const auto& tag=e->GetTagName();
                    if((tag=="button"||tag=="input")&&!e->HasAttribute("disabled"))candidates.push_back(e);
                    for(int i=0;i<e->GetNumChildren();++i)collect(e->GetChild(i));
                };
                collect(el("pause-panel"));
                if(!candidates.empty()) {
                    auto it=std::find(candidates.begin(),candidates.end(),focused);
                    int index=it==candidates.end()?(io.KeyShift?0:-1):int(it-candidates.begin());
                    index=(index+(io.KeyShift?-1:1)+int(candidates.size()))%int(candidates.size());
                    candidates[index]->Focus();candidates[index]->ScrollIntoView();
                    if(ui.traceInput_)std::fprintf(stderr,"Player Tab: from=%s to=%s focused=%s\n",focused?focused->GetId().c_str():"none",candidates[index]->GetId().c_str(),context->GetFocusElement()?context->GetFocusElement()->GetId().c_str():"none");
                }
            }
            constexpr std::pair<ImGuiKey,Rml::Input::KeyIdentifier> activationKeys[]={{ImGuiKey_Enter,Rml::Input::KI_RETURN},{ImGuiKey_Space,Rml::Input::KI_SPACE}};
            for(const auto& key:activationKeys)if(!editing()) {
                if(ImGui::IsKeyPressed(key.first,false)) {
                    keyboardOwned=true; // Space used to Resume must not also queue a jump.
                    if(ui.traceInput_)std::fprintf(stderr,"Player activate: focus=%s key=%d\n",context->GetFocusElement()?context->GetFocusElement()->GetId().c_str():"none",int(key.second));
                    // Activate the focused control once through the same RML
                    // click path as the mouse. Do not also send a default
                    // document action that could activate it a second time.
                    if(auto* button=context->GetFocusElement();button&&button->GetTagName()=="button"&&!button->HasAttribute("disabled"))button->Click();
                }
            }
        }
        if(contextOpen&&ImGui::IsKeyPressed(ImGuiKey_Escape,false)){contextOpen=false;display("context-menu",false);escapeConsumed=true;}
        if(editing()) {
            for(auto c:io.InputQueueCharacters)context->ProcessTextInput(Rml::Character(c));
            for(int c=GetCharPressed();c;c=GetCharPressed())context->ProcessTextInput(Rml::Character(c));
            // Selects need vertical keyboard navigation too; keeping their
            // events here prevents arrows leaking into viewport navigation.
            for(const auto& key:{std::pair{ImGuiKey_UpArrow,Rml::Input::KI_UP},std::pair{ImGuiKey_DownArrow,Rml::Input::KI_DOWN}}) {
                if(ImGui::IsKeyPressed(key.first))context->ProcessKeyDown(key.second,mods);
                if(ImGui::IsKeyReleased(key.first))context->ProcessKeyUp(key.second,mods);
            }
            const std::pair<ImGuiKey,Rml::Input::KeyIdentifier> keys[]={{ImGuiKey_Backspace,Rml::Input::KI_BACK},{ImGuiKey_Delete,Rml::Input::KI_DELETE},{ImGuiKey_LeftArrow,Rml::Input::KI_LEFT},{ImGuiKey_RightArrow,Rml::Input::KI_RIGHT},{ImGuiKey_Home,Rml::Input::KI_HOME},{ImGuiKey_End,Rml::Input::KI_END},{ImGuiKey_Tab,Rml::Input::KI_TAB},{ImGuiKey_A,Rml::Input::KI_A},{ImGuiKey_C,Rml::Input::KI_C},{ImGuiKey_V,Rml::Input::KI_V},{ImGuiKey_X,Rml::Input::KI_X},{ImGuiKey_Z,Rml::Input::KI_Z},{ImGuiKey_Y,Rml::Input::KI_Y}};
            for(auto key:keys){if(playerPaused&&key.first==ImGuiKey_Tab)continue;if(ImGui::IsKeyPressed(key.first))context->ProcessKeyDown(key.second,mods);if(ImGui::IsKeyReleased(key.first))context->ProcessKeyUp(key.second,mods);}
            if(ImGui::IsKeyPressed(ImGuiKey_Enter,false)){if(dialogOpen) {EditorCommand c{dialog,value(el("dialog-value")),ui.scene_.selection()?ui.scene_.selection()->id():0};if(actions.dispatch(c)){if(dialog=="import-sound"||dialog.rfind("rename-sound:",0)==0)invalidateAudio();if(dialog=="import-image"||dialog.rfind("rename-image:",0)==0||dialog.rfind("replace-image:",0)==0||dialog.rfind("delete-image:",0)==0)invalidateImages();dialogOpen=false;}else text("dialog-error",ui.status_);}else {commit(focused);focusValue=value(focused);}if(auto* select=dynamic_cast<Rml::ElementFormControlSelect*>(focused))select->HideSelectBox();if(context->GetFocusElement()!=focused)focused->Focus();focused->Blur();}
            if(ImGui::IsKeyPressed(ImGuiKey_Escape,false)) {
                if(playerPaused&&attr(focused,"type")=="range") {
                    // A slider is live, not an unfinished text edit. Escape
                    // closes the pause menu in one press after dragging it.
                    focused->Blur();commands.push_back({"resume",""});
                }else {
                    if(auto* form=dynamic_cast<Rml::ElementFormControl*>(focused))form->SetValue(focusValue);
                    restoreFocus=true;focused->Blur();if(dialogOpen)dialogOpen=false;
                }
                escapeConsumed=true;
            }
        } else if(dialogOpen && ImGui::IsKeyPressed(ImGuiKey_Escape,false)){dialogOpen=false;escapeConsumed=true;}
        if(treeKeyboard&&!editing()&&!dialogOpen&&!ui.runtime_.active()) {
            if(ImGui::IsKeyPressed(ImGuiKey_F2,false)) {if(!ui.scene_.selection()&&!selectedRigDecal.empty()){if(auto* name=el("rig-prop-Name"))name->Focus();}else prompt("rename");}
            if(ImGui::IsKeyPressed(ImGuiKey_Delete,false)) {
                if(!ui.scene_.selection()&&ui.selectedRigBody_>=0) {
                    if(auto* rig=ui.scene_.gameAssets().findRig(ui.selectedRig_);rig&&selectedDecalRig==rig->id)rig->eraseDecal(selectedRigDecal);
                    selectedRigDecal.clear();treeHash=0;
                }else commands.push_back({"delete",""});
            }
            if(io.KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_D,false))commands.push_back({"duplicate",""});
            if(io.KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_C,false))commands.push_back({"copy",""});
            if(io.KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_V,false))commands.push_back({"paste",""});
        }
        // Dispatch may blur a field and enqueue its commit. Drain by index so
        // a blur cannot invalidate an iterator while an action changes focus.
        bool refreshImages=false;
        for(size_t i=0;i<commands.size();++i){const auto c=commands[i];actions.dispatch(c);
            if(previewVoice&&(c.action.rfind("sound-volume:",0)==0||c.action.rfind("sound-loop:",0)==0)){
                const bool volume=c.action.rfind("sound-volume:",0)==0;const auto* sound=ui.scene_.gameAssets().findSound(c.action.substr(volume?13:11));
                if(sound&&sound->reference==previewAudio.reference(previewVoice)){std::string error;if(volume)previewAudio.setVolume(previewVoice,sound->volume,error);else previewAudio.setLoop(previewVoice,sound->loop);}}
if(c.action=="r6")showAdvanced("rig");
            refreshImages|=c.action=="apply-item-icon"||c.action=="clear-item-icon"||c.action=="apply-surface-image"||c.action.rfind("apply-image",0)==0||c.action.rfind("apply-texture",0)==0||c.action=="remove-image"||c.action.rfind("delete-image:",0)==0||c.action.rfind("replace-image:",0)==0||c.action.rfind("rename-image:",0)==0;}
        commands.clear();if(refreshImages)invalidateImages();
    }
    void updateTooltip() {
        auto* owner=ancestor(context->GetHoverElement(),"title");const std::string label=attr(owner,"title");
        if(label!=tooltip){tooltip=label;tooltipSince=GetTime();}
        const bool show=!tooltip.empty()&&GetTime()-tooltipSince>.6&&!dialogOpen&&!ui.gameplayCapture_&&!ui.camera_.cursorCaptured()&&!ImGui::IsMouseDown(0);
        display("tooltip",show);if(!show)return;
        text("tooltip",tooltip);const auto mouse=GetMousePosition();
        style("tooltip","left",Num(std::clamp(mouse.x+12,4.f,float(w)-325))+"px");style("tooltip","top",Num(std::min(mouse.y+20,float(h)-40))+"px");
    }
    // Página Áudio do Studio. A biblioteca é a mesma GameAssets.sounds usada
    // pelo runtime e pelo Lua: aqui só há apresentação, busca e comandos
    // estáveis. Nenhum catálogo paralelo é criado.
    void audioPanel() {
        auto& assets=ui.scene_.gameAssets();
        if(GetTime()-audioScan>2){audioFiles=game::ListAudioAssets(game::AssetsDir());audioScan=GetTime();}
        const auto& music=assets.music;
        auto lower=[](std::string text){for(auto& c:text)c=char(std::tolower(static_cast<unsigned char>(c)));return text;};
        auto find=[&](const std::string& id)->const game::SoundAsset*{return assets.findSound(id);};
        auto displayName=[&](const std::string& id){const auto* sound=find(id);return sound?sound->name:std::string();};
        auto button=[&](const std::string& id,const std::string& action,const std::string& label,const std::string& icon,const std::string& value){
            std::string markup="<button id=\""+Escape(id)+"\" data-action=\""+Escape(action)+"\" data-value=\""+Escape(value)+"\">";
            if(!icon.empty())markup+="<img src=\"icon-"+Escape(icon)+"\"/>";
            return markup+Escape(i18n::Text(label))+"</button>";
        };
        auto matches=[&](const game::SoundAsset& sound){return audioQuery.empty()||lower(sound.name+" "+sound.id+" "+sound.filename+" "+sound.kind).find(lower(audioQuery))!=std::string::npos;};
        auto knownReference=[&](const std::string& reference){const auto canonical=game::NormalizeImageReference(reference);for(const auto& sound:assets.sounds)if(game::NormalizeImageReference(sound.reference)==canonical)return true;return false;};
        std::string s;
        s+="<div class=\"audio-heading\"><div><h2>Áudio da experiência</h2><p class=\"muted\">Importe MP3, WAV ou OGG. O arquivo é copiado para <code>assets/</code>; o nome amigável pode mudar sem quebrar scripts ou referências.</p></div>";
        s+=button("audio-import","import-sound","Importar áudio","folder","");
        s+=button("audio-examples","audio-examples","Exemplos Lua de áudio","script","");
        s+="</div><div class=\"audio-toolbar\"><input id=\"audio-search\" class=\"audio-search\" placeholder=\"Pesquisar por nome, arquivo ou tipo\" value=\""+Escape(audioQuery)+"\"/></div>";
        // Música declarativa: aparece antes da biblioteca para que a faixa de
        // fundo e a playlist fiquem visíveis sem rolar a lista inteira.
        s+="<div class=\"music-panel\"><h3>Música da experiência</h3><p class=\"muted\">A música começa somente no Play. Quando a playlist tem faixas, ela tem precedência sobre a música de fundo. Faixas ausentes são ignoradas com aviso, sem impedir o jogo.</p><div class=\"music-grid\">";
        s+="<label class=\"music-field\"><span>Música de fundo</span><select id=\"music-background\" data-action=\"music-background\"><option value=\"\">Nenhuma</option>";
        bool backgroundKnown=music.background.empty();
        for(const auto& sound:assets.sounds){if(sound.id==music.background)backgroundKnown=true;s+="<option value=\""+Escape(sound.id)+"\" "+(sound.id==music.background?"selected=\"selected\"":"")+">"+Escape(sound.name+" · "+sound.filename)+"</option>";}
        if(!backgroundKnown)s+="<option value=\""+Escape(music.background)+"\" selected=\"selected\">Faixa ausente · "+Escape(music.background)+"</option>";
        s+="</select></label>";
        s+="<label class=\"music-field\"><span>Volume da música</span><input id=\"music-volume\" type=\"range\" min=\"0\" max=\"1\" step=\"0.05\" value=\""+Num(music.volume)+"\" data-action=\"music-volume\"/></label>";
        s+="<label class=\"music-field\"><span>Repetir fundo</span><input id=\"music-loop\" type=\"checkbox\" data-action=\"music-loop\" "+(music.loop?std::string("checked=\"checked\""):std::string())+"/></label>";
        s+="<label class=\"music-field\"><span>Repetir playlist</span><input id=\"music-repeat\" type=\"checkbox\" data-action=\"music-repeat\" "+(music.repeat?std::string("checked=\"checked\""):std::string())+"/></label>";
        s+="</div><div class=\"playlist\"><div class=\"playlist-heading\"><b>Playlist</b><span>"+std::to_string(music.playlist.size())+" faixa(s) · uma por vez</span></div>";
        if(music.playlist.empty())s+="<div class=\"playlist-empty\">Nenhuma faixa na playlist. Use <b>Playlist</b> nos cards abaixo.</div>";
        for(std::size_t i=0;i<music.playlist.size();++i){
            const auto id=music.playlist[i];const auto* sound=find(id);
            s+="<div class=\"playlist-row\"><span class=\"playlist-index\">"+std::to_string(i+1)+"</span><span class=\"playlist-track"+(sound?"":" invalid")+"\">"+(sound?Escape(sound->name):"Faixa ausente · "+Escape(id))+"</span><span class=\"playlist-actions\">";
            s+=button("music-playlist-up-"+std::to_string(i),"music-playlist-up","↑","",std::to_string(i));
            s+=button("music-playlist-down-"+std::to_string(i),"music-playlist-down","↓","",std::to_string(i));
            s+=button("music-playlist-remove-"+std::to_string(i),"music-playlist-remove","×","",std::to_string(i));
            s+="</span></div>";
        }
        if(!music.playlist.empty())s+=button("music-playlist-clear","music-playlist-clear","Limpar playlist","delete","");
        s+="</div></div>";
        s+="<h3>Sons e músicas importados</h3>";
        if(assets.sounds.empty())s+="<div class=\"audio-empty\">Nenhum áudio na experiência. Importe um MP3, WAV ou OGG para tocar por Lua ou usar como música de fundo.</div>";
        s+="<div class=\"audio-grid\">";
        std::size_t shown=0;
        for(const auto& sound:assets.sounds){
            if(!matches(sound))continue;
            ++shown;
            const auto reference=game::NormalizeImageReference(sound.reference);
            const auto path=std::filesystem::path(game::AssetsDir())/std::filesystem::path(reference).filename();
            const bool missing=reference.empty()||!std::filesystem::is_regular_file(path);
            const bool isMusic=sound.kind=="music";
            const std::size_t uses=game::DescribeSoundUsage(ui.scene_,reference).size();
            s+="<div id=\"audio-card-"+Escape(sound.id)+"\" class=\"audio-card\"><div class=\"audio-card-header\"><span class=\"audio-name\" title=\""+Escape(sound.name)+"\">"+Escape(sound.name)+"</span><span class=\"audio-kind"+(isMusic?" music":"")+"\">"+std::string(isMusic?"MÚSICA":"SOM")+"</span></div>";
            s+="<span class=\"audio-meta\">"+Escape(sound.filename)+" · "+(sound.durationSeconds>0?Num(float(sound.durationSeconds))+" s":"duração desconhecida")+" · "+std::to_string(uses)+" uso(s)</span>";
            if(missing)s+="<span class=\"audio-meta audio-missing\">Arquivo ausente: importe novamente ou remova da biblioteca.</span>";
            s+="<label class=\"audio-inline\"><span>Volume</span><input id=\"audio-volume-"+Escape(sound.id)+"\" type=\"range\" min=\"0\" max=\"1\" step=\"0.05\" value=\""+Num(sound.volume)+"\" data-action=\"sound-volume:"+Escape(sound.id)+"\"/></label>";
            s+="<label class=\"audio-inline\"><span>Repetir</span><input id=\"audio-loop-"+Escape(sound.id)+"\" type=\"checkbox\" data-action=\"sound-loop:"+Escape(sound.id)+"\" "+(sound.loop?"checked=\"checked\"":"")+"/></label>";
            s+="<label class=\"audio-inline\"><span>Tipo</span><select id=\"audio-kind-"+Escape(sound.id)+"\" data-action=\"sound-kind:"+Escape(sound.id)+"\"><option value=\"sound\" "+(isMusic?"":"selected=\"selected\"")+">Som</option><option value=\"music\" "+(isMusic?"selected=\"selected\"":"")+">Música</option></select></label>";
            s+="<div class=\"audio-actions\">";
            s+=button("audio-play-"+sound.id,"preview-sound","Ouvir","play",sound.id);
            s+=button("audio-pause-"+sound.id,"pause-sound","Pausar","pause",sound.id);
            s+=button("audio-stop-"+sound.id,"stop-sound","Parar","stop",sound.id);
            s+=button("audio-background-"+sound.id,"music-background","Fundo","music",sound.id);
            s+=button("audio-playlist-"+sound.id,"music-playlist-add","+ Playlist","",sound.id);
            s+=button("audio-copy-"+sound.id,"audio-copy","Copiar código","copy",sound.id);
            s+=button("audio-insert-"+sound.id,"audio-insert","Inserir no script","script",sound.id);
            s+=button("audio-rename-"+sound.id,"rename-sound","Renomear","edit",sound.id);
            s+=button("audio-delete-"+sound.id,"remove-sound","Excluir","delete",sound.id);
            s+="</div></div>";
        }
        // Arquivos presentes em assets/ que ainda não têm metadados (projetos
        // antigos ou cópia manual) continuam visíveis e podem ser catalogados.
        for(std::size_t i=0;i<audioFiles.size();++i){
            const auto& file=audioFiles[i];
            if(knownReference(file.reference)||!matches(file))continue;
            ++shown;
            s+="<div id=\"audio-card-extra-"+std::to_string(i)+"\" class=\"audio-card\"><div class=\"audio-card-header\"><span class=\"audio-name\">Áudio não catalogado</span><span class=\"audio-kind\">ARQUIVO</span></div>";
            s+="<span class=\"audio-meta\">"+Escape(file.reference)+"</span><div class=\"audio-actions\">";
            s+=button("audio-extra-play-"+std::to_string(i),"preview-sound","Ouvir","play",file.reference);
            s+=button("audio-extra-catalog-"+std::to_string(i),"catalog-sound","Adicionar à biblioteca","image",file.reference);
            s+="</div></div>";
        }
        if(shown==0&&!assets.sounds.empty())s+="<div class=\"audio-empty\">Nenhum áudio encontrado para esta busca.</div>";
        s+="</div>";
        html("advanced-content",s,advancedMarkup);
    }
    void commonAdvanced() {
        const bool common=advanced=="settings"||advanced=="assets"||advanced=="audio";
        display("advanced-content",common);if(!common)return;
        if(editing()||ImGui::IsMouseDown(0))return;
        if(advanced=="audio"){audioPanel();return;}
        std::string s;
        if(advanced=="settings") {
            const auto& a=ui.scene_.gameAssets();const auto& p=a.player;
            auto title=[&](const char* name){s+="<div class=\"property-group\">"+std::string(name)+"</div>";};
            auto field=[&](const std::string& name,float v){s+="<label class=\"config-field\"><span>"+name+"</span><input id=\"game-"+name+"\" data-action=\"game:"+name+"\" value=\""+Num(v)+"\"/></label>";};
            auto toggle=[&](const std::string& name,bool v){s+="<label class=\"config-field\"><span>"+name+"</span><input type=\"checkbox\" data-action=\"game:"+name+"\" "+(v?"checked=\"checked\"":"")+"/></label>";};
            title("CHARACTER · saved with this project");s+="<div class=\"config-fields\">";
            field("WalkSpeed",p.walkSpeed);field("JumpHeight",p.jumpHeight);field("Gravity",p.gravity);field("Acceleration",p.acceleration);field("Deceleration",p.deceleration);field("TurnSpeed",p.turnSpeed);
            field("MaxHealth",p.maxHealth);field("InitialHealth",p.health);field("RespawnDelay",p.respawnDelay);field("CollisionHeight",p.collisionHeight);field("CollisionRadius",p.collisionRadius);toggle("AutoRotate",p.autoRotate);s+="</div>";
            title("ANIMATION ASSETS · stable local IDs");s+="<div class=\"config-fields\">";
            for(const auto& state:std::vector<std::pair<std::string,std::string>>{{"Idle",p.idleAnimation},{"Walk",p.walkAnimation},{"Jump",p.jumpAnimation},{"Fall",p.fallAnimation}}) {
                s+="<label class=\"config-field\"><span>"+state.first+"</span><select data-action=\"game:"+state.first+"\"><option value=\"\">Native fallback</option>";
                for(const auto& clip:a.animations) {
                    s+="<option value=\""+Escape(clip.id)+"\" "+(clip.id==state.second?"selected=\"selected\"":"")+">"+Escape(clip.name+" · anim://"+clip.id)+"</option>";
                }
                s+="</select></label>";
            }
            s+="<label class=\"config-field\"><span>Default rig</span><select data-action=\"game:DefaultRig\">";
            for(const auto& rig:a.rigs) {
                s+="<option value=\""+Escape(rig.id)+"\" "+(a.defaultRig==rig.id?"selected=\"selected\"":"")+">"+Escape(rig.name)+"</option>";
            }
            s+="</select></label></div>";
            title("PLAYER CAMERA");s+="<div class=\"config-fields\"><label class=\"config-field\"><span>Mode</span><select data-action=\"game:CameraMode\">";
            for(int i=0;i<3;++i)s+="<option value=\""+std::to_string(i)+"\" "+(i==int(p.cameraMode)?"selected=\"selected\"":"")+">"+std::string(i==0?"Classic":i==1?"Follow":"First person")+"</option>";
            s+="</select></label>";field("MinZoom",p.minZoom);field("MaxZoom",p.maxZoom);field("CameraDistance",p.cameraDistance);field("Sensitivity",p.sensitivity);toggle("ShiftLock",p.enableShiftLock);toggle("FirstPerson",p.allowFirstPerson);s+="</div>";
        } else {
            if(GetTime()-imageScan>2){images=game::ListImages(game::AssetsDir());imageScan=GetTime();}
            const auto sounds=game::ListAudioAssets(game::AssetsDir());
            s="<h2>Imagens locais</h2><p class=\"muted\">PNG / JPEG · selecione uma peça e aplique adesivo ou textura. Renomear muda só o nome exibido.</p>"+Button("import-dialog","Importar imagem","folder","","assets");
            if(images.empty())s+="<p>Nenhuma imagem importada ainda.</p>";
            s+="<div class=\"asset-grid\">";
            for(const auto& image:images){const auto path=std::filesystem::absolute(std::filesystem::path(game::AssetsDir())/image.filename).string();
                const auto usage=game::DescribeImageUsage(ui.scene_,image.filename).size();
                s+="<div class=\"asset-card\" title=\""+Escape("ID técnico: "+image.id+" · arquivo: "+image.filename)+"\"><img class=\"asset-preview\" src=\""+Escape(path)+"\"/><span class=\"asset-name\">"+Escape(image.name)+"</span><span class=\"asset-meta\">"+ImageTypeLabel(image.filename)+" · "+(usage?std::to_string(usage)+" uso(s)":"não utilizada")+"</span>"+Button("apply-image","Adesivo","part",path)+Button("apply-texture","Textura","part",path)+Button("rename-image","Renomear","edit",image.filename)+Button("replace-image","Substituir","image",image.filename)+Button("delete-image","Excluir","delete",image.filename)+"</div>";}
            s+="</div>";
            s+="<h2>Sons locais</h2><p class=\"muted\">MP3 / WAV / OGG · use <code>linblux.audio.play(id)</code> em um Script.</p>"+Button("import-sound","Importar som","folder");
            if(sounds.empty())s+="<p>Nenhum som importado ainda.</p>";
            if(!previewAudio.deviceReady())s+="<p class=\"muted\">Sem dispositivo de áudio: importação e metadados disponíveis; prévia indisponível.</p>";
            s+="<div class=\"asset-grid\">";
            for(const auto& file:sounds){const auto* saved=ui.scene_.gameAssets().findSound(file.reference);const auto& sound=saved?*saved:file;
                s+="<div class=\"asset-card\"><span class=\"asset-name\">"+Escape(saved?sound.name:"Som não catalogado")+"</span><span class=\"asset-meta\">"+Escape(file.reference)+(sound.durationSeconds>0?" · "+Num(sound.durationSeconds)+" s":"")+"</span>";
                s+=Button("preview-sound","Ouvir","play",file.reference)+Button("pause-sound","Pausar / continuar","",file.reference)+Button("stop-sound","Parar","stop",file.reference);
                s+="<label>Volume<input id=\"audio-volume-"+Escape(sound.id)+"\" type=\"range\" min=\"0\" max=\"1\" step=\"0.05\" value=\""+Num(sound.volume)+"\" data-action=\"sound-volume:"+Escape(sound.id)+"\"/></label><label>Repetir<input id=\"audio-loop-"+Escape(sound.id)+"\" type=\"checkbox\" data-action=\"sound-loop:"+Escape(sound.id)+"\" "+(sound.loop?"checked=\"checked\"":"")+"/></label>";
                s+=Button("rename-sound","Renomear","edit",file.reference)+Button("remove-sound","Excluir","delete",file.reference)+"</div>";}

            s+="</div>";
        }
        html("advanced-content",s,advancedMarkup);
    }
    void syncGame() {
        const bool active=ui.runtime_.active(),paused=ui.gameMenuOpen();
        // A fresh pause opens on the session page; a tab chosen during the
        // same pause remains selected until play resumes.
        if(paused&&!pauseWasOpen) {
            page="game";
            el("pause-content")->SetScrollTop(0);
        }
        menuProgress=std::clamp(menuProgress+(paused?1.f:-1.f)*std::clamp(GetFrameTime(),0.f,.25f)/.18f,0.f,1.f);
        const float eased=menuProgress*menuProgress*(3-2*menuProgress);
        const float panelWidth=std::min(view.width,std::clamp(view.width*.70f,580.f,760.f));
        style("pause-panel","width",Num(panelWidth)+"px");
        style("pause-panel","left",Num(-panelWidth*(1-eased))+"px");
        style("pause","pointer-events",paused?"auto":"none");
        pauseWasOpen=paused;
        display("pause",active&&(paused||menuProgress>0),"block");display("health",false);display("game-topbar",active&&!paused&&menuProgress==0,"flex");
        display("game-settings",false); // configurações permanecem na navegação do painel
        const auto cursor=ui.gameplayCursor();
        display("mouse-lock-cursor",active&&cursor.visual==GameCursorVisual::MouseLock);
        style("mouse-lock-cursor","left",Num(cursor.position.x-view.x)+"px");
        style("mouse-lock-cursor","top",Num(cursor.position.y-view.y)+"px");
        const std::string menuIcon=paused?"icon-player-close":"icon-player-menu";
        if(attr(el("game-menu-icon"),"src")!=menuIcon)el("game-menu-icon")->SetAttribute("src",menuIcon);
        el("game-settings")->SetClass("selected",paused&&page=="settings");
        for(const auto* name:{"game","settings","controls","audio","video","about"}) {display(std::string(name)+"-page",page==name);el(std::string("page-")+name)->SetClass("active",page==name);}
        if(!active)return;
        const float health=ui.runtime_.health(),maximum=std::max(1.f,ui.runtime_.settings().maxHealth);
        char s[64];std::snprintf(s,sizeof(s),"%.0f / %.0f",health,maximum);text("pause-health",s);
        text("pause-experience",ui.experienceUi_?ui.experienceUi_->documentTitle():std::filesystem::path(ui.projectPath_).stem().string());
        if(IsAudioDeviceReady())el("master-volume")->RemoveAttribute("disabled");else el("master-volume")->SetAttribute("disabled","");
        text("master-volume-label",IsAudioDeviceReady()?std::to_string(int(GetMasterVolume()*100))+"%":"Dispositivo de áudio indisponível nesta sessão.");
        text("player-name",ui.runtime_.rig().name);text("map-name",std::filesystem::path(ui.projectPath_).filename().string());
        text("session-state",ui.runtime_.state()==game::RuntimeState::Paused?"Simulação pausada explicitamente":"Simulação em andamento");
        if(paused) {
            auto set=[&](const char* id,const std::string& v){if(auto* f=dynamic_cast<Rml::ElementFormControl*>(el(id));f&&context->GetFocusElement()!=f&&f->GetValue()!=v)f->SetValue(v);};
            const auto& p=ui.runtime_.settings();text("camera-mode",p.cameraMode==game::CameraMode::Classic?"Livre":p.cameraMode==game::CameraMode::Follow?"Acompanhar":"Primeira pessoa");
            set("sensitivity",Num(p.sensitivity));set("camera-distance",Num(p.cameraDistance));
            set("sensitivity-slider",Num(1+(p.sensitivity-.0001f)/.0299f*99));
            el("distance-slider")->SetAttribute("min",Num(p.minZoom));el("distance-slider")->SetAttribute("max",Num(p.maxZoom));
            set("distance-slider",Num(p.cameraDistance));
            text("shift-lock",p.enableShiftLock?"Ativado":"Desativado");text("first-person",p.allowFirstPerson?"Ativado":"Desativado");
            text("fullscreen-value",ui.gameFullscreen_?"Ativado":"Desativado");
        }
    }
};

StudioFrontend::StudioFrontend(EditorUI& e):impl_(std::make_unique<Impl>(e)){}
StudioFrontend::~StudioFrontend(){rlImGuiSetExternalTextInput(false);rlImGuiDeferPopupRendering(false);auto& p=*impl_;if(p.context)Rml::RemoveContext("linblux");if(p.initialized)Rml::Shutdown();p.renderer.reset();RmlGL3::Shutdown();}
bool StudioFrontend::initialize(std::string& error) {
    auto& p=*impl_;Rml::String message;
    if(!RmlGL3::Initialize(&message)){error=message;return false;}
    p.renderer=std::make_unique<ui::StudioRenderer>();if(!*p.renderer){error="RmlUi GL3 renderer unavailable";return false;}
    Rml::SetSystemInterface(&p.system);Rml::SetRenderInterface(p.renderer.get());
    if(!Rml::Initialise()){error="RmlUi initialization failed";return false;}p.initialized=true;
    // The stylesheet asks for the "DejaVu Sans" family. Register a regular
    // and a bold face of the SAME family so `font-weight: bold` is honoured
    // instead of silently falling back to the regular weight, and so the
    // fallback font never mixes two different typefaces.
    struct FontSet{const char* regular;const char* bold;};
    const FontSet fontSets[]={
        {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf","/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"},
        {"/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf","/usr/share/fonts/truetype/liberation2/LiberationSans-Bold.ttf"},
        {"/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf","/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf"},
    };
    bool font=false;
    for(const auto& set:fontSets) {
        if(!std::filesystem::exists(set.regular))continue;
        if(!Rml::LoadFontFace(set.regular,"DejaVu Sans",Rml::Style::FontStyle::Normal,Rml::Style::FontWeight::Normal))continue;
        font=true;
        if(std::filesystem::exists(set.bold))Rml::LoadFontFace(set.bold,"DejaVu Sans",Rml::Style::FontStyle::Normal,Rml::Style::FontWeight::Bold);
        break;
    }
    if(!font){error="A TrueType sans-serif font is required";return false;}
    p.context=Rml::CreateContext("linblux",{GetScreenWidth(),GetScreenHeight()});if(!p.context){error="RmlUi context creation failed";return false;}
    auto root=std::filesystem::path(GetApplicationDirectory())/"share/linblux/ui";
    if(!std::filesystem::exists(root))root=std::filesystem::path(GetApplicationDirectory()).parent_path()/"share/linblux/ui";
    if(!std::filesystem::exists(root))root=LINBLUX_UI_DIR;
    p.document=p.context->LoadDocument((root/"documents/studio.rml").string());if(!p.document){error="Failed to load studio.rml";return false;}
    std::ifstream ribbonFile(root/"documents/ribbon.rml");
    if(!ribbonFile){error="Failed to load ribbon.rml";return false;}
    const std::string ribbon((std::istreambuf_iterator<char>(ribbonFile)),std::istreambuf_iterator<char>());
    p.el("ribbon-tools")->SetInnerRML(i18n::Markup(ribbon));
    for(const char* type:{"click","dblclick","change","focus","blur","mousedown"})p.document->AddEventListener(type,&p,true);
    p.document->Show();p.display("context-menu",false);p.display("tooltip",false);p.display("dialog-overlay",false);p.display("game-layer",false);p.display("advanced-content",false);
    p.syncing=true;p.loadLayout();p.layout();p.ribbon();p.tree();p.properties();p.context->Update();p.syncing=false;
    p.traceBoxes();
    rlImGuiDeferPopupRendering(true);error.clear();return true;
}
void StudioFrontend::draw() {
    auto& p=*impl_;p.previewAudio.update(GetFrameTime());if((p.advanced!="assets"&&p.advanced!="audio")||p.ui.runtime_.active()||(!p.previewRoot.empty()&&p.previewRoot!=game::ProjectAssetRoot())){p.previewAudio.stopAll();p.previewVoice=0;}const double started=GetTime();
    if(p.experiences) {
        p.experiences->frame();
        const bool exclusive=p.experiences->overlayActive();p.document->SetClass("experience-exclusive",exclusive);
        if(exclusive) {
            p.w=GetScreenWidth();p.h=GetScreenHeight();p.dpi=float(GetRenderWidth())/std::max(1,p.w);
            p.context->SetDimensions({GetRenderWidth(),GetRenderHeight()});
            p.style("studio","width",std::to_string(p.w)+"px");p.style("studio","height",std::to_string(p.h)+"px");
            p.style("studio","transform-origin","0px 0px");p.style("studio","transform",p.dpi==1?"none":"scale("+Num(p.dpi)+")");
            p.experienceInput();p.context->Update();rlImGuiSetExternalTextInput(p.editing());return;
        }
    }
    p.layout();if(p.w!=p.tracedW||p.h!=p.tracedH)p.traceBoxes();p.input();p.syncing=true;p.ribbon();p.tree();p.properties();p.commonAdvanced();p.documents();
    p.ui.explorerFocused_=p.treeKeyboard&&!p.editing();
    if(!p.advanced.empty()&&!p.ui.runtime_.active()&&!p.dialogOpen) {
        p.ui.showScriptEditor_=p.advanced=="script";p.ui.showAnimationEditor_=p.advanced=="animation";p.ui.showRigEditor_=p.advanced=="rig";p.ui.showGameSettings_=false;p.ui.showOutput_=p.advanced=="output";
        if(p.advanced=="rig")p.ui.drawRigEditor();else if(p.advanced=="animation")p.ui.drawAnimationEditor();else if(p.advanced=="script")p.ui.drawScriptEditor();else if(p.advanced=="output")p.ui.drawOutput();
        if(p.ui.showGameSettings_)p.showAdvanced("settings");
        else if(p.ui.showScriptEditor_&&p.advanced!="script")p.showAdvanced("script");
        else if(p.ui.showAnimationEditor_&&p.advanced!="animation")p.showAdvanced("animation");
        else if(p.ui.showRigEditor_&&p.advanced!="rig")p.showAdvanced("rig");
        else if(p.ui.showOutput_&&p.advanced!="output")p.showAdvanced("output");
    }
    // Existing specialized modals remain interactive, never underneath an RML modal.
    if(p.ui.requestSavePopup_){p.ui.requestSavePopup_=false;p.prompt("save-as");}
    if(p.ui.requestOpenPopup_){p.ui.requestOpenPopup_=false;p.prompt("open");}
    if(!p.dialogOpen)p.ui.drawDialogs();
    const double beforeScene=GetTime();p.ui.drawViewport();p.documents();const double afterScene=GetTime();p.syncGame();p.updateTooltip();
    p.display("dialog-overlay",p.dialogOpen,"flex");
    p.text("project",p.experiences?p.experiences->documentTitle():std::filesystem::path(p.ui.projectPath_).filename().string());p.text("message",p.ui.status_);
    p.text("scene-count",std::to_string(p.ui.scene_.parts().size())+" "+i18n::Text("parts")+" · "+std::to_string(p.ui.scene_.objects().size())+" "+i18n::Text("instances"));
    p.el("snap")->SetAttribute("type","checkbox");if(p.ui.snapEnabled_)p.el("snap")->SetAttribute("checked","");else p.el("snap")->RemoveAttribute("checked");
    if(auto* ss=p.el("surface-snap")){ss->SetAttribute("type","checkbox");if(p.ui.surfaceSnapEnabled_)ss->SetAttribute("checked","");else ss->RemoveAttribute("checked");}
    if(auto* space=dynamic_cast<Rml::ElementFormControl*>(p.el("transform-space"));space&&p.context->GetFocusElement()!=space) {
        const std::string value=p.ui.gizmoLocalSpace_?"local":"world";
        if(space->GetValue()!=value)space->SetValue(value);
    }
    p.ui.gizmo_.setSnap(p.ui.snapEnabled_?p.ui.moveSnap_:0,p.ui.snapEnabled_?p.ui.scaleSnap_:0,p.ui.snapEnabled_?p.ui.rotateSnapDegrees_:0);
    p.context->Update();p.syncing=false;rlImGuiSetExternalTextInput(p.editing());
    if(p.timing){p.presentationTime+=beforeScene-started;p.sceneTime+=afterScene-beforeScene;p.updateTime+=GetTime()-afterScene;p.frameTime+=GetFrameTime();}
}
void StudioFrontend::render(){auto& p=*impl_;const double started=GetTime();p.renderer->drawCalls=0;if(p.experiences)p.experiences->drawBackground();rlDrawRenderBatchActive();p.renderer->SetViewport(GetRenderWidth(),GetRenderHeight());p.renderer->BeginFrame();p.context->Render();p.renderer->EndFrame();
    rlImGuiRenderDeferredPopups();
    if(p.mouseOwned && !(Within(GetMousePosition(),p.advancedRect)&&p.advanced!="settings"&&p.advanced!="assets"&&p.advanced!="audio"))SetMouseCursor(p.system.cursor);
    if(p.timing){p.renderTime+=GetTime()-started;if(++p.timingFrames==30){std::fprintf(stderr,"RML timing ms: presentation=%.2f scene=%.2f update=%.2f render=%.2f frame=%.2f draws=%d\n",p.presentationTime*1000/30,p.sceneTime*1000/30,p.updateTime*1000/30,p.renderTime*1000/30,p.frameTime*1000/30,p.renderer->drawCalls);p.timingFrames=0;p.presentationTime=p.sceneTime=p.updateTime=p.renderTime=p.frameTime=0;}}
}
void StudioFrontend::filter(EditorInput& in,InputContext& c) {
    auto& p=*impl_;if(p.keyboardOwned||p.editing()||p.dialogOpen){c.text=true;c.widget=true;c.keyboardCaptured=true;in.play=in.save=in.open=in.remove=in.rename=in.duplicate=false;in.tool=-1;in.jump=false;in.movement={};}
    if(p.mouseOwned){in.leftPressed=in.leftDown=in.rightPressed=in.rightDown=in.middlePressed=in.middleDown=false;in.wheel=0;c.hovered=false;
        // In the editor any RML surface (panels, ribbon, tabs, advanced
        // editors) owns the pointer; otherwise a stale ImGui viewport focus
        // would let WASD/F move the 3D camera while the user works in RML.
        // During Play only real modals/text/drags capture gameplay keys, so
        // hovering the HUD still leaves WASD and jump with the character.
        if(!p.ui.runtime_.active()||p.dialogOpen||p.contextOpen||p.dragging||!p.split.empty())c.widget=true;
        // Passive panels own the pointer, not the editor's history shortcuts.
        // Text, popups and active drags keep their normal input protection.
        if((in.undo||in.redo)&&!p.ui.runtime_.active()&&!p.keyboardOwned&&!p.editing()&&
            !p.dialogOpen&&!p.contextOpen&&!p.dragging&&p.split.empty())c.widget=false;
    }
    if(p.ui.runtime_.active()&&!p.editing()&&!p.dialogOpen)c.focused=true;
    if(p.escapeConsumed)in.escape=false;
    if(p.treeKeyboard&&!Within(in.mouse,p.view))in.remove=in.duplicate=in.rename=in.copy=in.paste=false;
}
Rectangle StudioFrontend::viewport()const{return impl_->view;}
Rectangle StudioFrontend::elementBounds(const std::string& id)const{return impl_->bounds(id);}
std::string StudioFrontend::elementText(const std::string& id)const{auto* e=impl_->el(id);return e?e->GetInnerRML():std::string();}
void StudioFrontend::scrollIntoView(const std::string& id){if(auto* e=impl_->el(id))e->ScrollIntoView(false);}
bool StudioFrontend::textFocused()const{return impl_->editing();}
void StudioFrontend::enableExperiences(experiences::ExperienceManager& manager,bool home) {
    auto& p=*impl_;
    auto root=std::filesystem::path(GetApplicationDirectory())/"share/linblux/ui";
    if(!std::filesystem::exists(root))root=std::filesystem::path(GetApplicationDirectory()).parent_path()/"share/linblux/ui";
    if(!std::filesystem::exists(root))root=LINBLUX_UI_DIR;
    std::ifstream file(root/"documents/home.rml");if(!file)throw std::runtime_error("Missing home.rml");
    const std::string html((std::istreambuf_iterator<char>(file)),{});p.el("experience-host")->SetInnerRML(i18n::Markup(html));
    p.experiences=std::make_unique<ExperienceUI>(p.ui,manager,*p.context,*p.document);p.experiences->initialize(home);
}
void StudioFrontend::resetDocumentView() {
    auto& p=*impl_;p.advanced.clear();p.search.clear();p.propertySearch.clear();p.collapsed.clear();p.treeHash=0;
    p.treeMarkup.clear();p.propertyMarkup.clear();p.documentMarkup.clear();p.selectedRigDecal.clear();p.selectedDecalRig.clear();
    p.dragNode=p.dragTarget=0;p.dragging=false;p.contextOpen=false;p.split.clear();p.context->GetRootElement()->Focus();p.commands.clear();p.treeKeyboard=false;
    invalidateImageLibrary();Rml::ReleaseTextures(p.renderer.get());
}
void StudioFrontend::invalidateImageLibrary(){impl_->images.clear();impl_->imageScan=-10;impl_->advancedMarkup.clear();}
void StudioFrontend::invalidateAudio(){impl_->invalidateAudio();}
void StudioFrontend::prepareAdvanced(){auto r=impl_->advancedRect;ImGui::SetNextWindowDockID(0,ImGuiCond_Always);ImGui::SetNextWindowPos({r.x,r.y},ImGuiCond_Always);ImGui::SetNextWindowSize({r.width,r.height},ImGuiCond_Always);}
}
