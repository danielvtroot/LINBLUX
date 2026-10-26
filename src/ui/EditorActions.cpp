#include "ui/EditorActions.h"
#include "ui/ImageSafety.h"
#include "editor/EditorUI.h"
#include "rendering/EditorCamera.h"
#include "scene/Scene.h"
#include "objects/Lighting.h"
#include "objects/Workspace.h"
#include "game/Audio.h"
#include "game/ImageAssets.h"
#include "ui/ExperienceUI.h"
#include "serialization/ProjectIO.h"
#include "core/Diagnostics.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <stdexcept>
namespace loblox {
namespace {
float Number(const std::string& text) {size_t n=0;float v=std::stof(text,&n);if(n!=text.size()||!std::isfinite(v))throw std::runtime_error("Enter a finite number");return v;}
Color ReadColor(std::string text) {
    if(!text.empty()&&text.front()=='#')text.erase(text.begin());
    if(text.size()!=6||text.find_first_not_of("0123456789abcdefABCDEF")!=std::string::npos)throw std::runtime_error("Use a color in #RRGGBB format");
    const auto rgb=std::stoul(text,nullptr,16);return {static_cast<unsigned char>(rgb>>16),static_cast<unsigned char>(rgb>>8),static_cast<unsigned char>(rgb),255};
}
}
bool EditorActions::dispatch(const EditorCommand& c) {
    auto& e=editor_;
    try {
        if(c.action=="play")e.startPlay();
        else if(c.action=="stop")e.stopPlay();
        else if(c.action=="collision-shapes")e.showCollisionShapes_=!e.showCollisionShapes_;
        else if(c.action=="gizmo-size") {
            const float size=Number(c.value);if(size<.75f||size>1.75f)throw std::runtime_error("Gizmo size must be 0.75..1.75");
            e.endGizmoSession(true,"gizmo_size");e.cancelRigGizmo();e.gizmo_.setPixelScale(size);e.rigGizmo_.setPixelScale(size);
        }
        else if(c.action=="transform-space") {
            if(c.value!="world"&&c.value!="local")throw std::runtime_error("Choose World or Local axes");
            e.endGizmoSession(true,"space_toggle");e.cancelRigGizmo();e.clearTransformReadout();
            e.gizmoLocalSpace_=c.value=="local";e.gizmo_.setLocalSpace(e.gizmoLocalSpace_);
        }
        else if(c.action=="pause") {e.runtime_.pause();e.setGameplayCapture(false);}
        else if(c.action=="resume")e.runtime_.resume();
        else if(c.action=="respawn") {std::string error;if(!e.runtime_.respawn(error))throw std::runtime_error(error);}
        else if(c.action=="maximize-game")e.toggleGameMaximized();
        else if(c.action=="fullscreen-game")e.toggleGameFullscreen();
        else if(e.runtime_.active())return false;
        else if(c.action=="editor-camera") {
            if(c.value!="fps"&&c.value!="focus")throw std::runtime_error("Unknown Studio camera mode");
            e.camera_.setNavigationMode(c.value=="fps"?EditorCamera::NavigationMode::FPS:EditorCamera::NavigationMode::Focus);
        }
        else if(c.action=="select") {e.endGizmoSession(true,"tree_selection");e.cancelRigGizmo();e.selectedRig_.clear();if(c.value=="toggle")e.scene_.toggleSelection(e.scene_.find(c.instance));else e.scene_.select(e.scene_.find(c.instance));}
        else if(c.action=="group") {
            e.endGizmoSession(true,"group");e.cancelRigGizmo();std::string error;
            if(!e.scene_.group(e.scene_.selectedIds(),"Modelo",error))throw std::runtime_error(error);
            e.selectedRig_.clear();e.setStatus("Seleção agrupada como Modelo.");
        }
        else if(c.action=="ungroup") {
            e.endGizmoSession(true,"ungroup");std::string error;
            if(!e.scene_.selection()||!e.scene_.ungroup(e.scene_.selection()->id(),error))throw std::runtime_error(error.empty()?"Selecione um Modelo.":error);
            e.setStatus("Modelo desagrupado; transformações preservadas.");
        }
        else if(c.action=="rig-select")e.selectRig(c.value);
        else if(c.action=="tool") {const int tool=std::stoi(c.value);if(tool<0||tool>3)throw std::runtime_error("Invalid tool");e.setTool(static_cast<ToolMode>(tool),"rml_ribbon");}
        else if(c.action=="part")e.insertPart(PartShape::Block);
        else if(c.action=="sphere")e.insertPart(PartShape::Sphere);
        else if(c.action=="spawn")e.insertSpawnLocation();
        else if(c.action=="folder")e.insertContainer(InstanceClass::Folder,e.scene_.find(c.instance));
        else if(c.action=="model")e.insertContainer(InstanceClass::Model,e.scene_.find(c.instance));
        else if(c.action=="script")e.insertScript(InstanceClass::Script);
        else if(c.action=="local-script")e.insertScript(InstanceClass::LocalScript);
        else if(c.action=="module")e.insertScript(InstanceClass::ModuleScript);
        else if(c.action=="open-script")e.openScript(c.instance);
        else if(c.action=="activate-document")e.activateDocument(InstanceId(std::stoull(c.value)));
        else if(c.action=="close-document")e.closeDocument(InstanceId(std::stoull(c.value)));
        else if(c.action=="r6")e.insertR6();
        else if(c.action=="duplicate")e.duplicateSelection();
        else if(c.action=="delete")e.deleteSelection();
        else if(c.action=="focus")e.focusSelection();
        else if(c.action=="copy")e.clipboardId_=e.scene_.selection()?e.scene_.selection()->id():0;
        else if(c.action=="paste") {if(auto* source=e.scene_.find(e.clipboardId_)) {auto* p=e.scene_.selection();if(p && p->isPart())p=p->parent();if(auto* copy=e.scene_.duplicateTree(*source,p))e.scene_.select(copy);}}
        else if(c.action=="save")return e.saveCurrentProject();
        else if(c.action=="save-as") {std::string error;if(!e.saveProjectAt(c.value,error))throw std::runtime_error(error);e.setProjectPath(c.value);}
        else if(c.action=="open") {std::string error;e.endGizmoSession(true,"load");e.cancelRigGizmo();if(!projectio::load(e.scene_,c.value,error))throw std::runtime_error(error);
            e.scriptDrafts_.clear();e.openDocuments_.clear();e.activeScript_=e.activeDocument_=e.requestedScript_=0;e.selectedRig_.clear();e.animationSource_.clear();
            e.previewPlaying_=false;e.previewPose_={};e.animationDirty_=false;e.timelineDragJoint_=-1;e.hasCopiedKey_=false;e.clipboardId_=0;
            e.camera_.releaseCapture();e.setProjectPath(c.value);if(e.onSceneLoaded_)e.onSceneLoaded_();e.checkInvariants("rml_load");
        }
        else if(c.action=="rename") {if(c.value.empty()||c.value.size()>128)throw std::runtime_error("Name must contain 1..128 bytes");if(auto* node=e.scene_.find(c.instance))node->setName(c.value);else if(auto* rig=e.scene_.gameAssets().findRig(e.selectedRig_))rig->name=c.value;}
        else if(c.action=="reparent") {std::string error;if(!e.scene_.reparent(e.scene_.find(c.instance),e.scene_.find(std::stoull(c.value)),error))throw std::runtime_error(error);}
        else if(c.action=="snap") {e.endGizmoSession(true,"snap");e.cancelRigGizmo();e.snapEnabled_=c.value=="true";}
        else if(c.action=="move-snap"||c.action=="scale-snap"||c.action=="rotate-snap") {float v=Number(c.value);if(v<=0||v>180)throw std::runtime_error("Snap increment must be 0..180");e.endGizmoSession(true,"snap");e.cancelRigGizmo();(c.action=="move-snap"?e.moveSnap_:c.action=="scale-snap"?e.scaleSnap_:e.rotateSnapDegrees_)=v;}
        else if(c.action=="anchor") {if(auto* node=e.scene_.selection();node&&node->isPart())static_cast<Part*>(node)->setAnchored(!static_cast<Part*>(node)->anchored());}
        else if(c.action=="import-image") {
            if(!ui::SafeImageFile(c.value))throw std::runtime_error("Use a PNG/JPEG within the 32 MiB file / 64 MiB decoded budget");
            std::string asset,error;if(!game::ImportImageTo(game::AssetsDir(),c.value,asset,error))throw std::runtime_error(error);
            e.setStatus("Image imported: "+asset);
        }
        else if(c.action=="import-sound") {
            std::string asset,error;
            if(!game::ImportMediaTo(game::AssetsDir(),c.value,game::MediaKind::Audio,asset,error))throw std::runtime_error(error);
            double seconds=0;
            if(!game::ProbeAudio(asset,seconds,error))throw std::runtime_error(error);
            const std::string reference="assets/"+std::filesystem::path(asset).filename().string();
            auto& sound=e.scene_.gameAssets().addSound(reference);
            e.setStatus("Sound imported: "+sound.name);
        }
        else if(c.action=="remove-sound") {
            auto& sounds=e.scene_.gameAssets().sounds;
            const auto it=std::find_if(sounds.begin(),sounds.end(),[&](const game::SoundAsset& sound){
                return sound.reference==c.value||std::filesystem::path(sound.reference).filename().string()==c.value;});
            if(it==sounds.end())throw std::runtime_error("Sound is no longer in this project's library");
            const std::string reference=it->reference;
            e.scene_.gameAssets().removeSound(it->id);
            if(game::SoundInUse(e.scene_,reference))throw std::runtime_error("Sound is still used by a script or attribute");
            if(e.experienceUi_)e.experienceUi_->removeAsset(reference,false);
            else {std::string error;if(!game::RemoveAssetFile(game::AssetsDir(),std::filesystem::path(reference).filename().string(),false,error))throw std::runtime_error(error);}
            e.setStatus("Sound removed: "+reference);
        }
        else if(c.action=="remove-image"||c.action=="remove-asset") {
            if(!e.experienceUi_)throw std::runtime_error("Abra uma experiência antes de remover imagens importadas");
            e.experienceUi_->removeAsset(c.value,game::ImageInUse(e.scene_,c.value)||game::SoundInUse(e.scene_,c.value));
            e.setStatus("Asset removido: "+c.value);
        }
        else if(c.action.rfind("rename-image:",0)==0) {
            std::string error;
            if(!game::RenameImageAsset(game::AssetsDir(),c.action.substr(13),c.value,error))throw std::runtime_error(error);
            e.setStatus("Imagem renomeada: "+c.value);
        }
        else if(c.action.rfind("replace-image:",0)==0) {
            const std::string oldFile=std::filesystem::path(c.action.substr(14)).filename().string();
            if(!ui::SafeImageFile(c.value))throw std::runtime_error("Use a PNG/JPEG within the 32 MiB file / 64 MiB decoded budget");
            std::string asset,error;if(!game::ImportImageTo(game::AssetsDir(),c.value,asset,error))throw std::runtime_error(error);
            const std::string reference=game::ImageAssetReference(asset);
            // A troca reescreve as referências existentes; nada aponta para o
            // arquivo antigo e nenhum objeto perde a superfície.
            std::size_t replaced=game::ReplaceImageReferences(e.scene_,oldFile,reference);
            if(e.experienceUi_)replaced+=e.experienceUi_->replaceImageReferences(oldFile,reference);
            e.setStatus("Imagem substituída em "+std::to_string(replaced)+" referência(s).");
        }
        else if(c.action.rfind("delete-image:",0)==0) {
            const std::string file=std::filesystem::path(c.action.substr(13)).filename().string();
            std::vector<std::string> uses;
            for(const auto& use:game::DescribeImageUsage(e.scene_,file))uses.push_back(use.where);
            if(e.experienceUi_)for(const auto& use:e.experienceUi_->presentationUses(file))uses.push_back(use);
            if(!uses.empty()) {
                std::string message="A imagem ainda é usada em: ";
                for(std::size_t i=0;i<uses.size();++i)message+=(i?" · ":"")+uses[i];
                throw std::runtime_error(message+". Remova ou substitua essas referências antes de excluir.");
            }
            if(!e.saveCurrentProject())throw std::runtime_error("A imagem foi preservada porque a experiência não pôde ser salva.");
            if(e.experienceUi_)e.experienceUi_->removeAsset(file,false);
            else {std::string error;if(!game::RemoveAssetFile(game::AssetsDir(),file,false,error))throw std::runtime_error(error);}
            e.setStatus("Imagem excluída da experiência: "+file);
        }
        else if(c.action=="apply-surface-image") {
            auto* node=e.scene_.selection();if(!node||!IsSurfaceClass(node->instanceClass()))throw std::runtime_error("Selecione um adesivo ou textura.");
            if(game::ResolveImagePath(game::AssetsDir(),c.value).empty())throw std::runtime_error("Imagem ausente.");
            auto& surface=static_cast<Surface&>(*node);auto value=surface.properties();value.image=game::ImageAssetReference(c.value);
            if(!surface.setProperties(value))throw std::runtime_error("Propriedades de superfície inválidas.");
        }
        else if(c.action=="apply-image"||c.action=="apply-texture") {
            auto* part=e.scene_.selection();if(!part||!part->isPart())throw std::runtime_error("Select a Part before applying an image");
            const auto path=game::ResolveImagePath(game::AssetsDir(),c.value);if(path.empty())throw std::runtime_error("Image asset missing");
            part->setAttribute(c.action=="apply-image"?"decal_image":"texture_image",game::ImageReference(path));
            e.setStatus("Image applied to "+part->name());
        }
        else if(c.action.rfind("game:",0)==0) {
            auto settings=e.scene_.gameAssets().player;const auto field=c.action.substr(5);
            const std::pair<const char*,float game::PlayerSettings::*> numeric[]={
                {"WalkSpeed",&game::PlayerSettings::walkSpeed},{"JumpHeight",&game::PlayerSettings::jumpHeight},{"Gravity",&game::PlayerSettings::gravity},
                {"Acceleration",&game::PlayerSettings::acceleration},{"Deceleration",&game::PlayerSettings::deceleration},{"TurnSpeed",&game::PlayerSettings::turnSpeed},
                {"MaxHealth",&game::PlayerSettings::maxHealth},{"InitialHealth",&game::PlayerSettings::health},{"RespawnDelay",&game::PlayerSettings::respawnDelay},
                {"MinZoom",&game::PlayerSettings::minZoom},{"MaxZoom",&game::PlayerSettings::maxZoom},{"CameraDistance",&game::PlayerSettings::cameraDistance},
                {"Sensitivity",&game::PlayerSettings::sensitivity},{"CollisionHeight",&game::PlayerSettings::collisionHeight},{"CollisionRadius",&game::PlayerSettings::collisionRadius}};
            bool known=false;for(const auto& item:numeric)if(field==item.first){settings.*item.second=Number(c.value);known=true;break;}
            if(field=="AutoRotate")settings.autoRotate=c.value=="true";
            else if(field=="ShiftLock")settings.enableShiftLock=c.value=="true";
            else if(field=="FirstPerson")settings.allowFirstPerson=c.value=="true";
            else if(field=="CameraMode")settings.cameraMode=static_cast<game::CameraMode>(std::stoi(c.value));
            else if(field=="DefaultRig") {if(!e.scene_.gameAssets().findRig(c.value))throw std::runtime_error("Rig not found");e.scene_.gameAssets().defaultRig=c.value;return true;}
            else if(field=="Idle"||field=="Walk"||field=="Jump"||field=="Fall") {
                if(!c.value.empty()&&!e.scene_.gameAssets().findAnimation(c.value))throw std::runtime_error("Animation ID not found");
                (field=="Idle"?settings.idleAnimation:field=="Walk"?settings.walkAnimation:field=="Jump"?settings.jumpAnimation:settings.fallAnimation)=c.value;
            }else if(!known)throw std::runtime_error("Unknown Game Config field");
            std::string error;if(!settings.validate(error))throw std::runtime_error(error);e.scene_.gameAssets().player=settings;
            if(field=="Gravity")e.physics_.setGravity({0,-settings.gravity,0});
        }
        else if(c.action.rfind("property:",0)==0) {
            auto* node=e.scene_.find(c.instance);if(!node)return false;const auto name=c.action.substr(9);
            if(name=="Name") {if(c.value.empty()||c.value.size()>128)throw std::runtime_error("Invalid name");node->setName(c.value);}
            else if(name=="Parent") {std::string error;if(!e.scene_.reparent(node,e.scene_.find(std::stoull(c.value)),error))throw std::runtime_error(error);}
            else if(name.rfind("Attribute:",0)==0) {
                const auto key=name.substr(10);const auto it=node->attributes().find(key);if(it==node->attributes().end())throw std::runtime_error("Attribute no longer exists");
                if(std::holds_alternative<double>(it->second))node->setAttribute(key,double(Number(c.value)));
                else if(std::holds_alternative<bool>(it->second)){if(c.value!="true"&&c.value!="false")throw std::runtime_error("Use true or false");node->setAttribute(key,c.value=="true");}
                else node->setAttribute(key,c.value);
            }
            else if(node->instanceClass()==InstanceClass::Model&&(name.rfind("Model.",0)==0||name.rfind("Pivot.",0)==0)) {
                ModelFrame frame;std::string error;if(!e.scene_.modelFrame(node->id(),frame,error))throw std::runtime_error(error);
                if(name=="Model.Scale")frame.scale=Number(c.value);
                else {
                    const auto split=name.find_last_of('.');const int axis=std::stoi(name.substr(split+1));if(axis<0||axis>2)throw std::runtime_error("Eixo do Modelo inválido.");
                    const float value=Number(c.value);
                    if(name.find(".Position.")!=std::string::npos)(&frame.position.x)[axis]=value;
                    else if(name.find(".Rotation.")!=std::string::npos){auto angles=EulerDegreesFromQuat(frame.rotation);(&angles.x)[axis]=value;frame.rotation=QuatFromEulerDegrees(angles);}
                    else throw std::runtime_error("Propriedade do Modelo inválida.");
                }
                e.endGizmoSession(true,"model_property");
                const bool ok=name.rfind("Pivot.",0)==0?e.scene_.setModelPivot(node->id(),frame,error):e.scene_.transformModel(node->id(),frame,error);
                if(!ok)throw std::runtime_error(error);e.physics_.syncScene(e.scene_);
            }
            else if(IsSurfaceClass(node->instanceClass())&&name.rfind("Surface.",0)==0) {
                auto& surface=static_cast<Surface&>(*node);auto value=surface.properties();const float number=Number(c.value);
                if(name=="Surface.Face"){if(number<0||number>5||std::floor(number)!=number)throw std::runtime_error("Face deve ser inteira de 0 a 5.");value.face=int(number);}
                else if(name=="Surface.Transparency")value.transparency=number;else if(name=="Surface.Rotation")value.rotation=number;
                else if(name=="Surface.OffsetU")value.offsetU=number;else if(name=="Surface.OffsetV")value.offsetV=number;
                else if(name=="Surface.TileU")value.studsPerTileU=number;else if(name=="Surface.TileV")value.studsPerTileV=number;
                else throw std::runtime_error("Propriedade de superfície desconhecida.");
                if(!surface.setProperties(value))throw std::runtime_error("Propriedades de superfície fora dos limites.");
            }
            else if(node->isPart()) {
                auto& p=*static_cast<Part*>(node);e.endGizmoSession(true,"property");
                if(name=="Anchored")p.setAnchored(c.value=="true");
                else if(name=="CanCollide")p.setCanCollide(c.value=="true");
                else if(name=="Density"||name=="Friction"||name=="Restitution"||name=="LinearDamping"||name=="AngularDamping") {
                    auto body=p.bodyProperties();const float value=Number(c.value);
                    if(name=="Density")body.density=value;else if(name=="Friction")body.friction=value;
                    else if(name=="Restitution")body.restitution=value;else if(name=="LinearDamping")body.linearDamping=value;else body.angularDamping=value;
                    std::string error;if(!p.setBodyProperties(body,error))throw std::runtime_error(error);
                }
                else if(name=="Spawn") {
                    if(c.value=="true"||c.value=="1") {if(!p.setAttribute("spawn",true))throw std::runtime_error("Attribute limit reached");}
                    else p.removeAttribute("spawn");
                }
                else if(name=="SpawnPriority") {
                    const float value=Number(c.value);
                    if(value<-1000000.f||value>1000000.f)throw std::runtime_error("Spawn priority must be between -1000000 and 1000000");
                    if(!p.setAttribute("spawn_priority",double(value)))throw std::runtime_error("Attribute limit reached");
                }
                else if(name=="Material")p.setMaterial(PartMaterialFromIndex(std::stoi(c.value)));
                else if(name=="Color") {auto color=ReadColor(c.value);color.a=p.color().a;p.setColor(color);}
                else if(name=="Transparency") {const float v=Number(c.value);if(v<0||v>1)throw std::runtime_error("Transparency must be 0..1");auto color=p.color();color.a=static_cast<unsigned char>(std::lround((1-v)*255));p.setColor(color);}
                else if(name.rfind("decal_",0)==0||name.rfind("texture_",0)==0) {
                    if(name.find("transparency")!=std::string::npos) {const float v=Number(c.value);if(v<0||v>1)throw std::runtime_error("Transparency must be 0..1");p.setAttribute(name,double(v));}
                    else if(name.find("rotation")!=std::string::npos||name.find("repeat")!=std::string::npos||name.find("offset")!=std::string::npos)p.setAttribute(name,double(Number(c.value)));
                    else if(name.find("face")!=std::string::npos) {if(c.value!="Front"&&c.value!="Back"&&c.value!="Left"&&c.value!="Right"&&c.value!="Top"&&c.value!="Bottom")throw std::runtime_error("Invalid face");p.setAttribute(name,c.value);}
                    else p.setAttribute(name,c.value);
                }
                else {const auto split=name.find('.');int axis=std::stoi(name.substr(split+1));if(axis<0||axis>2)throw std::runtime_error("Invalid axis");const auto field=name.substr(0,split);auto t=p.transform();Vector3 v=field=="Position"?t.position:field=="Size"?t.size:p.rotationEulerDegrees();(&v.x)[axis]=Number(c.value);if(field=="Size"&&(v.x<.2f||v.y<.2f||v.z<.2f))throw std::runtime_error("Minimum size is 0.2");if(field=="Position")p.setPosition(v);else if(field=="Size")p.setSize(v);else p.setRotationEulerDegrees(v);}
                e.physics_.applyChanges(&p);
            } else if(node==e.scene_.lighting()) {
                auto* l=e.scene_.lighting();
                if(name=="SkyEnabled")l->setSkyEnabled(c.value=="true"||c.value=="1");
                else if(name=="SkyZenithColor")l->setSkyZenithColor(ReadColor(c.value));else if(name=="SkyHorizonColor")l->setSkyHorizonColor(ReadColor(c.value));
                else if(name=="SkyGroundColor")l->setSkyGroundColor(ReadColor(c.value));else if(name=="SunColor")l->setSunColor(ReadColor(c.value));else if(name=="FogColor")l->setFogColor(ReadColor(c.value));
                else if(name.rfind("SunDirection.",0)==0) {const int axis=std::stoi(name.substr(13));if(axis<0||axis>2)throw std::runtime_error("Invalid axis");auto direction=l->sunDirection();(&direction.x)[axis]=Number(c.value);l->setSunDirection(direction);}
                else {const float v=Number(c.value);
                if(name=="SunIntensity")l->setSunIntensity(v);
                else if(name=="Brightness")l->setBrightness(v);else if(name=="FogDensity")l->setFogDensity(v);
                else if(name=="CloudCoverage")l->setCloudDensity(v);else if(name=="SkyRotation")l->setSkyRotation(v);
                else if(name=="CloudScale")l->setCloudScale(v);else if(name=="CloudSpeed")l->setCloudSpeed(v);else if(name=="CloudSoftness")l->setCloudSoftness(v);else if(name=="SunSize")l->setSunSize(v);
                }
            } else if(node==e.scene_.workspace() && name=="VoidHeight")e.scene_.workspace()->setFallenPartsDestroyHeight(Number(c.value));
            else if(IsScriptClass(node->instanceClass()) && name=="Enabled")static_cast<Script*>(node)->enabled=c.value=="true";
        } else return false;
        diag::Event event;event.subsystem="ui";event.name="command";event.entityId=c.instance;event.context={{"source","rml"},{"action",c.action}};event.result="ok";diag::log(event);
        return true;
    } catch(const std::exception& error) {e.setStatus(error.what(),true);diag::Event event;event.level=diag::Level::Error;event.subsystem="ui";event.name="command";event.entityId=c.instance;event.context={{"source","rml"},{"action",c.action}};event.result="error";event.error=error.what();diag::log(event);return false;}
}
}
