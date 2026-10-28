#include "rendering/GraphicsGL.h"
#include "rendering/DirectionalShadow.h"
#include "objects/Lighting.h"
#include "game/ImageAssets.h"
#include "raymath.h"
#include <cstdlib>
#include <limits>
namespace loblox {
namespace {
const char* vs=R"GLSL(#version 330
in vec3 vertexPosition;in vec3 vertexNormal;in vec2 vertexTexCoord;
uniform mat4 mvp;out vec3 localPosition,localNormal;out vec2 uv;
void main(){localPosition=vertexPosition;localNormal=vertexNormal;uv=vertexTexCoord;gl_Position=mvp*vec4(vertexPosition,1);})GLSL";
const char* fs=R"GLSL(#version 330
in vec3 localPosition,localNormal;in vec2 uv;
uniform sampler2D texture0;uniform int masked,spherical;
uniform vec3 objectSize;uniform vec2 tile,uvOffset;uniform float uvRotation;
void main(){if(masked!=0){vec3 n=abs(localNormal),p=localPosition*objectSize;
vec2 t=spherical!=0?uv*objectSize.x/tile:(n.x>n.y&&n.x>n.z?p.zy/tile:n.y>n.z?p.xz/tile:p.xy/tile);
float c=cos(uvRotation),s=sin(uvRotation);t=mat2(c,-s,s,c)*t+uvOffset;
if(texture(texture0,t).a<.5)discard;}})GLSL";
}
bool DirectionalShadow::initialize() {
    if(framebuffer_)return true;if(failed_||std::getenv("LINBLUX_DISABLE_SHADOWS"))return false;
    shader_=LoadShaderFromMemory(vs,fs);
    if(!shader_.id||shader_.id==rlGetShaderIdDefault()){shader_={};failed_=true;return false;}
    shader_.locs[SHADER_LOC_MAP_ALBEDO]=GetShaderLocation(shader_,"texture0");
    material_=LoadMaterialDefault();material_.shader=shader_;
    FrameState state;
    framebuffer_=rlLoadFramebuffer();depth_=rlLoadTextureDepth(1024,1024,false);
    rlFramebufferAttach(framebuffer_,depth_,RL_ATTACHMENT_DEPTH,RL_ATTACHMENT_TEXTURE2D,0);
    glBindFramebuffer(GL_FRAMEBUFFER,framebuffer_);glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE);
    const bool valid=framebuffer_&&depth_&&glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
    if(depth_){Texture2D texture{depth_,1024,1024,1,19};SetTextureFilter(texture,TEXTURE_FILTER_POINT);SetTextureWrap(texture,TEXTURE_WRAP_CLAMP);}
    state.restore();
    if(!valid){release();failed_=true;TraceLog(LOG_WARNING,"LINBLUX: shadow target unavailable; shadows disabled");}
    return valid;
}
void DirectionalShadow::release() {
    if(framebuffer_)rlUnloadFramebuffer(framebuffer_); // attached depth is owned/deleted by rlgl
    framebuffer_=depth_=0;active_=false;
    if(material_.maps)MemFree(material_.maps);material_={};
    if(shader_.id)UnloadShader(shader_);shader_={};
}
DirectionalShadow::~DirectionalShadow(){release();}
void DirectionalShadow::update(const std::vector<ShadowCaster>& casters,Camera3D camera,const Lighting* light,
    bool enabled,const Mesh& cube,const Mesh& sphere) {
    active_=false;
    if(!enabled||!light||!light->shadowsEnabled()||light->sunIntensity()<=0||light->shadowStrength()<=0) {release();return;}
    if(!initialize()||casters.empty())return;
    const float range=light->shadowDistance(),radius=range*.5f;
    auto forward=Vector3Normalize(Vector3Subtract(camera.target,camera.position));
    const auto center=Vector3Add(camera.position,Vector3Scale(forward,std::min(Vector3Distance(camera.target,camera.position),range*.35f)));
    const auto direction=Vector3Normalize(light->sunDirection());
    const auto up=std::fabs(direction.y)>.98f?Vector3{0,0,1}:Vector3{0,1,0};
    const auto eye=Vector3Subtract(center,Vector3Scale(direction,range*1.5f));
    const auto view=MatrixLookAt(eye,center,up);
    Vector3 low{INFINITY,INFINITY,INFINITY},high{-INFINITY,-INFINITY,-INFINITY};
    std::vector<const ShadowCaster*> visible;
    for(const auto& caster:casters) {
        Vector3 a{INFINITY,INFINITY,INFINITY},b{-INFINITY,-INFINITY,-INFINITY};
        for(const auto corner:OrientedBox{caster.position,caster.rotation,HalfExtents(caster.size)}.corners()) {a=Vector3Min(a,corner);b=Vector3Max(b,corner);}
        a=Vector3Max(a,Vector3Subtract(center,{radius,radius,radius}));b=Vector3Min(b,Vector3Add(center,{radius,radius,radius}));
        if(a.x>b.x||a.y>b.y||a.z>b.z)continue;visible.push_back(&caster);
        for(int bits=0;bits<8;++bits){const auto p=Vector3Transform({bits&1?b.x:a.x,bits&2?b.y:a.y,bits&4?b.z:a.z},view);low=Vector3Min(low,p);high=Vector3Max(high,p);}
    }
    if(visible.empty())return;
    const float span=std::max(16.f,std::ceil((std::max(high.x-low.x,high.y-low.y)+2)*2)/2);
    const float texel=span/1024;
    // Snap in the fixed light basis, not relative to the moving camera.
    const float cx=std::round(((low.x+high.x)*.5f-view.m12)/texel)*texel+view.m12;
    const float cy=std::round(((low.y+high.y)*.5f-view.m13)/texel)*texel+view.m13;
    const float nearPlane=std::max(.1f,-high.z-range*.5f),farPlane=std::max(nearPlane+1,-low.z+range*.5f);
    const auto projection=MatrixOrtho(cx-span*.5f,cx+span*.5f,cy-span*.5f,cy+span*.5f,nearPlane,farPlane);
    matrix_=MatrixMultiply(view,projection);normalBias_=std::max(.002f,texel*.12f);
    FrameState state;glBindFramebuffer(GL_FRAMEBUFFER,framebuffer_);rlViewport(0,0,1024,1024);
    glDisable(GL_SCISSOR_TEST);glDisable(GL_FRAMEBUFFER_SRGB);rlEnableDepthTest();rlEnableDepthMask();rlDisableBackfaceCulling();
    rlSetMatrixProjection(projection);rlSetMatrixModelview(view);glClear(GL_DEPTH_BUFFER_BIT);
    for(const auto* c:visible) {
        Material3D m;if(c->part)m=c->part->material3D()?*c->part->material3D():LegacyMaterial3D(c->part->material(),c->part->color());
        Texture2D texture{};if(!m.baseColorTexture.empty())texture=game::AssetTexture(game::ResolveImagePath(game::AssetsDir(),m.baseColorTexture));
        material_.maps[MATERIAL_MAP_ALBEDO].texture=texture.id?texture:Texture2D{rlGetTextureIdDefault(),1,1,1,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        const auto integer=[&](const char* name,int v){SetShaderValue(shader_,GetShaderLocation(shader_,name),&v,SHADER_UNIFORM_INT);};
        integer("masked",texture.id!=0);integer("spherical",c->sphere);
        SetShaderValue(shader_,GetShaderLocation(shader_,"objectSize"),&c->size.x,SHADER_UNIFORM_VEC3);
        SetShaderValue(shader_,GetShaderLocation(shader_,"tile"),&m.studsPerTile.x,SHADER_UNIFORM_VEC2);
        SetShaderValue(shader_,GetShaderLocation(shader_,"uvOffset"),&m.offset.x,SHADER_UNIFORM_VEC2);
        const float angle=m.rotation*DEG2RAD;SetShaderValue(shader_,GetShaderLocation(shader_,"uvRotation"),&angle,SHADER_UNIFORM_FLOAT);
        const auto size=c->sphere?Vector3Scale(c->size,.5f):c->size;
        const auto transform=MatrixMultiply(MatrixMultiply(MatrixScale(size.x,size.y,size.z),QuaternionToMatrix(c->rotation)),MatrixTranslate(c->position.x,c->position.y,c->position.z));
        DrawMesh(c->sphere?sphere:cube,material_,transform);
    }
    state.restore();active_=true;
}
}
