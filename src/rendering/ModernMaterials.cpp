#include "rendering/ModernMaterials.h"
#include "objects/Lighting.h"
#include "game/ImageAssets.h"
#include "raymath.h"
#include "rlgl.h"
#include <cstdlib>
#include <array>
#include <set>
namespace loblox {
namespace {
const char* vertex=R"GLSL(#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
in vec2 vertexTexCoord;
uniform mat4 mvp,matModel,matNormal;
out vec3 worldPosition,worldNormal,localPosition,localNormal;
out vec2 meshUV;
void main(){
    worldPosition=(matModel*vec4(vertexPosition,1)).xyz;
    worldNormal=normalize((matNormal*vec4(vertexNormal,0)).xyz);
    localPosition=vertexPosition;localNormal=vertexNormal;meshUV=vertexTexCoord;
    gl_Position=mvp*vec4(vertexPosition,1);
})GLSL";
const char* fragment=R"GLSL(#version 330
in vec3 worldPosition,worldNormal,localPosition,localNormal;
in vec2 meshUV;
out vec4 finalColor;
uniform sampler2D texture0,normalMap,mrMap,aoMap;
uniform sampler2DShadow shadowMap;
uniform mat4 lightVP;
uniform int shadowEnabled,shadowTaps;
uniform float shadowMapSize;
uniform float normalBias,shadowDistance,shadowStrength,shadowTexelDepth;
uniform vec4 baseColor,mapFlags;
uniform vec3 cameraPosition,sunDirection,sunColor,ambientColor,emissiveColor,objectSize,fogColor;
uniform vec2 tile,uvOffset;
uniform float metallic,roughness,normalScale,emissiveStrength,uvRotation,sunIntensity,brightness,fogDensity;
uniform int quality,finish,spherical;
const float PI=3.14159265;
vec3 linear(vec3 c){return mix(pow((c+.055)/1.055,vec3(2.4)),c/12.92,lessThanEqual(c,vec3(.04045)));}
vec3 srgb(vec3 c){return mix(1.055*pow(max(c,vec3(0)),vec3(1./2.4))-.055,12.92*c,lessThanEqual(c,vec3(.0031308)));}
vec3 straight(vec4 c){return c.a>.0001?c.rgb/c.a:vec3(0);}
vec3 tonemap(vec3 x){return clamp((x*(2.51*x+.03))/(x*(2.43*x+.59)+.14),0.,1.);}
float visibility(vec3 geometricNormal,float nl){
    if(shadowEnabled==0)return 1.;
    vec4 clip=lightVP*vec4(worldPosition+geometricNormal*normalBias,1.);
    vec3 p=clip.xyz/clip.w*.5+.5;
    // Receiver-plane correction compensates depth along the filtering disk.
    // The residual bias covers the bilinear comparison footprint, not the
    // full penumbra radius: contact remains attached to the real geometry.
    vec2 dx=dFdx(p.xy),dy=dFdy(p.xy);float zx=dFdx(p.z),zy=dFdy(p.z),det=dx.x*dy.y-dx.y*dy.x;
    vec2 gradient=abs(det)>1e-10?vec2(zx*dy.y-zy*dx.y,zy*dx.x-zx*dy.x)/det:vec2(0);
    if(length(cameraPosition-worldPosition)>=shadowDistance||any(lessThanEqual(p,vec3(0)))||any(greaterThanEqual(p,vec3(1))))return 1.;
    float slope=min(12.,sqrt(max(0.,1.-nl*nl))/max(nl,.05));
    float bias=max(.000005,shadowTexelDepth*(.3+.85*slope));
    const vec2 disk[16]=vec2[16](
        vec2(.25,0),vec2(-.25,0),vec2(-.319,.292),vec2(.319,-.292),
        vec2(.049,-.557),vec2(-.049,.557),vec2(.402,.525),vec2(-.402,-.525),
        vec2(-.739,-.131),vec2(.739,.131),vec2(.700,-.446),vec2(-.700,.446),
        vec2(-.234,.870),vec2(.234,-.870),vec2(-.446,-.859),vec2(.446,.859));
    float radius=shadowTaps>8?2.75:shadowTaps>4?1.75:.8;
    float normalization=shadowTaps>8?1.:shadowTaps>4?.661:.433;
    float lit=0.;
    for(int i=0;i<16;++i){if(i>=shadowTaps)break;
        // Fixed light-space pattern: no per-frame or screen-space noise.
        vec2 offset=mat2(.819,-.574,.574,.819)*disk[i]*(radius/(normalization*shadowMapSize));
        lit+=texture(shadowMap,vec3(p.xy+offset,p.z+dot(gradient,offset)-bias));
    }
    float edge=min(min(p.x,p.y),min(1.-p.x,1.-p.y));
    float fade=(1.-smoothstep(shadowDistance*.8,shadowDistance,length(cameraPosition-worldPosition)))*smoothstep(0.,4./shadowMapSize,edge);
    return 1.-(1.-lit/float(shadowTaps))*shadowStrength*fade;
}
void main(){
    vec3 n=normalize(worldNormal);vec3 ln=abs(localNormal);vec3 p=localPosition*objectSize;
    vec2 uv=spherical!=0?meshUV*objectSize.x/tile:(ln.x>ln.y&&ln.x>ln.z?p.zy/tile:ln.y>ln.z?p.xz/tile:p.xy/tile);
    float c=cos(uvRotation),s=sin(uvRotation);uv=mat2(c,-s,s,c)*uv+uvOffset;
    vec4 texel=mapFlags.x>.5?texture(texture0,uv):vec4(1);
    vec3 albedo=linear(baseColor.rgb)*linear(straight(texel));
    if(finish==1)albedo*=.84+.16*sin(uv.x*71.+sin(uv.y*91.))*sin(uv.y*83.);
    if(finish==2){float grain=.5+.5*sin(uv.x*33.+3.*sin(uv.y*2.));albedo*=mix(vec3(.36,.18,.065),vec3(1.,.63,.28),grain);}
    float r=clamp(roughness,.06,1.),m=metallic,ao=1.;
    if(quality>1&&mapFlags.z>.5){vec3 mr=straight(texture(mrMap,uv));r=clamp(r*mr.g,.06,1.);m*=mr.b;}
    if(mapFlags.w>.5)ao=straight(texture(aoMap,uv)).r;
    if(quality>1&&mapFlags.y>.5){
        vec3 dp1=dFdx(worldPosition),dp2=dFdy(worldPosition);vec2 d1=dFdx(uv),d2=dFdy(uv);
        vec3 t=dp1*d2.y-dp2*d1.y;t-=n*dot(n,t);float len=length(t);
        if(len>.00001&&abs(d1.x*d2.y-d1.y*d2.x)>.000001){t/=len;vec3 b=normalize(cross(n,t))*sign(d1.x*d2.y-d1.y*d2.x);
            vec3 map=straight(texture(normalMap,uv))*2.-1.;map.xy*=normalScale;n=normalize(mat3(t,b,n)*map);}
    }
    if(!gl_FrontFacing)n=-n;
    vec3 l=normalize(sunDirection),v=normalize(cameraPosition-worldPosition),h=normalize(l+v);
    float nl=max(dot(n,l),0.),nv=max(dot(n,v),.001),nh=max(dot(n,h),0.),vh=max(dot(v,h),0.);
    vec3 light=linear(sunColor)*sunIntensity*2.*visibility(normalize(worldNormal),max(dot(normalize(worldNormal),l),0.));
    vec3 hemi=mix(vec3(.16,.18,.23),linear(ambientColor)*.6+vec3(.18),n.y*.5+.5);
    vec3 result=albedo*hemi*ao;
    if(quality<3){
        result+=albedo*light*nl;
        if(quality>1){vec3 f0=mix(vec3(.04),albedo,m);float power=mix(200.,4.,r*r);
            result+=f0*pow(nh,power)*light*nl*(1.-.75*r)*2.;}
    }else{
        vec3 f0=mix(vec3(.04),albedo,m);vec3 f=f0+(1.-f0)*pow(1.-vh,5.);
        float a=r*r,a2=a*a;float d=a2/(PI*pow(nh*nh*(a2-1.)+1.,2.)+.00001);
        float k=pow(r+1.,2.)/8.;float g=nv/(nv*(1.-k)+k)*nl/(nl*(1.-k)+k);
        vec3 spec=d*g*f/(4.*nv*max(nl,.001)+.00001);
        result+=((1.-f)*(1.-m)*albedo/PI+spec)*light*nl*PI;
        // Approximate sky reflection only; no IBL/BRDF LUT or claim of full PBR.
        result+=f0*hemi*(1.-r)*.4;
    }
    result=result*brightness+linear(emissiveColor)*emissiveStrength;
    vec3 display=srgb(tonemap(result));
    float fog=1.-exp(-pow(fogDensity*length(cameraPosition-worldPosition),2.));
    display=mix(display,fogColor,clamp(fog,0.,1.)); // sky/fog are already display-space
    finalColor=vec4(display,baseColor.a*texel.a);
})GLSL";
}
bool ModernMaterials::initialize() {
    if(attempted_)return ready_;attempted_=true;
    if(!IsWindowReady()||std::getenv("LINBLUX_DISABLE_MATERIAL_SHADER"))return false;
    shader_=LoadShaderFromMemory(vertex,fragment);
    if(!shader_.id||shader_.id==rlGetShaderIdDefault()) {
        shader_={};TraceLog(LOG_WARNING,"LINBLUX: material shader unavailable; legacy fallback");return false;
    }
    shader_.locs[SHADER_LOC_MATRIX_MODEL]=GetShaderLocation(shader_,"matModel");
    shader_.locs[SHADER_LOC_MATRIX_NORMAL]=GetShaderLocation(shader_,"matNormal");
    shader_.locs[SHADER_LOC_MAP_ALBEDO]=GetShaderLocation(shader_,"texture0");
    shader_.locs[SHADER_LOC_MAP_NORMAL]=GetShaderLocation(shader_,"normalMap");
    shader_.locs[SHADER_LOC_MAP_METALNESS]=GetShaderLocation(shader_,"mrMap");
    shader_.locs[SHADER_LOC_MAP_HEIGHT]=GetShaderLocation(shader_,"shadowMap");
    shader_.locs[SHADER_LOC_MAP_OCCLUSION]=GetShaderLocation(shader_,"aoMap");
    cube_=GenMeshCube(1,1,1);sphere_=GenMeshSphere(1,32,48);
    // Sparse guides composed of the actual mesh edges, not long chords or
    // every triangle. Keep their subsegments to follow the smooth surface.
    std::set<std::array<int,6>> seen;
    const auto vertexAt=[&](int i){const int index=sphere_.indices?sphere_.indices[i]:i;
        return Vector3{sphere_.vertices[index*3],sphere_.vertices[index*3+1],sphere_.vertices[index*3+2]};};
    const auto uvAt=[&](int i){const int index=sphere_.indices?sphere_.indices[i]:i;
        return Vector2{sphere_.texcoords[index*2],sphere_.texcoords[index*2+1]};};
    for(int t=0;t<sphere_.triangleCount;++t)for(int edge=0;edge<3;++edge) {
        auto a=vertexAt(t*3+edge),b=vertexAt(t*3+(edge+1)%3);
        const auto ua=uvAt(t*3+edge),ub=uvAt(t*3+(edge+1)%3);
        // Select grid lines in the mesh's native UVs, independent of its pole
        // axis. Guessing latitude from Y makes discontinuous zigzags on a
        // sphere generated with a different parameterization.
        const bool ring=std::fabs(ua.y-ub.y)<1e-5f&&int(std::lround(ua.y*32))%4==0;
        const bool line=std::fabs(ua.x-ub.x)<1e-5f&&int(std::lround(ua.x*48))%4==0;
        if(!ring&&!line)continue;
        std::array<int,3> ka{int(std::lround(a.x*1e5)),int(std::lround(a.y*1e5)),int(std::lround(a.z*1e5))};
        std::array<int,3> kb{int(std::lround(b.x*1e5)),int(std::lround(b.y*1e5)),int(std::lround(b.z*1e5))};
        if(kb<ka){std::swap(ka,kb);std::swap(a,b);}
        const std::array<int,6> key{ka[0],ka[1],ka[2],kb[0],kb[1],kb[2]};
        if(seen.insert(key).second)sphereGuides_.push_back({a,b});
    }
    material_=LoadMaterialDefault();material_.shader=shader_;
    ready_=cube_.vaoId&&sphere_.vaoId;return ready_;
}
ModernMaterials::~ModernMaterials() {
    if(cube_.vaoId)UnloadMesh(cube_);if(sphere_.vaoId)UnloadMesh(sphere_);
    if(material_.maps)MemFree(material_.maps); // textures belong to ImageAssets
    if(shader_.id)UnloadShader(shader_);
}
bool ModernMaterials::draw(const Part& part,Camera3D camera,const Lighting* light,RenderQuality quality,bool transparent) {
    if(!initialize())return false;
    const auto m=part.material3D()?*part.material3D():LegacyMaterial3D(part.material(),part.color());
    const auto f=[&](const char* name,float v){SetShaderValue(shader_,GetShaderLocation(shader_,name),&v,SHADER_UNIFORM_FLOAT);};
    const auto i=[&](const char* name,int v){SetShaderValue(shader_,GetShaderLocation(shader_,name),&v,SHADER_UNIFORM_INT);};
    const auto v3=[&](const char* name,Vector3 v){SetShaderValue(shader_,GetShaderLocation(shader_,name),&v.x,SHADER_UNIFORM_VEC3);};
    const auto color=[](Color c){return Vector3{c.r/255.f,c.g/255.f,c.b/255.f};};
    const auto c=color(m.baseColor);const Vector4 base{c.x,c.y,c.z,m.baseColor.a/255.f};
    SetShaderValue(shader_,GetShaderLocation(shader_,"baseColor"),&base.x,SHADER_UNIFORM_VEC4);
    i("shadowMap",MATERIAL_MAP_HEIGHT);i("shadowEnabled",shadow_.active());i("shadowTaps",shadow_.taps());f("shadowMapSize",float(shadow_.resolution()));f("normalBias",shadow_.normalBias());f("shadowTexelDepth",shadow_.depthBiasScale());
    f("shadowDistance",shadow_.distance());f("shadowStrength",light?light->shadowStrength():1);
    if(shadow_.active())SetShaderValueMatrix(shader_,GetShaderLocation(shader_,"lightVP"),shadow_.matrix());
    material_.maps[MATERIAL_MAP_HEIGHT].texture={shadow_.active()?shadow_.texture():0,shadow_.resolution(),shadow_.resolution(),1,19};
    f("metallic",m.metallic);f("roughness",m.roughness);f("normalScale",m.normalScale);f("emissiveStrength",m.emissiveStrength);
    f("uvRotation",m.rotation*DEG2RAD);f("sunIntensity",light?light->sunIntensity():1);f("brightness",light?light->brightness():1);
    f("fogDensity",light?light->fogDensity():0);v3("fogColor",color(light?light->fogColor():Color{120,180,240,255}));
    v3("cameraPosition",camera.position);v3("sunDirection",Vector3Negate(light?light->sunDirection():Vector3{-.45f,-1,.55f}));
    v3("sunColor",color(light?light->sunColor():Color{255,240,200,255}));
    v3("ambientColor",color(light?light->ambientColor():Color{150,150,160,255}));v3("emissiveColor",color(m.emissive));
    Vector3 size=part.size();if(part.shape()==PartShape::Sphere)size={part.sphereRadius()*2,part.sphereRadius()*2,part.sphereRadius()*2};
    v3("objectSize",size);i("spherical",part.shape()==PartShape::Sphere);i("quality",int(quality));i("finish",m.finish);
    SetShaderValue(shader_,GetShaderLocation(shader_,"tile"),&m.studsPerTile.x,SHADER_UNIFORM_VEC2);
    SetShaderValue(shader_,GetShaderLocation(shader_,"uvOffset"),&m.offset.x,SHADER_UNIFORM_VEC2);
    Vector4 flags{};
    const std::string* references[]={&m.baseColorTexture,&m.normalTexture,&m.metallicRoughnessTexture,&m.occlusionTexture};
    const int slots[]={MATERIAL_MAP_ALBEDO,MATERIAL_MAP_NORMAL,MATERIAL_MAP_METALNESS,MATERIAL_MAP_OCCLUSION};
    for(int k=0;k<4;++k) {
        Texture2D texture{};if(!references[k]->empty())texture=game::AssetTexture(game::ResolveImagePath(game::AssetsDir(),*references[k]));
        material_.maps[slots[k]].texture=texture;(&flags.x)[k]=texture.id?1.f:0.f;
    }
    if(!material_.maps[MATERIAL_MAP_ALBEDO].texture.id)material_.maps[MATERIAL_MAP_ALBEDO].texture.id=rlGetTextureIdDefault();
    SetShaderValue(shader_,GetShaderLocation(shader_,"mapFlags"),&flags.x,SHADER_UNIFORM_VEC4);
    Vector3 scale=part.shape()==PartShape::Sphere?Vector3{part.sphereRadius(),part.sphereRadius(),part.sphereRadius()}:part.size();
    const auto matrix=MatrixMultiply(MatrixMultiply(MatrixScale(scale.x,scale.y,scale.z),QuaternionToMatrix(part.rotation())),
                                     MatrixTranslate(part.position().x,part.position().y,part.position().z));
    rlDrawRenderBatchActive();
    if(transparent)rlEnableBackfaceCulling();
    DrawMesh(part.shape()==PartShape::Sphere?sphere_:cube_,material_,matrix);
    // DrawMesh cleans its own texture/VAO/shader state. Scene remains uncullled.
    rlDisableBackfaceCulling();return true;
}
void ModernMaterials::prepareShadows(const std::vector<ShadowCaster>& casters,Camera3D camera,const Lighting* light,RenderQuality quality,float aspect,ShadowQuality shadowQuality) {
    const bool enabled=(quality==RenderQuality::Medium||quality==RenderQuality::High)&&initialize();
    if(shadowQuality==ShadowQuality::Auto)shadowQuality=quality==RenderQuality::High?ShadowQuality::High:ShadowQuality::Soft;
    shadow_.update(casters,camera,light,enabled,cube_,sphere_,aspect,shadowQuality);
}
bool ModernMaterials::wireSphere(Vector3 center,float radius,Quaternion rotation,Color color) {
    if(!initialize())return false;
    for(const auto& line:sphereGuides_)DrawLine3D(
        Vector3Add(center,Vector3Scale(Vector3RotateByQuaternion(line.first,rotation),radius)),
        Vector3Add(center,Vector3Scale(Vector3RotateByQuaternion(line.second,rotation),radius)),color);
    return true;
}
}
