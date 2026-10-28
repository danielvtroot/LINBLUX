#pragma once
#include "serialization/ProjectIO.h"
#include <fstream>
#include <iomanip>
// Uses the authored character delivered with LINBLUX, including its real rig.
// Environment-selected capture helper; not run as part of every regression.
inline int CaptureCharacterShadows(const char* directory) {
    namespace fs=std::filesystem;const fs::path out=directory;fs::create_directories(out);
    const auto root=fs::path(__FILE__).parent_path().parent_path();
    SetTraceLogLevel(LOG_WARNING);InitWindow(640,400,"LINBLUX character shadow acceptance");
    if(!IsWindowReady())return 77;
    int failures=0;
    {
    Scene scene;std::string error;
    if(!projectio::load(scene,(root/"assets/defaults/factory/character.loblox.json").string(),error)){std::fprintf(stderr,"Character load: %s\n",error.c_str());CloseWindow();return 1;}
    if(scene.gameAssets().rigs.empty()){CloseWindow();return 1;}
    auto& rig=scene.gameAssets().rigs.front();rig.root={{0,3,0},{0,0,0,1}};
    float sole=INFINITY;const auto bodyWorld=rig.evaluate({},rig.root);
    for(int i=0;i<game::kBodyCount;++i)for(const auto p:OrientedBox{bodyWorld[i].position,bodyWorld[i].rotation,HalfExtents(rig.bodies[i].size)}.corners())sole=std::min(sole,p.y);
    rig.root.position.y-=sole;
    auto* floor=scene.createPart(PartShape::Block,{0,-.5f,0},{160,1,160},"Piso do mundo distribuido");floor->setColor({190,190,190,255});floor->setAnchored(true);
    scene.lighting()->setSkyEnabled(false);scene.lighting()->setSunDirection({.7f,-1,.4f});scene.lighting()->setSunIntensity(1);scene.lighting()->setFogDensity(0);
    Camera3D camera{{8,6,12},{0,1.5f,0},{0,1,0},45,CAMERA_PERSPECTIVE};
    SceneRenderer renderer;
    std::ofstream stats(out/"measurements.csv");stats<<"width,height,quality,aa,active,resolution,span,texel,normal_bias,ms\n";
    const auto diagnostics=[&](){std::array<float,4> result{};
        for(GLuint program=1;program<256;++program)if(glIsProgram(program)){
            const int enabled=glGetUniformLocation(program,"shadowEnabled"),vp=glGetUniformLocation(program,"lightVP"),step=glGetUniformLocation(program,"shadowTexelDepth");
            if(enabled<0||vp<0||step<0)continue;GLint active=0;glGetUniformiv(program,enabled,&active);if(!active)continue;
            float m[16]{},texelDepth=0,bias=0;glGetUniformfv(program,vp,m);glGetUniformfv(program,step,&texelDepth);
            const int biasLoc=glGetUniformLocation(program,"normalBias");glGetUniformfv(program,biasLoc,&bias);
            const float sx=std::sqrt(m[0]*m[0]+m[4]*m[4]+m[8]*m[8]),sz=std::sqrt(m[2]*m[2]+m[6]*m[6]+m[10]*m[10]);
            if(sx>0&&sz>0){const float span=2/sx,texel=texelDepth*2/sz;result={std::round(span/texel),span,texel,bias};break;}
        }return result;};
    for(const Vector2 size:{Vector2{1366,768},Vector2{1920,1080}}){
        auto target=LoadRenderTexture(int(size.x),int(size.y));
        const auto draw=[&](){BeginTextureMode(target);renderer.drawScene(scene,camera,int(size.x),int(size.y),0);EndTextureMode();};
        const auto save=[&](const std::string& name){auto image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);failures+=!ExportImage(image,(out/(name+"-"+std::to_string(int(size.x))+".png")).c_str());UnloadImage(image);};
        for(const auto q:{RenderQuality::Low,RenderQuality::Medium,RenderQuality::High}){
            renderer.setQuality(q);for(int frame=0;frame<2;++frame)draw();glFinish();const auto start=std::chrono::steady_clock::now();
            for(int frame=0;frame<3;++frame)draw();glFinish();
            const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/3;
            const auto d=diagnostics();stats<<int(size.x)<<','<<int(size.y)<<','<<int(q)<<','<<int(renderer.actualAntialiasing())<<','<<renderer.shadowsActive()<<','<<d[0]<<','<<d[1]<<','<<d[2]<<','<<d[3]<<','<<ms<<'\n';
            const std::string label=q==RenderQuality::Low?"low":q==RenderQuality::Medium?"medium":"high";save(label);
            scene.lighting()->setShadowsEnabled(false);draw();save(label+"-unshadowed");scene.lighting()->setShadowsEnabled(true);
        }
        if(size.x==1366){renderer.setQuality(RenderQuality::Medium);
            for(int frame=0;frame<8;++frame){auto moved=camera;moved.position.x+=frame*.025f;moved.target.x+=frame*.025f;
                BeginTextureMode(target);renderer.drawScene(scene,moved,int(size.x),int(size.y),0);EndTextureMode();save("motion-"+std::to_string(frame));}}
        failures+=glGetError()!=GL_NO_ERROR;UnloadRenderTexture(target);
    }
    }
    game::ClearImageTextures();CloseWindow();std::printf("Character shadow captures: %d failures; %s\n",failures,directory);return failures?1:0;
}
