#include "experiences/ExperienceManager.h"
#include "game/ImageAssets.h"
#include "game/Runtime.h"
#include "core/ImageInfo.h"
#include "serialization/ProjectIO.h"
#include "nlohmann/json.hpp"
#include <cstdio>
#include <fstream>
#include <functional>
#include <filesystem>
#include <zlib.h>

using namespace loblox;
namespace fs=std::filesystem;
namespace ex=loblox::experiences;
int checks=0,failures=0;
#define CHECK(x) do{++checks;if(!(x)){++failures;std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);}}while(0)
bool Throws(const std::function<void()>& action){try{action();return false;}catch(const std::exception&){return true;}}
std::string Read(const fs::path& file){std::ifstream f(file,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void Integer(std::ostream& out,uint64_t n,int bytes){for(int i=0;i<bytes;++i)out.put(char(n>>(i*8)));}
void Malicious(const fs::path& file,const std::vector<std::pair<std::string,std::string>>& entries,uint64_t fakeSize=0) {
    std::ofstream f(file,std::ios::binary);f.write("LOEXP\1\r\n",8);Integer(f,entries.size(),4);
    for(const auto& item:entries){Integer(f,item.first.size(),2);Integer(f,fakeSize?fakeSize:item.second.size(),8);Integer(f,crc32(0,reinterpret_cast<const Bytef*>(item.second.data()),item.second.size()),4);f<<item.first<<item.second;}
}
int main(int argc,char** argv) {
    SetTraceLogLevel(LOG_WARNING);
    if(argc==3&&std::string(argv[1])=="--factory"){ex::ExperienceManager::writeFactoryResources(argv[2]);return 0;}
    const auto base=fs::temp_directory_path()/("loblox-experiences-"+ex::newId());fs::create_directories(base);
    try {
        const auto factory=base/"factory";ex::ExperienceManager::writeFactoryResources(factory);
        ex::ExperienceManager author(base/"alice/data",factory),receiver(base/"other-user/data",factory);
        CHECK(author.templates().size()==4);CHECK(author.list().empty());
        ex::CreateOptions invalid;invalid.name="Bad image";invalid.icon=(base/"missing.png").string();
        CHECK(Throws([&]{author.create(invalid);}));CHECK(author.list().empty());
        for(const auto& entry:fs::directory_iterator(author.libraryRoot()))CHECK(entry.path().filename().string().rfind(".staging-",0)!=0);
        const auto cwd=fs::current_path();
        Image image=GenImageColor(5001,8,{90,175,210,255});CHECK(ExportImage(image,(base/"wide.png").c_str()));UnloadImage(image);
        fs::copy_file(fs::path(__FILE__).parent_path()/"fixtures/image-test.jpg",base/"small.jpg");
        ImageInfo info;CHECK(ReadSafeImageInfo((base/"wide.png").string(),info));CHECK(info.width==5001);CHECK(info.height==8);
        ex::CreateOptions options;options.name="My map / keeps its name";options.description="Independent icon, banner and loading";
        options.icon=(base/"small.jpg").string();options.banner=(base/"wide.png").string();options.loadingBackground=options.icon;
        auto project=author.create(options);CHECK(project.manifest.id!=project.manifest.installationId);CHECK(author.list().size()==1);
        CHECK(!project.manifest.icon.empty());CHECK(project.manifest.icon!=project.manifest.banner);
        Scene scene;author.open(project,scene);CHECK(scene.parts().size()==2);CHECK(fs::current_path()==cwd);
        auto& rig=scene.gameAssets().createR6();const auto rigId=rig.id;rig.bodies[game::RightArm].color=RED;
        auto& decal=rig.addDecal(game::Torso);decal.surface.image=project.manifest.banner;decal.surface.face=1;
        auto* part=scene.createPart(PartShape::Block,{7,4,3},{2,3,4},"PortablePart");part->setRotationEulerDegrees({0,37,0});
        part->setAttribute("decal_image",project.manifest.icon);const auto partId=part->id();
        const auto previous=author.activePlace();author.save(scene);CHECK(author.activePlace()!=previous);CHECK(fs::exists(previous));
        auto m=author.active()->manifest;m.name="Renamed";m.description="Updated description";author.updateMetadata(m);
        const auto goodManifest=Read(project.root/"experience.json");auto badMedia=m;badMedia.icon="assets/missing.png";
        CHECK(Throws([&]{author.updateMetadata(badMedia);}));CHECK(Read(project.root/"experience.json")==goodManifest);
        CHECK(author.active()->root==project.root);CHECK(author.active()->manifest.id==project.manifest.id);
        Scene reopened;author.open(*author.active(),reopened);CHECK(reopened.findPart(partId));CHECK(reopened.gameAssets().findRig(rigId));
        CHECK(reopened.gameAssets().findRig(rigId)->bodies[game::Torso].decals.size()==1);
        const auto originalWorld=projectio::toJson(reopened);
        auto copy=author.duplicate(*author.active(),"Copy");CHECK(copy.root!=project.root);CHECK(copy.manifest.id!=project.manifest.id);
        CHECK(copy.manifest.icon==author.active()->manifest.icon);CHECK(fs::exists(copy.root/copy.manifest.icon));
        author.setArchived(copy,true);CHECK(author.list(true).size()==1);author.setArchived(copy,false);CHECK(author.list(true).empty());
        author.setArchived(copy,true,true);CHECK(author.inspect(copy.root).manifest.trashed);author.setArchived(copy,false);CHECK(!author.inspect(copy.root).manifest.trashed);
        author.setDefaultTemplate(reopened);author.setDefaultCharacter(reopened,rigId);
        auto customOptions=options;customOptions.name="From my default";customOptions.templateId="my-default";
        auto custom=author.create(customOptions);Scene customScene;author.open(custom,customScene);
        CHECK(customScene.findPart(partId));CHECK(customScene.gameAssets().defaultRig==rigId);
        CHECK(customScene.gameAssets().findRig(rigId)->bodies[game::RightArm].color.r==RED.r);
        CHECK(customScene.gameAssets().findRig(rigId)->bodies[game::Torso].decals[0].surface.face==1);
        author.restoreFactoryDefaults();CHECK(author.templates().size()==4);
        CHECK(Throws([&]{author.create(customOptions);}));
        const auto portable=base/"Shared.lo";author.exportPackage(custom,portable);
        CHECK(fs::file_size(portable)>100);CHECK(Read(portable).substr(0,5)=="LOEXP");
        auto received=receiver.importPackage(portable);CHECK(received.manifest.id==custom.manifest.id);CHECK(received.manifest.installationId!=custom.manifest.installationId);
        CHECK(!received.manifest.faceImage.empty());CHECK(fs::is_regular_file(received.root/received.manifest.faceImage));
        author.close();Scene receivedScene;receiver.open(received,receivedScene);
        CHECK(receivedScene.findPart(partId));CHECK(receivedScene.gameAssets().findRig(rigId));
        CHECK(fs::current_path()==cwd);CHECK(!game::ResolveImagePath(game::AssetsDir(),received.manifest.banner).empty());
        CHECK(projectio::toJson(receivedScene).find(base.string())==std::string::npos);
        auto again=receiver.importPackage(portable);CHECK(again.root!=received.root);CHECK(receiver.list().size()==2);
        const auto movedDirectory=base/"moved-outside-library";fs::rename(again.root,movedDirectory);
        CHECK(receiver.missing().size()==1);CHECK(!fs::exists(again.root));
        CHECK(Throws([&]{receiver.relocate(again.manifest.installationId,received.root);}));
        const auto relocated=receiver.relocate(again.manifest.installationId,movedDirectory);CHECK(relocated.root==movedDirectory);CHECK(receiver.missing().empty());
        fs::rename(movedDirectory,base/"temporarily-unavailable");receiver.forgetMissing(again.manifest.installationId);CHECK(receiver.missing().empty());CHECK(fs::is_directory(base/"temporarily-unavailable"));
        // Legacy import makes an independent copy and never rewrites the source.
        const auto legacy=base/"legacy.loblox.json";std::string error;CHECK(projectio::save(receivedScene,legacy.string(),error));
        fs::create_directories(base/"assets");for(const auto& file:fs::directory_iterator(received.root/"assets"))fs::copy_file(file.path(),base/"assets"/file.path().filename(),fs::copy_options::overwrite_existing);
        const auto before=Read(legacy);auto migrated=receiver.importLegacy(legacy,"Legacy imported");CHECK(Read(legacy)==before);CHECK(fs::exists(migrated.root/migrated.manifest.mainPlace));
        // Untrusted archives: traversal, absolute paths, duplicate entries,
        // malformed manifests, oversized declarations, truncation and CRC.
        const auto attack=base/"attack.lo";
        for(const auto& path:{"../escape","/tmp/escape","assets/../../escape","assets\\escape","assets/./escape","assets//escape"}) {
            Malicious(attack,{{path,"bad"}});CHECK(Throws([&]{receiver.importPackage(attack);}));
        }
        Malicious(attack,{{"experience.json","{}"},{"experience.json","{}"}});CHECK(Throws([&]{receiver.importPackage(attack);}));
        Malicious(attack,{{"experience.json","{}"}},uint64_t(1)<<40);CHECK(Throws([&]{receiver.importPackage(attack);}));
        Malicious(attack,{{"experience.json","{}"}});CHECK(Throws([&]{receiver.importPackage(attack);}));
        {std::ofstream f(attack);f<<"PK ZIP unsupported";}CHECK(Throws([&]{receiver.importPackage(attack);}));
        const auto safeManifest=Read(custom.root/"experience.json");
        Malicious(attack,{{"experience.json",safeManifest},{"assets/missing.png","invalid"}});CHECK(Throws([&]{receiver.importPackage(attack);}));
        Malicious(attack,{{"experience.json",std::string(70,'[')+std::string(70,']')}});CHECK(Throws([&]{receiver.importPackage(attack);}));
        Malicious(attack,{{"experience.json",safeManifest}},0);{std::ofstream f(attack,std::ios::app);f<<"trailing";}CHECK(Throws([&]{receiver.importPackage(attack);}));
        auto corrupt=Read(portable);corrupt.back()^=1;{std::ofstream f(attack,std::ios::binary);f<<corrupt;}
        CHECK(Throws([&]{receiver.importPackage(attack);}));
        {std::ofstream f(attack,std::ios::binary);f<<corrupt.substr(0,30);}CHECK(Throws([&]{receiver.importPackage(attack);}));
        CHECK(!fs::exists(base/"escape"));
        for(const auto& item:fs::directory_iterator(receiver.libraryRoot()))CHECK(item.path().filename().string().rfind(".staging-",0)!=0);
        fs::create_symlink(base/"wide.png",received.root/"assets/unsafe.png");CHECK(Throws([&]{receiver.exportPackage(received,base/"unsafe.lo");}));
        receiver.close();CHECK(game::ProjectAssetRoot().empty());
        fs::create_directory(base/"outside-cache");fs::create_directory_symlink(base/"outside-cache",received.root/".cache");CHECK(Throws([&]{receiver.thumbnail(received);}));CHECK(fs::is_empty(base/"outside-cache"));
        CHECK(originalWorld.find("PortablePart")!=std::string::npos);
        // Reproduce the old whole-rig bug: the torso was moved in restPose,
        // while the root/debug anchor stayed far away. The posed joint must
        // remain attached, and whole-rig edits must never add another offset.
        auto authored=game::MakeR6();authored.restPose[game::RootJoint].position={70,2,-5};
        const auto initial=authored;const auto world=initial.evaluate({},initial.root);
        CHECK(Vector3Distance(authored.pivot(game::RootJoint,{}).position,world[game::Torso].position)<.0001f);
        auto target=authored.authoringPivot();target.position=Vector3Add(target.position,{5,3,0});
        CHECK(authored.transformAuthoring(target,1,error));
        const auto moved=authored.evaluate({},authored.root);
        for(int b=0;b<game::kBodyCount;++b)CHECK(NearlyEqual(moved[b].position,Vector3Add(world[b].position,{5,3,0}),.0001f));
        CHECK(NearlyEqual(authored.restPose[game::RootJoint].position,initial.restPose[game::RootJoint].position));
        const auto beforeCenter=authored.evaluate({},authored.root);CHECK(authored.recenterAuthoringRoot(error));
        const auto centered=authored.evaluate({},authored.root);
        for(int b=0;b<game::kBodyCount;++b)CHECK(NearlyEqual(centered[b].position,beforeCenter[b].position,.0001f));
        CHECK(NearlyEqual(authored.root.position,centered[game::Torso].position,.0001f));
        const auto start=authored;target=authored.authoringPivot();target.rotation=QuatFromEulerDegrees({0,37,0});
        CHECK(authored.transformAuthoring(target,1.2f,error));
        const auto scaled=authored.evaluate({},authored.root);const auto unscaled=start.evaluate({},start.root);const auto pivot=start.authoringPivot();
        for(int b=0;b<game::kBodyCount;++b){const auto expected=Vector3Add(target.position,Vector3RotateByQuaternion(Vector3Scale(Vector3Subtract(unscaled[b].position,pivot.position),1.2f),target.rotation));CHECK(NearlyEqual(expected,scaled[b].position,.0001f));}
        Scene playScene;playScene.buildDefaultScene();playScene.gameAssets().createR6();playScene.gameAssets().rigs={initial};playScene.gameAssets().defaultRig=initial.id;
        game::RuntimeSession runtime;const bool started=runtime.start(playScene,error);if(!started)std::fprintf(stderr,"Rig runtime: %s\n",error.c_str());CHECK(started);
        const auto visible=runtime.rig().evaluate(runtime.pose(),runtime.characterRoot());
        CHECK(Vector3Distance(visible[game::Torso].position,runtime.characterRoot().position)<.001f);
        CHECK(NearlyEqual(playScene.gameAssets().rigs[0].restPose[game::RootJoint].position,initial.restPose[game::RootJoint].position));runtime.stop();
    } catch(const std::exception& error){++failures;std::fprintf(stderr,"Unexpected: %s\n",error.what());}
    game::SetProjectAssetRoot({});
    std::printf("Experiences: %d checks, %d failures; fixture: %s\n",checks,failures,base.c_str());
    return failures?1:0;
}
