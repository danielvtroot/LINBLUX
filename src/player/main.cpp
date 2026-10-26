#include "rendering/ExperienceOverlay.h"
#include "experiences/ExperienceManager.h"
#include "serialization/ProjectIO.h"
#include "rendering/SceneRenderer.h"
#include "game/Runtime.h"
#include "game/ImageAssets.h"
#include "ui/ThemeColors.generated.h"
#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace loblox;
int main(int argc,char** argv) {
    std::string project;bool validate=false,checkScripts=false;int frames=0;
    for(int i=1;i<argc;++i) {
        const std::string argument=argv[i];
        if(argument=="--help"||argument=="-h") {std::puts("LINBLUX Player\nUso: linblux_player --project CAMINHO [--frames N] [--check-scripts]\n     linblux_player --validate CAMINHO\nExecuta experiências locais. Multiplayer ainda não implementado.");return 0;}
        if(argument=="--check-scripts"){checkScripts=true;continue;}
        if((argument=="--project"||argument=="--validate")&&i+1<argc){validate=argument=="--validate";project=argv[++i];}
        else if(argument=="--frames"&&i+1<argc){char* end=nullptr;const long value=std::strtol(argv[++i],&end,10);if(!end||*end||value<1||value>100000)return 2;frames=int(value);}
        else {std::fprintf(stderr,"Argumento inválido: %s\n",argument.c_str());return 2;}
    }
    if(project.empty()){std::fprintf(stderr,"Informe --project CAMINHO ou --validate CAMINHO.\n");return 2;}
    SetTraceLogLevel(LOG_WARNING);Scene authoring;std::string error,storageIdentity;
    try {
        experiences::ExperienceManager library;
        const std::filesystem::path path(project);
        if(path.extension()==".lo"){const auto experience=library.importPackage(path);library.loadReadOnly(experience,authoring);storageIdentity="experience:"+experience.manifest.installationId;}
        else if(path.filename()=="experience.json"||std::filesystem::is_directory(path)){const auto experience=library.inspect(path.filename()=="experience.json"?path.parent_path():path);library.loadReadOnly(experience,authoring);storageIdentity="experience:"+experience.manifest.installationId;}
        else {if(!projectio::load(authoring,project,error))throw std::runtime_error(error);game::SetProjectAssetRoot(std::filesystem::absolute(path).parent_path().string());storageIdentity="scene:"+std::filesystem::weakly_canonical(path).string();}
    }catch(const std::exception& e){std::fprintf(stderr,"Falha ao abrir experiência: %s\n",e.what());return 1;}
    game::RuntimeSession runtime;if(!runtime.start(authoring,error,{storageIdentity,{},!validate})){std::fprintf(stderr,"Falha na execução: %s\n",error.c_str());return 1;}
    if(validate){
        for(int i=0;i<120;++i)runtime.update(1.f/60,{});
        const bool valid=runtime.validate(error);runtime.stop();std::size_t retainedErrors=0;
        for(const auto& message:runtime.scripts().messages())if(message.error){
            ++retainedErrors;std::fprintf(stderr,"%s:%d: %s\n",message.script.c_str(),message.line,message.text.c_str());
        }
        const auto scriptErrors=runtime.scripts().errorCount();
        if(scriptErrors>retainedErrors)std::fprintf(stderr,"Erros anteriores omitidos pelo limite de registros; consulte os logs da sessão.\n");
        if(!valid)std::fprintf(stderr,"Falha de runtime: %s\n",error.c_str());
        std::printf("LINBLUX Engine: %s; erros Lua: %zu\n",valid&&scriptErrors==0?"experiência validada":"validação falhou",scriptErrors);
        return valid&&scriptErrors==0?0:1;
    }
    SetConfigFlags(FLAG_WINDOW_RESIZABLE|FLAG_VSYNC_HINT);InitWindow(1280,720,"LINBLUX Player");
    if(!IsWindowReady()){runtime.stop(false);std::fprintf(stderr,"Falha ao criar a janela gráfica do LINBLUX Player.\n");return 1;}
    SetExitKey(KEY_NULL);SetWindowMinSize(900,560);SetTargetFPS(60);
    InitAudioDevice(); // som 2D global; sem dispositivo, AudioSystem segue sem stream
    Image icon=LoadImage(game::BuiltinAssetPath("logo.png").c_str());if(icon.data){SetWindowIcon(icon);UnloadImage(icon);}
    Font font=GetFontDefault();bool customFont=false;
    for(const char* path:{"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf","/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf"})if(FileExists(path)){font=LoadFontEx(path,24,nullptr,256);customFont=font.texture.id!=0;break;}
    {
        SceneRenderer renderer;const auto logo=game::AssetTexture(game::BuiltinAssetPath("logo.png"));int rendered=0;
        const auto label=[&](const char* text,float x,float y,float size,Color color){DrawTextEx(font,text,{x,y},size,1,color);};
        while(!WindowShouldClose()&&(!frames||rendered++<frames)) {
            if(IsKeyPressed(KEY_ESCAPE)){if(runtime.state()==game::RuntimeState::Paused)runtime.resume();else runtime.pause();EnableCursor();}
            const bool paused=runtime.state()==game::RuntimeState::Paused;
            game::PlayerInput input;
            if(!paused)input.script=game::CaptureScriptInput();
            if(!paused){input.movement={float(IsKeyDown(KEY_D)-IsKeyDown(KEY_A)),float(IsKeyDown(KEY_W)-IsKeyDown(KEY_S))};input.jump=IsKeyDown(KEY_SPACE);input.shiftKey=IsKeyDown(KEY_LEFT_SHIFT);input.zoom=GetMouseWheelMove();if(!runtime.playerCamera().scripted()&&(IsMouseButtonDown(MOUSE_BUTTON_RIGHT)||runtime.shiftLockActive())){if(!IsCursorHidden())DisableCursor();input.look=GetMouseDelta();}else if(IsCursorHidden())EnableCursor();}
            runtime.update(GetFrameTime(),input);
            BeginDrawing();ClearBackground(ui::theme::background);
            RigRenderState avatar{&runtime.rig(),runtime.pose(),runtime.renderCharacterRoot()};
            renderer.drawScene(runtime.scene(),runtime.playerCamera().camera(),GetScreenWidth(),GetScreenHeight(),0,runtime.playerCamera().avatarHidden()?nullptr:&avatar,false);
            DrawExperienceOverlay(runtime,GetScreenWidth(),GetScreenHeight(),font);
            DrawRectangleRounded({16,16,245,58},.2f,8,ui::theme::panel);
            if(logo.id){BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);DrawTexturePro(logo,{0,0,float(logo.width),float(logo.height)},{24,21,48,48},{0,0},0,WHITE);EndBlendMode();}
            label("LINBLUX Player",84,35,18,ui::theme::text);
            label("Esc · Pausar",float(GetScreenWidth()-156),32,16,ui::theme::text);
            if(paused){DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),ui::theme::overlay);const float x=GetScreenWidth()*.5f-220,y=GetScreenHeight()*.5f-90;DrawRectangleRounded({x,y,440,190},.08f,8,ui::theme::panel);label("Experiência pausada",x+28,y+26,24,ui::theme::text);label("Esc para continuar · WASD para mover",x+28,y+76,17,ui::theme::secondary);const Rectangle quit{x+28,y+118,384,42};DrawRectangleRounded(quit,.12f,6,ui::theme::accent);label("Sair da experiência",x+124,y+130,18,ui::theme::text);if(CheckCollisionPointRec(GetMousePosition(),quit)&&IsMouseButtonReleased(MOUSE_BUTTON_LEFT)){EndDrawing();break;}}
            EndDrawing();
        }
        runtime.stop();game::ClearImageTextures();
    }
    if(customFont)UnloadFont(font);if(IsAudioDeviceReady())CloseAudioDevice();CloseWindow();return checkScripts&&runtime.scripts().errorCount()>0?1:0;
}
