#include "ui/ExperienceUI.h"
#include "ui/Localization.h"
#include "ui/EditorActions.h"
#include "ui/StudioFrontend.h"
#include "editor/EditorUI.h"
#include "game/ImageAssets.h"
#include "core/ImageInfo.h"
#include "serialization/ProjectIO.h"
#include "rendering/EditorCamera.h"
#include "physics/PhysicsWorld.h"
#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include "imgui.h"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <fstream>
#include <stdexcept>

namespace loblox {
namespace fs=std::filesystem;
namespace {
std::string esc(const std::string& s){std::string out;for(char c:s)out+=c=='&'?"&amp;":c=='<'?"&lt;":c=='>'?"&gt;":c=='\"'?"&quot;":std::string(1,c);return out;}
std::string button(const std::string& action,const std::string& label,const std::string& value={},const std::string& id={},bool primary=false,const std::string& classes={}) {
    const std::string kind=(primary?"experience-primary ":"")+classes;
    return "<button "+(id.empty()?"":"id=\""+id+"\" ")+"data-experience=\""+action+"\" data-value=\""+esc(value)+"\" class=\""+kind+"\">"+esc(i18n::Text(label))+"</button>";
}
void show(Rml::ElementDocument& doc,const char* id,bool visible,const char* layout="block") {if(auto* e=doc.GetElementById(id))e->SetProperty("display",visible?layout:"none");}
void text(Rml::ElementDocument& doc,const char* id,const std::string& value) {if(auto* e=doc.GetElementById(id);e&&e->GetInnerRML()!=esc(i18n::Text(value)))e->SetInnerRML(esc(i18n::Text(value)));}
std::string field(const char* id,const char* name,const std::string& value,const char* hint="") {
    return "<label class=\"experience-field\"><span>"+std::string(name)+"</span><input id=\""+id+"\" value=\""+esc(value)+"\"/><small>"+hint+"</small></label>";
}
}
ExperienceUI::ExperienceUI(EditorUI& editor,experiences::ExperienceManager& manager,Rml::Context& context,Rml::ElementDocument& document)
    :editor_(editor),manager_(manager),context_(context),document_(document) {editor_.experienceUi_=this;}
ExperienceUI::~ExperienceUI(){if(loadingJob_.valid())loadingJob_.wait();if(load_!=LoadState::None)editor_.runtime_.stop(false);editor_.experienceUi_=nullptr;}
bool ExperienceUI::overlayActive() const {return home_||load_!=LoadState::None;}
bool ExperienceUI::loading() const {return load_!=LoadState::None;}
void ExperienceUI::initialize(bool home) {home_=home;documentOpened();show(document_,"experience-menu",true);show(document_,"experience-root",true);dirty_=true;}
void ExperienceUI::documentOpened(){savedWorld_=projectio::toJson(editor_.scene_);}
std::string ExperienceUI::documentTitle() const {return manager_.active()?manager_.active()->manifest.name:fs::path(editor_.projectPath_).filename().string();}
void ExperienceUI::enqueue(const std::string& action,const std::string& value){commands_.emplace_back(action,value);}
std::string ExperienceUI::value(const std::string& id) const {
    if(auto* field=dynamic_cast<Rml::ElementFormControl*>(document_.GetElementById(id)))return field->GetValue();
    return {};
}
void ExperienceUI::setValue(const std::string& id,const std::string& v) {
    if(auto* field=dynamic_cast<Rml::ElementFormControl*>(document_.GetElementById(id)))field->SetValue(v);
}
bool ExperienceUI::unsaved() const {
    if(editor_.animationDirty_)return true;
    for(const auto& entry:editor_.scriptDrafts_)if(entry.second.dirty)return true;
    return projectio::toJson(editor_.scene_)!=savedWorld_;
}
void ExperienceUI::resetDocument() {
    editor_.scriptDrafts_.clear();editor_.openDocuments_.clear();editor_.activeScript_=editor_.activeDocument_=editor_.requestedScript_=0;
    editor_.cancelRigGizmo(false);editor_.selectedRig_.clear();editor_.animationSource_.clear();editor_.animationDirty_=false;
    editor_.previewPlaying_=false;editor_.previewPose_={};editor_.history_.clear();editor_.clearTransformReadout();editor_.editStartValid_=false;
    editor_.camera_.releaseCapture();editor_.setGameplayCapture(false);editor_.showRigEditor_=editor_.showAnimationEditor_=editor_.showScriptEditor_=editor_.showOutput_=false;
    editor_.frontend_->resetDocumentView();
}
void ExperienceUI::open(const experiences::Experience& experience) {
    if(manager_.active()&&manager_.active()->root==experience.root){home_=false;return;}
    if(manager_.active()&&unsaved())throw std::runtime_error("Save your current work or return Home with Discard before opening another experience.");
    editor_.endGizmoSession(true,"experience_open");editor_.cancelRigGizmo();
    manager_.open(experience,editor_.scene_);resetDocument();editor_.physics_.setGravity({0,-editor_.scene_.gameAssets().player.gravity,0});editor_.physics_.rebuildAll(editor_.scene_);
    editor_.setProjectPath(manager_.activePlace().string());documentOpened();home_=false;selected_=manager_.active();fileMenu_=false;error_.clear();
    if(auto* rig=editor_.scene_.gameAssets().findRig(editor_.scene_.gameAssets().defaultRig))editor_.camera_.focusOn(rig->authoringPivot().position,8);
    else editor_.camera_.focusOn({0,3,0},18);
    editingDefault_=editingCharacter_=false;editor_.setStatus("Experience opened: "+manager_.active()->manifest.name);
}
void ExperienceUI::closeDocument() {
    editor_.stopPlay();editor_.endGizmoSession(true,"experience_close");resetDocument();editor_.physics_.clearBodies();editor_.scene_.clearObjects();manager_.close();
    documentOpened();home_=true;page_="home";selected_.reset();dirty_=true;fileMenu_=false;returnToStudio_=false;
}
void ExperienceUI::requestHome() {
    if(loading())return;
    if(chooser_.active())return;
    if(editor_.runtime_.active()){error_="Stop the Play session before changing experiences.";return;}
    fileMenu_=false;
    if(!home_&&unsaved()){confirm_="unsaved";return;}
    closeDocument();
}
bool ExperienceUI::save(std::string& error) {
    try{manager_.save(editor_.scene_);editor_.setProjectPath(manager_.activePlace().string());documentOpened();return true;}
    catch(const std::exception& e){error=e.what();return false;}
}
void ExperienceUI::choose(const std::string& purpose,bool saveFile,bool directory,bool images) {
    const auto initial=directory?manager_.libraryRoot().string()+"/":saveFile?(manager_.libraryRoot()/"Experience.lo").string():std::string(std::getenv("HOME")?std::getenv("HOME"):"/")+"/";
    if(!chooser_.begin(images?"Choose an image":directory?"Choose experience library":saveFile?"Export Experiência LINBLUX":"Open / Import Experience",initial,saveFile,directory,images)){error_=chooser_.error();return;}
    dialogPurpose_=purpose;editor_.camera_.releaseCapture();editor_.setGameplayCapture(false);
}
void ExperienceUI::chooseAsset(){choose("asset",false,false,true);}
void ExperienceUI::removeAsset(const std::string& reference,bool usedByScene) {
    manager_.removeAsset(reference,usedByScene);
    dirty_=true;
}
std::vector<game::ImageUsage> ExperienceUI::presentationReferences(const std::string& reference) const {
    const auto active=manager_.active();return active?game::DescribeImageUsage(manager_.inspect(active->root).manifest,reference):std::vector<game::ImageUsage>{};
}
std::vector<std::string> ExperienceUI::presentationUses(const std::string& reference) const {
    std::vector<std::string> result;for(const auto& use:presentationReferences(reference))result.push_back(use.where);return result;
}
bool ExperienceUI::clearPresentationImage(const game::ImageUsage& use,std::string& error) {
    try {
        const auto active=manager_.active();if(!active)throw std::runtime_error("Nenhuma experiência aberta.");
        auto manifest=manager_.inspect(active->root).manifest;std::string* field=nullptr;
        if(use.property=="icon")field=&manifest.icon;else if(use.property=="banner")field=&manifest.banner;
        else if(use.property=="loadingBackground")field=&manifest.loadingBackground;else if(use.property=="faceImage")field=&manifest.faceImage;
        if(!field||*field!=use.reference)throw std::runtime_error("Referência alterada. Consulte os usos novamente.");
        field->clear();manager_.updateMetadata(manifest);game::SetProjectFaceImage(manifest.faceImage);dirty_=true;error.clear();return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
std::size_t ExperienceUI::replaceImageReferences(const std::string& from,const std::string& to) {
    const auto active=manager_.active();if(!active)return 0;
    const auto source=std::filesystem::path(game::NormalizeImageReference(from)).filename().string();
    if(source.empty())return 0;
    auto manifest=active->manifest;
    std::size_t replaced=0;
    const auto swap=[&](std::string& value) {
        if(!game::ImageReferenceMatches(value,from))return;
        value=to;++replaced;
    };
    swap(manifest.icon);swap(manifest.banner);swap(manifest.loadingBackground);swap(manifest.faceImage);
    if(replaced) {
        manager_.updateMetadata(manifest);
        game::SetProjectFaceImage(manifest.faceImage);
        dirty_=true;
    }
    return replaced;
}
void ExperienceUI::releaseLoadingImages() {
    document_.GetElementById("exp-loading-background")->RemoveAttribute("src");
    document_.GetElementById("exp-loading-icon")->RemoveAttribute("src");
    Rml::ReleaseTextures();
}
void ExperienceUI::selectedFile(const std::string& purpose,const std::string& path) {
    if(purpose=="character") {
        const auto id=manager_.importCharacter(path,editor_.scene_);editor_.selectRig(id);editor_.frontend_->invalidateImageLibrary();
        editor_.setStatus("Personagem importado. Configure-o e confirme Salvar como padrão da engine.");
    } else if(purpose=="import") {
        const fs::path file(path);experiences::Experience experience;
        if(file.extension()==".lo")experience=manager_.importPackage(file);
        else if(file.filename()=="experience.json")experience=manager_.inspect(file.parent_path());
        else experience=manager_.importLegacy(file,file.stem().string());
        open(experience);
    } else if(purpose=="export") {
        auto file=fs::path(path);if(file.extension()!=".lo")file+=".lo";
        if(!selected_)throw std::runtime_error("Choose an experience to export");
        manager_.exportPackage(*selected_,file);error_.clear();editor_.setStatus("Exported: "+file.string());
    } else if(purpose.rfind("locate-",0)==0) {manager_.relocate(purpose.substr(7),path);dirty_=true;}
    else if(purpose=="library") {manager_.setLibraryRoot(path);if(page_=="new")setValue("exp-location",path);else dirty_=true;}
    else if(purpose=="asset") {
        EditorActions actions(editor_);if(!actions.dispatch({"import-image",path}))throw std::runtime_error(editor_.status_);editor_.frontend_->invalidateImageLibrary();
    } else if(purpose.rfind("media-",0)==0) {
        ImageInfo info;if(!ReadSafeImageInfo(path,info))throw std::runtime_error("Invalid image or memory budget exceeded");
        setValue("exp-"+purpose.substr(6),path);
        if(auto* preview=document_.GetElementById("exp-preview-"+purpose.substr(6)))preview->SetAttribute("src",path);
    }
}
void ExperienceUI::dispatch(const std::string& action,const std::string& v) {
    if(action=="cancel-load") {
        cancelLoad_=true;
        if(load_==LoadState::Paint||load_==LoadState::Error) {load_=LoadState::None;pendingPath_.clear();error_.clear();releaseLoadingImages();}
        else if(load_==LoadState::Fading){editor_.runtime_.stop(false);load_=LoadState::None;releaseLoadingImages();}
        return;
    }
    if(loading())return;
    if(action=="dismiss-error"){error_.clear();return;}
    if(action=="close-file"){fileMenu_=false;return;}
    if(action=="file"){fileMenu_=!fileMenu_;return;}
    if(action=="back-home"){requestHome();return;}
    if(action=="return-studio"){home_=false;return;}
    if(action=="import-character"){choose("character");return;}
    if(action=="save"){if(!editor_.saveCurrentProject())throw std::runtime_error(editor_.status_);fileMenu_=false;return;}
    if(editor_.runtime_.active())throw std::runtime_error("Stop Play before managing experience files.");
    if(action!="card-menu"&&!cardMenu_.empty()){cardMenu_.clear();dirty_=true;}
    if(action=="page") {page_=v;home_=true;selected_.reset();dirty_=true;error_.clear();libraryPage_=0;filter_.clear();}
    else if(action=="filter"){filter_=value("exp-filter");libraryPage_=0;dirty_=true;}
    else if(action=="previous-page"){if(libraryPage_)--libraryPage_;dirty_=true;}
    else if(action=="next-page"){++libraryPage_;dirty_=true;}
    else if(action=="locate"){choose("locate-"+v,false,true);}
    else if(action=="forget-missing"){manager_.forgetMissing(v);dirty_=true;}
    else if(action=="new"){
        if(manager_.active()&&unsaved())throw std::runtime_error("Save your current work or return Home before creating another experience.");
        page_="new";home_=true;pendingPath_=v.empty()?"engine-default":v;dirty_=true;error_.clear();
    }
    else if(action=="cancel-form") {if(returnToStudio_){home_=false;returnToStudio_=false;}else{page_="home";dirty_=true;}}
    else if(action=="create") {
        experiences::CreateOptions options;options.name=value("exp-name");options.description=value("exp-description");options.templateId=value("exp-template");
        options.icon=value("exp-icon");options.banner=value("exp-banner");options.loadingBackground.clear();
        options.useDefaultCharacter=document_.GetElementById("exp-use-character")->HasAttribute("checked");
        if(value("exp-location")!=manager_.libraryRoot().string())manager_.setLibraryRoot(value("exp-location"));
        open(manager_.create(options));
    } else if(action=="open"||action=="details"||action=="card-menu") {
        auto found=std::find_if(entries_.begin(),entries_.end(),[&](const auto& e){return e.manifest.installationId==v;});
        if(found==entries_.end())throw std::runtime_error("Experience not found; refresh the library");
        selected_=*found;
        if(action=="open")open(*selected_);
        else if(action=="card-menu"){cardMenu_=cardMenu_==v?"":v;dirty_=true;}
        else{page_="details";dirty_=true;}
    } else if(action=="current-details") {
        if(manager_.active()&&manager_.isEngineDefault(*manager_.active()))throw std::runtime_error("Este é um recurso protegido da engine; use Salvar como padrão da engine.");
        if(!manager_.active())throw std::runtime_error("Import this legacy project as an experience first (Home > Open / Import)");
        selected_=manager_.active();home_=true;page_="details";returnToStudio_=true;fileMenu_=false;dirty_=true;
    } else if(action=="save-details") {
        if(!selected_)return;
        auto m=selected_->manifest;m.name=value("exp-name");m.description=value("exp-description");
        const auto media=[&](const std::string& input,std::string& ref){const auto path=value(input);if(path.empty()){ref.clear();return;}
            if(path!=ref)ref=manager_.importImage(*selected_,path);};
        media("exp-icon",m.icon);media("exp-banner",m.banner);
        manager_.updateMetadata(*selected_,m);selected_=manager_.inspect(selected_->root);error_.clear();
        if(returnToStudio_){home_=false;returnToStudio_=false;}else{page_="home";dirty_=true;}
    } else if(action=="remove-media") {setValue("exp-"+v,"");if(auto* image=document_.GetElementById("exp-preview-"+v))image->SetAttribute("src","icon-image");}
    else if(action=="choose-media")choose("media-"+v,false,false,true);
    else if(action=="import") {if(!home_&&unsaved()){requestHome();return;}choose("import");}
    else if(action=="library-location")choose("library",false,true);
    else if(action=="duplicate") {if(selected_){selected_=manager_.duplicate(*selected_,selected_->manifest.name+" (cópia)");page_="details";dirty_=true;}}
    else if(action=="export-current") {if(!manager_.active())throw std::runtime_error("Import the legacy project as an experience before exporting");
        if(!editor_.saveCurrentProject())throw std::runtime_error(editor_.status_);
        selected_=manager_.active();fileMenu_=false;choose("export",true);}
    else if(action=="export") {if(selected_)choose("export",true);}
    else if(action=="archive"||action=="delete"||action=="restore-factory") {
        if(action!="restore-factory"&&selected_&&manager_.isEngineDefault(*selected_))throw std::runtime_error("O Mundo Padrão da Engine é protegido contra exclusão e arquivamento");
        confirm_=selected_&&manager_.active()&&selected_->root==manager_.active()->root&&unsaved()?"unsaved":action;fileMenu_=false;
    }
    else if(action=="restore") {if(selected_){manager_.setArchived(*selected_,false);page_="archive";dirty_=true;}}
    else if(action=="cancel-confirm")confirm_.clear();
    else if(action=="discard-home") {confirm_.clear();closeDocument();}
    else if(action=="confirm") {
        const auto previous=confirm_;const bool selectedActive=selected_&&manager_.active()&&selected_->root==manager_.active()->root;
        if(previous=="unsaved"){if(!editor_.saveCurrentProject())throw std::runtime_error(editor_.status_);closeDocument();}
        else if(previous=="restore-factory") {manager_.restoreFactoryDefaults();dirty_=true;editor_.setStatus("Padrão de fábrica restaurado. Backup preservado em "+manager_.lastDefaultsBackup().string());}
        else if(previous=="save-default-world"||previous=="save-default-character") {
            if(!editor_.saveCurrentProject())throw std::runtime_error(editor_.status_);
            const auto rigId=previous=="save-default-character"?pendingPath_:editor_.scene_.gameAssets().defaultRig;
            manager_.saveEngineDefaults(editor_.scene_,rigId);editor_.scene_.gameAssets().defaultRig=rigId;
            documentOpened();editor_.setStatus("Mundo e personagem padrão salvos em "+manager_.engineDefaultsRoot().string()+". Backup da revisão anterior preservado.");
        }
        else if(selected_&&(previous=="archive"||previous=="delete")){if(previous=="delete")manager_.deletePermanently(*selected_);else manager_.setArchived(*selected_,true);if(selectedActive)closeDocument();page_=previous=="delete"?"home":"archive";selected_.reset();dirty_=true;editor_.setStatus(previous=="delete"?"Experiência excluída permanentemente.":"Experiência arquivada.");}
        confirm_.clear();
    } else if(action=="default-map"||action=="default-r6") {
        if(home_||!manager_.active())throw std::runtime_error("Abra o Mundo Padrão da Engine ou uma experiência para salvar o padrão");
        const auto id=action=="default-r6"?editor_.selectedRig_:editor_.scene_.gameAssets().defaultRig;
        if(!editor_.scene_.gameAssets().findRig(id))throw std::runtime_error("Selecione e configure um personagem de jogador antes de salvar o padrão");
        pendingPath_=id;confirm_=action=="default-r6"?"save-default-character":"save-default-world";fileMenu_=false;
    }
    else if(action=="edit-default"||action=="edit-character") {
        if(manager_.active()&&unsaved())throw std::runtime_error("Salve o trabalho atual antes de editar os padrões da engine.");
        open(manager_.editEngineDefault(action=="edit-character"));editingDefault_=action=="edit-default";editingCharacter_=action=="edit-character";
    }
    else if(action=="help") {const auto path=game::BuiltinAssetPath("docs/"+v);if(path.empty())helpText_="Construa com peças, selecione uma ferramenta e pressione F5 para executar.\nScripts executam somente durante o teste.\nUse Arquivo > Exportar experiência para compartilhar um pacote .lo.";
        else {std::ifstream file(path);helpText_={std::istreambuf_iterator<char>(file),{}};if(helpText_.size()>65536)helpText_.resize(65536);}page_="help";dirty_=true;}
}
void ExperienceUI::buildPage() {
    std::string html,footer;
    const auto title=[&](const std::string& name,const std::string& description){return "<div class=\"experience-title\"><div><h1>"+esc(i18n::Text(name))+"</h1><p>"+esc(i18n::Text(description))+"</p></div><div>"+(manager_.active()?button("return-studio","Return to Studio"):"")+button("new","New Experience",{},"exp-new-main",true)+"</div></div>";};
    const bool archive=page_=="archive";
    if(page_=="home"||page_=="library"||archive||page_=="templates") {
        if(page_=="home")html+="<div class=\"home-intro\"><small>LINBLUX STUDIO</small><h1>Dê forma ao seu próximo mundo.</h1><p>Crie, programe e explore experiências 3D com liberdade.</p><div class=\"home-actions\">"+button("new","Criar experiência",{},"exp-new-main",true)+button("import","Abrir experiência",{},"exp-import")+"</div></div>";
        else html+=title(archive?"Archive":page_=="library"?"My Experiences":page_=="templates"?"Templates":"Início",archive?"Archived and deleted experiences can be restored.":"Create, build and share experiences on your computer.");
        if(page_=="home")html+="<div id=\"engine-default-card\" class=\"home-default-card\"><div class=\"home-default-icon\"><img src=\"linblux-logo\"/></div><div class=\"home-default-copy\"><h2>Mundo Padrão da Engine</h2><p>Edite o mundo e o personagem que dão origem às novas experiências.</p><small>RECURSO PROTEGIDO · LINBLUX ENGINE</small></div><div class=\"home-default-actions\">"+button("edit-default","Editar padrão",{},"exp-edit-default")+"</div></div>";
        if(page_!="templates") {
            entries_=manager_.list(archive);
            if(page_!="home") {
                html+="<div class=\"experience-search\"><input id=\"exp-filter\" value=\""+esc(filter_)+"\" placeholder=\"Search name or description\"/>"+button("filter","Search",{},"exp-search")+"</div>";
                auto lower=[](std::string s){for(char& c:s)c=char(std::tolower(static_cast<unsigned char>(c)));return s;};
                const auto query=lower(filter_);entries_.erase(std::remove_if(entries_.begin(),entries_.end(),[&](const auto& e){return lower(e.manifest.name+" "+e.manifest.description).find(query)==std::string::npos;}),entries_.end());
            }
            html+="<div class=\"experience-section\"><div class=\"experience-section-title\"><h2>"+std::string(archive?"Archived Experiences":page_=="library"?"All Experiences":"Experiências recentes")+"</h2>"+(page_=="home"?"<span class=\"experience-section-count\">"+std::to_string(entries_.size())+"</span>":"")+"</div>"+button("page","View all","library")+"</div><div class=\"experience-grid"+(page_=="home"?" home-recent":"")+"\">";
            if(entries_.empty())html+="<div class=\"experience-empty\">"+std::string(archive?"Your archive is empty.":"Seu próximo mundo começa aqui. Crie uma experiência ou abra um projeto existente.")+"</div>";
            const size_t limit=page_=="home"?8:24;
            const size_t pages=std::max(size_t(1),(entries_.size()+limit-1)/limit);
            libraryPage_=std::min(libraryPage_,pages-1);const size_t start=page_=="home"?0:libraryPage_*limit;
            for(size_t i=start;i<std::min(entries_.size(),start+limit);++i) {
                const auto& entry=entries_[i];fs::path thumb;try{thumb=manager_.thumbnail(entry);}catch(const std::exception&){}
                const auto& id=entry.manifest.installationId;
                html+=std::string("<div class=\"experience-card")+(page_=="home"?" home-card":"")+(cardMenu_==id?" menu-open":"")+"\" id=\"exp-card-"+std::to_string(i)+"\" data-experience-card=\""+id+"\"><div class=\"experience-cover\"><img class=\""+std::string(thumb.empty()?"experience-placeholder":"")+"\" src=\""+esc(thumb.empty()?"linblux-logo":thumb.string())+"\"/></div><div class=\"experience-card-body\"><h3>"+esc(entry.manifest.name)+"</h3><p>"+esc(entry.manifest.description.empty()?"Uma experiência LINBLUX local":entry.manifest.description)+"</p><div class=\"experience-card-meta\"><small>Local</small><span class=\"experience-card-dot\"/><small>"+esc(entry.manifest.modified.substr(0,10))+"</small></div></div><div class=\"experience-card-actions\">"+button(archive?"details":"open",archive?"Manage":"Open",id,"exp-open-"+std::to_string(i),true)+button("card-menu","…",id,"exp-details-"+std::to_string(i),false,std::string("experience-more")+(cardMenu_==id?" active":""))+"</div>";
                if(cardMenu_==id)html+="<div id=\"experience-card-menu\" class=\"experience-card-menu\">"+button("details","Rename / Edit Details…",id,"exp-card-details")+button("duplicate","Duplicar",{},"exp-card-duplicate")+button("export","Exportar .lo…",{},"exp-card-export")+"<div class=\"experience-menu-rule\"/>"+button(archive?"restore":"archive",archive?"Restaurar":"Arquivar")+"<div class=\"experience-menu-rule\"/>"+button("delete","Excluir…",{},"exp-card-delete",false,"experience-menu-danger")+"<div class=\"experience-menu-rule\"/>"+button("dismiss-card","Fechar",{},"exp-card-close")+"</div>";
                html+="</div>";
            }
            html+="</div>";
            if(page_!="home"&&pages>1)html+="<div class=\"experience-pagination\">"+button("previous-page","Previous",{},"exp-prev")+"<span>Page "+std::to_string(libraryPage_+1)+" of "+std::to_string(pages)+" · "+std::to_string(entries_.size())+" experiences</span>"+button("next-page","Next",{},"exp-next")+"</div>";
            if(page_=="home"||page_=="library")for(const auto& lost:manager_.missing())html+="<div class=\"experience-missing\"><b>"+esc(lost.name)+"</b><small>Unavailable: "+esc(lost.previousRoot.string())+"</small>"+button("locate","Locate directory…",lost.installationId)+button("forget-missing","Forget recent entry",lost.installationId)+"</div>";
        }
        if(page_=="templates") {
            const auto available=manager_.templates();const bool hasDefault=std::any_of(available.begin(),available.end(),[](const auto& item){return item.id=="my-default";});
            html+="<div class=\"experience-section\"><h2>Open a Template</h2>"+(hasDefault?button("edit-default","Edit My Default"):"")+"</div><div class=\"experience-grid\">";
            for(const auto& item:available)html+="<button id=\"exp-template-"+item.id+"\" class=\"experience-template\" data-experience=\"new\" data-value=\""+item.id+"\"><img src=\"icon-"+std::string(item.id=="classic"?"rig":item.id=="my-default"?"folder":"part")+"\"/><b>"+esc(item.name)+"</b><small>Local template</small></button>";
            html+="</div>";
        }

    } else if(page_=="new"||page_=="details") {
        const bool create=page_=="new";const auto m=!create&&selected_?selected_->manifest:experiences::Manifest{};
        html+=title(create?"New Experience":"Experience Details",create?"Start from a real template. Your source files stay untouched.":"Presentation images are independent from your map and character.");
        html+="<div class=\"experience-form\"><div class=\"experience-fields\">"+field("exp-name","Name",create?"Minha experiência":m.name,"Rename without changing internal paths or IDs.");
        html+="<label class=\"experience-field\"><span>Description</span><textarea id=\"exp-description\">"+esc(m.description)+"</textarea></label>";
        if(create) {
            html+="<label class=\"experience-field\"><span>Template</span><select id=\"exp-template\">";
            for(const auto& item:manager_.templates())html+="<option value=\""+item.id+"\" "+(item.id==pendingPath_?"selected=\"selected\"":"")+">"+esc(item.name)+"</option>";
            html+="</select></label>"+field("exp-location","Local Save Location",manager_.libraryRoot().string(),"A separate directory with a stable installation ID is created inside.")+button("library-location","Choose folder…");
            html+="<label class=\"experience-inline\"><input id=\"exp-use-character\" type=\"checkbox\" checked=\"checked\"/><span>Use My Default Character, if configured</span></label>";
        } else {
            html+="<p>Experience ID: "+esc(m.id)+"</p><p>Local directory: "+esc(selected_?selected_->root.string():"")+"</p><div class=\"experience-detail-actions\">";
            html+=button("duplicate","Duplicate")+button("export","Export .lo…")+button("archive","Archive")+button("delete","Delete…");
            if(m.archived||m.trashed)html+=button("restore","Restore");
            html+="</div>";
        }
        html+="</div><div class=\"experience-media\">";
        const char* ids[]={"icon","banner"};const char* names[]={"Ícone do mapa","Banner do mapa"};
        const std::string refs[]={m.icon,m.banner};
        for(int i=0;i<2;++i){const auto path=selected_&&!create&&!refs[i].empty()?(selected_->root/refs[i]).string():"icon-image";
            html+="<div class=\"experience-media-block\"><b>"+std::string(names[i])+"</b><div class=\"experience-media-preview\"><img id=\"exp-preview-"+ids[i]+"\" src=\""+esc(path)+"\"/></div><input id=\"exp-"+ids[i]+"\" value=\""+esc(refs[i])+"\" placeholder=\"Optional image path\"/>"+button("choose-media","Choose image…",ids[i],"exp-choose-"+std::string(ids[i]))+button("remove-media","Remove",ids[i])+"<small>"+std::string(i==0?"Square recommended · 512 × 512":i==1?"16:9 recommended · 1920 × 1080":"Any aspect ratio · cover crop during entry")+"</small></div>";}
        html+="</div></div>";
        footer=button("cancel-form",returnToStudio_?"Back to Studio":"Cancel")+button(create?"create":"save-details",create?"Create & Open":"Save Details",{},create?"exp-create":"exp-save-details",true);
    } else if(page_=="settings") {
        html+=title("Configurações","Padrões editáveis e biblioteca local da engine.")+"<div class=\"experience-settings\">"+button("library-location","Local da biblioteca…")+button("edit-default","Editar mundo padrão")+button("edit-character","Editar personagem padrão")+button("restore-factory","Restaurar mundo e personagem de fábrica…")+"<p>Os padrões são recursos versionados. Projetos existentes são preservados.</p></div>";
    } else if(page_=="help")html+=title("Ajuda do LINBLUX","Local documentation for implemented features")+"<pre class=\"experience-help\">"+esc(helpText_)+"</pre>";
    document_.GetElementById("experience-content")->SetInnerRML(i18n::Markup(html));
    if(auto* actions=document_.GetElementById("experience-form-actions")){actions->SetInnerRML(i18n::Markup(footer));actions->SetProperty("display",footer.empty()?"none":"flex");}
    if(auto* main=document_.GetElementById("experience-main"))main->SetProperty("bottom",footer.empty()?"0px":"76px");
    if(renderedPage_!=page_) {if(auto* main=document_.GetElementById("experience-main"))main->SetScrollTop(0);renderedPage_=page_;}
    // Release old page thumbnails at navigation boundaries, never per frame.
    Rml::ReleaseTextures();markup_=std::move(html);dirty_=false;
}
void ExperienceUI::requestPlay() {
    if(loading()||home_||editor_.runtime_.active())return;
    editor_.endGizmoSession(true,"play");editor_.cancelRigGizmo();editor_.camera_.releaseCapture();editor_.exitGameDisplayModes();editor_.previewPlaying_=false;
    std::string error;if(!editor_.commitScriptDrafts(error)){error_=error;return;}
    pendingPath_=projectio::toJson(editor_.scene_);cancelLoad_=false;load_=LoadState::Paint;readyTime_=0;
    const auto* m=manager_.active()?&manager_.active()->manifest:nullptr;
    text(document_,"exp-loading-title",m?m->name:"Experiência LINBLUX");text(document_,"exp-loading-description",m?m->description:"Local playtest");
    text(document_,"exp-loading-status","Preparing assets and local runtime…");
    auto* icon=document_.GetElementById("exp-loading-icon");icon->SetAttribute("src",m&&!m->icon.empty()?(manager_.active()->root/m->icon).string():"linblux-logo");
    auto* background=document_.GetElementById("exp-loading-background");
    const auto ref=m?(m->loadingBackground.empty()?m->banner:m->loadingBackground):std::string{};
    if(m&&!ref.empty()) {
        const auto path=(manager_.active()->root/ref).string();ImageInfo info;
        if(ReadSafeImageInfo(path,info)) {
            background->SetAttribute("src","experience-full:"+path);const float scale=std::max(float(GetScreenWidth())/info.width,float(GetScreenHeight())/info.height);
            background->SetProperty("width",std::to_string(info.width*scale)+"px");background->SetProperty("height",std::to_string(info.height*scale)+"px");
            background->SetProperty("left",std::to_string((GetScreenWidth()-info.width*scale)/2)+"px");background->SetProperty("top",std::to_string((GetScreenHeight()-info.height*scale)/2)+"px");
        }
    } else background->RemoveAttribute("src");
}
void ExperienceUI::drawBackground() {
    if(homeVisible())background_.draw(GetScreenWidth(),GetScreenHeight());
}
void ExperienceUI::frame() {
    auto commands=std::move(commands_);commands_.clear();
    for(const auto& c:commands)try{dispatch(c.first,c.second);}catch(const std::exception& e){error_=e.what();}
    if(auto result=chooser_.poll()){if(!result->error.empty())error_=result->error;else if(result->accepted)try{selectedFile(dialogPurpose_,result->path);}catch(const std::exception& e){error_=e.what();}}
    if(load_==LoadState::Paint) {
        // First paint is submitted before any clone/physics/Lua preparation.
        if(readyTime_==0)readyTime_=GetTime();
        else {
            auto snapshot=std::move(pendingPath_);load_=LoadState::Starting;
            const auto identity=manager_.active()&&!editingEngineDefault()?"experience:"+manager_.active()->manifest.installationId:std::string();
            loadingJob_=std::async(std::launch::async,[this,snapshot=std::move(snapshot),identity] {
                try {Scene prepared;std::string error;if(!projectio::fromJson(prepared,snapshot,error))return error;
                    return editor_.runtime_.start(prepared,error,{identity})?std::string{}:error;
                }catch(const std::exception& e){return std::string(e.what());}
            });
        }
    } else if(load_==LoadState::Starting&&loadingJob_.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        const auto error=loadingJob_.get();
        if(cancelLoad_){editor_.runtime_.stop(false);load_=LoadState::None;releaseLoadingImages();}
        else if(!error.empty()){load_=LoadState::Error;text(document_,"exp-loading-status",error);}
        else {load_=LoadState::Fading;readyTime_=GetTime();text(document_,"exp-loading-status","Ready");}
    } else if(load_==LoadState::Fading) {
        const float opacity=std::clamp(1.f-float((GetTime()-readyTime_)/.14),0.f,1.f);
        document_.GetElementById("experience-loading")->SetProperty("opacity",std::to_string(opacity));
        if(opacity<=0){load_=LoadState::None;releaseLoadingImages();editor_.finishPlayStart();}
    }
    if(home_&&dirty_)try{buildPage();}catch(const std::exception& e){error_=e.what();}
    if(homeVisible()){if(!background_.running())background_.start(game::BuiltinAssetPath("midia/careditfullhd.mp4"));}
    else if(background_.running())background_.stop();
    show(document_,"experience-home",home_);show(document_,"experience-root",true);
    show(document_,"experience-file-shade",fileMenu_&&!home_);show(document_,"experience-loading",loading());
    if(load_==LoadState::Paint)document_.GetElementById("experience-loading")->SetProperty("opacity","1");
    show(document_,"experience-confirm",!confirm_.empty(),"flex");
    if(!confirm_.empty()) {
        const bool unsaved=confirm_=="unsaved";
        const bool saveDefault=confirm_=="save-default-world"||confirm_=="save-default-character";
        text(document_,"exp-confirm-title",saveDefault?"Salvar como padrão da engine?":unsaved?"Save your changes?":confirm_=="restore-factory"?"Restore factory defaults?":confirm_=="delete"?"Excluir permanentemente: "+(selected_?selected_->manifest.name:std::string{})+"?":"Archive experience?");
        text(document_,"exp-confirm-description",saveDefault?"O mundo, o personagem de jogador, suas animações, imagens e configurações serão publicados juntos no padrão da engine. Novas experiências receberão cópias. Projetos existentes serão preservados. Uma cópia do índice anterior será guardada em backups/.":unsaved?"Your map, scripts or animation has unsaved changes. Save before returning Home, discard them, or cancel.":confirm_=="restore-factory"?"Um backup do índice atual será criado antes da restauração. Novas experiências usarão o mundo e personagem de fábrica. Projetos existentes e revisões anteriores serão preservados.":confirm_=="delete"?"Todos os arquivos desta experiência serão apagados do disco. Esta operação não pode ser desfeita. Outras experiências serão preservadas.":"The selected experience and its assets remain in its own directory. It will leave My Experiences and can be restored from Archive. No external files are deleted.");
        text(document_,"exp-confirm-ok",saveDefault?"Salvar padrão":unsaved?"Save & Home":confirm_=="restore-factory"?"Restore defaults":confirm_=="delete"?"Excluir permanentemente":"Move to Archive");show(document_,"exp-confirm-discard",unsaved);
    }
    const bool special=manager_.active()&&manager_.isEngineDefault(*manager_.active());
    show(document_,"exp-header-default-save",special);
    text(document_,"exp-save-experience",special?"Salvar rascunho":"Salvar experiência");
    text(document_,"experience-error",error_);show(document_,"experience-error",!error_.empty());
    document_.SetClass("experience-compact",GetScreenWidth()<1400);
    for(const char* page:{"home","library","templates","archive"})if(auto* el=document_.GetElementById(std::string("exp-nav-")+page))el->SetClass("active",page_==page);
}
} // namespace loblox
