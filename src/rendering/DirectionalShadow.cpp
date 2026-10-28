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
bool DirectionalShadow::initialize(int requested) {
    if(requested>1024&&(highUnavailable_||std::getenv("LINBLUX_DISABLE_HIGH_SHADOWS")))requested=1024;
    if(framebuffer_&&resolution_==requested)return true;
    release();if(failed_||std::getenv("LINBLUX_DISABLE_SHADOWS"))return false;
    shader_=LoadShaderFromMemory(vs,fs);
    if(!shader_.id||shader_.id==rlGetShaderIdDefault()){shader_={};failed_=true;return false;}
    shader_.locs[SHADER_LOC_MAP_ALBEDO]=GetShaderLocation(shader_,"texture0");
    material_=LoadMaterialDefault();material_.shader=shader_;
    FrameState state;
    GLint limit=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&limit);
    if(requested>limit){highUnavailable_=true;requested=1024;}
    resolution_=requested;
    framebuffer_=rlLoadFramebuffer();depth_=rlLoadTextureDepth(resolution_,resolution_,false);
    rlFramebufferAttach(framebuffer_,depth_,RL_ATTACHMENT_DEPTH,RL_ATTACHMENT_TEXTURE2D,0);
    glBindFramebuffer(GL_FRAMEBUFFER,framebuffer_);glDrawBuffer(GL_NONE);glReadBuffer(GL_NONE);
    const bool valid=framebuffer_&&depth_&&glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
    if(depth_){
        GLint active=0,binding=0;glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&binding);
        glBindTexture(GL_TEXTURE_2D,depth_);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_MODE,GL_COMPARE_REF_TO_TEXTURE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_FUNC,GL_LEQUAL);
        glBindTexture(GL_TEXTURE_2D,binding);glActiveTexture(active);
    }
    state.restore();
    if(!valid){release();
        if(requested>1024){highUnavailable_=true;return initialize(1024);}
        failed_=true;TraceLog(LOG_WARNING,"LINBLUX: shadow target unavailable; shadows disabled");}
    return valid;
}
void DirectionalShadow::release() {
    if(!framebuffer_&&!shader_.id&&!material_.maps){active_=false;return;}
    FrameState state;
    if(state.read==GLint(framebuffer_))state.read=0;
    if(state.draw==GLint(framebuffer_))state.draw=0;
    if(framebuffer_)rlUnloadFramebuffer(framebuffer_); // attached depth is owned/deleted by rlgl
    framebuffer_=depth_=0;resolution_=0;stableSpan_=0;active_=false;
    if(material_.maps)MemFree(material_.maps);material_={};
    if(shader_.id)UnloadShader(shader_);shader_={};
    state.restore();
}
DirectionalShadow::~DirectionalShadow(){release();}
namespace {
struct Plane {Vector3 n;float d;};
using Planes=std::array<Plane,6>;
using Corners=std::array<Vector3,8>;
bool clip(Vector3 a,Vector3 b,const Planes& planes,Vector3& first,Vector3& last) {
    float lo=0,hi=1;
    for(const auto& p:planes){const float da=Vector3DotProduct(p.n,a)+p.d,db=Vector3DotProduct(p.n,b)+p.d;
        if(da<0&&db<0)return false;
        if(da<0)lo=std::max(lo,da/(da-db));else if(db<0)hi=std::min(hi,da/(da-db));
        if(lo>hi)return false;
    }
    const auto delta=Vector3Subtract(b,a);first=Vector3Add(a,Vector3Scale(delta,lo));last=Vector3Add(a,Vector3Scale(delta,hi));return true;
}
void edges(const Corners& corners,const Planes& planes,std::vector<Vector3>& output) {
    for(int bits=0;bits<8;++bits)for(int axis=0;axis<3;++axis)if(!(bits&(1<<axis))){Vector3 a,b;
        if(clip(corners[bits],corners[bits|(1<<axis)],planes,a,b)){output.push_back(a);output.push_back(b);}}
}
struct ReceiverFrustum {Corners corners;Planes planes;};
ReceiverFrustum receivers(Camera3D camera,float aspect,float range) {
    ReceiverFrustum result;const auto forward=Vector3Normalize(Vector3Subtract(camera.target,camera.position));
    auto right=Vector3CrossProduct(forward,camera.up);if(Vector3LengthSqr(right)<.0001f)right={1,0,0};right=Vector3Normalize(right);
    const auto up=Vector3Normalize(Vector3CrossProduct(right,forward));
    const bool ortho=camera.projection==CAMERA_ORTHOGRAPHIC;const float half=ortho?camera.fovy*.5f:std::tan(camera.fovy*DEG2RAD*.5f);
    for(int bits=0;bits<8;++bits){const float depth=bits&4?range:.1f,h=ortho?half:half*depth,w=h*aspect;
        result.corners[bits]=Vector3Add(Vector3Add(camera.position,Vector3Scale(forward,depth)),
            Vector3Add(Vector3Scale(right,bits&1?w:-w),Vector3Scale(up,bits&2?h:-h)));}
    result.planes[0]={forward,-Vector3DotProduct(forward,camera.position)-.1f};
    result.planes[1]={Vector3Negate(forward),Vector3DotProduct(forward,camera.position)+range};
    for(int side=0;side<4;++side){const auto basis=side<2?right:up;const float extent=side<2?half*aspect:half;
        const auto n=ortho?Vector3Scale(basis,side%2?-1.f:1.f):Vector3Add(Vector3Scale(basis,side%2?-1.f:1.f),Vector3Scale(forward,extent));
        result.planes[side+2]={n,-Vector3DotProduct(n,camera.position)+(ortho?extent:0)};}
    return result;
}
struct CasterBounds {const ShadowCaster* caster;Vector3 lo,hi;Corners corners;};
CasterBounds bounds(const ShadowCaster& caster) {
    CasterBounds b{&caster,{INFINITY,INFINITY,INFINITY},{-INFINITY,-INFINITY,-INFINITY},{}};
    for(const auto p:OrientedBox{caster.position,caster.rotation,HalfExtents(caster.size)}.corners()){b.lo=Vector3Min(b.lo,p);b.hi=Vector3Max(b.hi,p);}
    for(int bits=0;bits<8;++bits)b.corners[bits]={bits&1?b.hi.x:b.lo.x,bits&2?b.hi.y:b.lo.y,bits&4?b.hi.z:b.lo.z};return b;
}
Planes boxPlanes(Vector3 a,Vector3 b){return {{{{1,0,0},-a.x},{{-1,0,0},b.x},{{0,1,0},-a.y},{{0,-1,0},b.y},{{0,0,1},-a.z},{{0,0,-1},b.z}}};}
}
void DirectionalShadow::update(const std::vector<ShadowCaster>& casters,Camera3D camera,const Lighting* light,
    bool enabled,const Mesh& cube,const Mesh& sphere,float aspect,ShadowQuality quality) {
    active_=false;quality_=quality;
    if(!enabled||!light||!light->shadowsEnabled()||light->sunIntensity()<=0||light->shadowStrength()<=0||quality==ShadowQuality::Off) {release();return;}
    if(casters.empty()||!initialize(quality==ShadowQuality::High?2048:1024))return;
    // Budget the camera-visible receiver region. A huge floor or offscreen wall
    // must not spend all texels on regions behind the viewer.
    const float focus=Vector3Distance(camera.position,camera.target);
    distance_=std::min(light->shadowDistance(),std::max(resolution_>1024?48.f:36.f,std::min(focus+8,64.f)));
    const auto frustum=receivers(camera,std::max(.1f,aspect),distance_);
    const auto forward=Vector3Normalize(Vector3Subtract(camera.target,camera.position));
    const auto center=Vector3Add(camera.position,Vector3Scale(forward,distance_*.5f));
    const auto direction=Vector3Normalize(light->sunDirection());const auto up=std::fabs(direction.y)>.98f?Vector3{0,0,1}:Vector3{0,1,0};
    const auto view=MatrixLookAt(Vector3Subtract(center,Vector3Scale(direction,distance_*2)),center,up);
    std::vector<CasterBounds> boxes;std::vector<Vector3> points;
    for(const auto& caster:casters){auto b=bounds(caster);edges(b.corners,frustum.planes,points);edges(frustum.corners,boxPlanes(b.lo,b.hi),points);boxes.push_back(b);}
    if(points.empty())return;
    Vector3 low{INFINITY,INFINITY,INFINITY},high{-INFINITY,-INFINITY,-INFINITY};
    for(const auto p:points){const auto q=Vector3Transform(p,view);low=Vector3Min(low,q);high=Vector3Max(high,q);}
    const float requestedSpan=std::max(16.f,std::ceil((std::max(high.x-low.x,high.y-low.y)+1)/2)*2);
    if(stableSpan_==0||requestedSpan>stableSpan_||requestedSpan<stableSpan_*.8f)stableSpan_=requestedSpan;
    const float span=stableSpan_,texel=span/resolution_;
    const float cx=std::round(((low.x+high.x)*.5f-view.m12)/texel)*texel+view.m12;
    const float cy=std::round(((low.y+high.y)*.5f-view.m13)/texel)*texel+view.m13;
    std::vector<const ShadowCaster*> visible;
    // Include off-camera casters whose light-space footprint reaches the
    // visible receivers. This culling is confined to the shadow pass.
    for(const auto& b:boxes){Vector3 a{INFINITY,INFINITY,INFINITY},z{-INFINITY,-INFINITY,-INFINITY};
        for(const auto p:b.corners){const auto q=Vector3Transform(p,view);a=Vector3Min(a,q);z=Vector3Max(z,q);}
        if(a.x>cx+span*.5f||z.x<cx-span*.5f||a.y>cy+span*.5f||z.y<cy-span*.5f)continue;
        visible.push_back(b.caster);low.z=std::min(low.z,a.z);high.z=std::max(high.z,z.z);}
    const float nearPlane=-high.z-2,farPlane=std::max(nearPlane+1,-low.z+2);
    const auto projection=MatrixOrtho(cx-span*.5f,cx+span*.5f,cy-span*.5f,cy+span*.5f,nearPlane,farPlane);
    matrix_=MatrixMultiply(view,projection);normalBias_=std::max(.001f,texel*.15f);depthBiasScale_=texel/(farPlane-nearPlane);
    FrameState state;glBindFramebuffer(GL_FRAMEBUFFER,framebuffer_);rlViewport(0,0,resolution_,resolution_);
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
