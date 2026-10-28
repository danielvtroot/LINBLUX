#include "rendering/ExperienceOverlay.h"
#include "game/Inventory.h"
#include "rendering/SceneRenderer.h"
#include "rendering/ModernMaterials.h"
#include "rendering/SceneTarget.h"
#include <cstdlib>
#include "physics/PhysicsWorld.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include "objects/Baseplate.h"
#include "objects/Lighting.h"
#include "objects/SpawnLocation.h"
#include "scene/Scene.h"
#include "game/ImageAssets.h"

namespace loblox {

namespace {


Color Shade(Color color, float factor) {
    const auto channel = [factor](unsigned char value) {
        const long scaled = std::lround(static_cast<float>(value) * factor);
        return static_cast<unsigned char>(std::clamp<long>(scaled, 0, 255));
    };
    return Color{channel(color.r), channel(color.g), channel(color.b), color.a};
}

void SetColor4(Color color) {
    rlColor4ub(color.r, color.g, color.b, color.a);
}

void Vertex(Vector3 point) {
    rlVertex3f(point.x, point.y, point.z);
}

void Triangle(Vector3 a, Vector3 b, Vector3 c) {
    Vertex(a);
    Vertex(b);
    Vertex(c);
}

// Caixa com sombreamento por face (Lambert simples, custo quase zero).
// A rotacao e aplicada na CPU: os vertices e as normais passam pelo mesmo
// quaternion, portanto desenho, selecao e collider compartilham a orientacao.
// Estado de fog por frame ( câmera + alvo configurados pelo drawScene ).
// CPU puro: sem shader; funciona igual em editor e Play.
struct FogState {
    Vector3 camera{0, 0, 0};
    Color color{120, 180, 240, 255};
    float density = 0.0f;
    bool enabled = false;
};
FogState gFog{};

Color ApplyFog(Color color, Vector3 worldPosition) {
    if (!gFog.enabled || gFog.density <= 0.0f) return color;
    const float distance = Vector3Distance(gFog.camera, worldPosition);
    if (distance <= 0.0f) return color;
    const float amount = 1.0f - std::exp(-gFog.density * distance);
    const auto blend = [&](unsigned char channel, unsigned char fogChannel) {
        return static_cast<unsigned char>(
            std::lround(channel + (static_cast<float>(fogChannel) - channel) * amount));
    };
    return Color{blend(color.r, gFog.color.r), blend(color.g, gFog.color.g),
                 blend(color.b, gFog.color.b), color.a};
}

// Colour and camera of the current frame. AmbientColor already existed in
// Lighting and in the project format, but the renderer ignored it and used a
// flat 50% ambient term. Using it here gives cool sky-tinted shadows and a
// warm sun-facing side without a shader or an extra pass.
struct SurfaceLightState {
    Color ambient{150, 150, 160, 255};
    Color sun{255, 240, 200, 255};
    Color sky{180, 200, 230, 255};
    Vector3 camera{};
};
SurfaceLightState gSurfaceLight;

Color BlendColor(Color a, Color b, float t) {
    const auto mix = [t](unsigned char x, unsigned char y) {
        return static_cast<unsigned char>(std::lround(x + (static_cast<float>(y) - x) * t));
    };
    return Color{mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), a.a};
}

float MaterialGloss(PartMaterial material) {
    switch (material) {
        case PartMaterial::SmoothPlastic: return 0.18f;
        case PartMaterial::Plastic: return 0.10f;
        case PartMaterial::Metal: return 0.28f;
        case PartMaterial::Wood: return 0.05f;
        case PartMaterial::Concrete: return 0.02f;
        case PartMaterial::Neon: return 0.0f;
    }
    return 0.0f;
}

// Hemispheric ambient + warm directional sun + a small per-face sheen.
// Flat faces stay flat like solid scene geometry, but the light now has
// colour and the shadow side is darker than the sky-facing side.
Color LitSurface(Color color, Vector3 normal, Vector3 point, Vector3 lightDirection,
                 float sunIntensity, float brightness, float gloss, bool unlit) {
    const float exposure = std::clamp(brightness, 0.2f, 2.0f);
    if (unlit) return Shade(color, std::clamp(exposure * 1.12f, 0.9f, 1.35f));

    const float up = std::clamp(0.5f + 0.5f * normal.y, 0.0f, 1.0f);
    // AmbientColor is an albedo tint in the project format, not a dimmer.
    // Keep most of the ambient white and borrow the sky hue for the shadows,
    // otherwise the default grey (150) would cut the scene brightness by 40%.
    const Color skyAmbient = BlendColor(gSurfaceLight.ambient, gSurfaceLight.sky, 0.45f);
    const Color ambientTint = BlendColor(Color{255, 255, 255, 255}, skyAmbient, 0.36f);
    const Color sunTint = BlendColor(Color{255, 255, 255, 255}, gSurfaceLight.sun, 0.45f);
    const float ambient = (0.58f + 0.20f * up) * exposure;  // was 0.72+0.06: more contrast top/bottom
    const float lambert = std::max(0.0f, Vector3DotProduct(normal, lightDirection));
    const float sun = 0.38f * lambert * std::clamp(sunIntensity, 0.0f, 3.0f) * exposure; // was 0.34

    float sheen = 0.0f;
    if (gloss > 0.0f && sunIntensity > 0.0f) {
        const Vector3 view = Vector3Normalize(Vector3Subtract(gSurfaceLight.camera, point));
        const Vector3 half = Vector3Normalize(Vector3Add(lightDirection, view));
        const float highlight = std::pow(std::max(0.0f, Vector3DotProduct(normal, half)), 40.0f);
        sheen = gloss * highlight * std::clamp(sunIntensity, 0.0f, 2.0f) * exposure;
    }

    const auto channel = [&](unsigned char base, unsigned char ambientChannel,
                             unsigned char sunChannel) {
        const float lit = static_cast<float>(base) *
                              (ambient * ambientChannel / 255.0f + sun * sunChannel / 255.0f) +
                          sheen * 255.0f * sunChannel / 255.0f;
        return static_cast<unsigned char>(std::clamp<long>(std::lround(lit), 0, 255));
    };
    return Color{channel(color.r, ambientTint.r, sunTint.r),
                 channel(color.g, ambientTint.g, sunTint.g),
                 channel(color.b, ambientTint.b, sunTint.b), color.a};
}

// ---------- Procedural Sky Shader System ----------
// Fullscreen quad + GLSL shader para céu procedural contínuo e sem costuras.
// Não usa texturas externas; tudo é gerado no shader (gradiente, sol, nuvens, atmosfera).
struct ProceduralSky {
    Shader shader{};
    int locMvp = -1;
    int locCameraPos = -1;
    int locSunDir = -1;
    int locSunColor = -1;
    int locSunSize = -1;
    int locSunIntensity = -1;
    int locZenithColor = -1;
    int locHorizonColor = -1;
    int locGroundColor = -1;
    int locAtmosphereStrength = -1;
    int locCloudDensity = -1;
    int locCloudScale = -1;
    int locCloudSpeed = -1;
    int locCloudSoftness = -1;
    int locSkyRotation = -1;
    int locTime = -1;
    int locFogColor = -1;
    int locFogDensity = -1;
    int locViewMatrix = -1;
    int locProjMatrix = -1;
    int locForward=-1,locRight=-1,locUp=-1,locLens=-1;
    bool initialized = false;
    bool attempted = false;
    float timeAccumulator = 0.0f;
};

ProceduralSky& SkyData() { static ProceduralSky sky; return sky; }

// Shader source embedded as strings
const char* skyVertexShader = R"(
#version 330 core

in vec3 vertexPosition;
in vec2 vertexTexCoord;

out vec2 fragTexCoord;
out vec3 worldDir;

uniform mat4 mvp;

void main() {
    fragTexCoord = vertexTexCoord;
    fragTexCoord=vertexPosition.xy*.5+.5;
    worldDir=vec3(0.0);
    gl_Position=vec4(vertexPosition.xy,0.0,1.0);
}
)";

const char* skyFragmentShader = R"(
#version 330 core

in vec2 fragTexCoord;
in vec3 worldDir;

out vec4 fragColor;

uniform vec3 cameraPos;
uniform vec3 cameraForward;
uniform vec3 cameraRight;
uniform vec3 cameraUp;
uniform vec2 lens;
uniform vec3 sunDir;
uniform vec3 sunColor;
uniform float sunSize;
uniform float sunIntensity;

uniform vec3 zenithColor;
uniform vec3 horizonColor;
uniform vec3 groundColor;
uniform float atmosphereStrength;

uniform float cloudDensity;
uniform float cloudScale;
uniform float cloudSpeed;
uniform float cloudSoftness;
uniform float skyRotation;
uniform float time;

uniform vec3 fogColor;
uniform float fogDensity;

float hash(vec2 p) {
    // Integer grid hash avoids dozens of transcendental operations per pixel
    // on software OpenGL while keeping deterministic procedural clouds.
    uint n=uint(int(p.x))*1664525u+uint(int(p.y))*1013904223u;
    n=(n^(n>>16u))*2246822519u;
    n^=n>>13u;
    return float(n&0x00ffffffu)/16777215.0;
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p, int octaves) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    for (int i = 0; i < octaves; i++) {
        value += amplitude * noise(p * frequency);
        amplitude *= 0.5;
        frequency *= 2.0;
    }
    return value;
}

vec3 computeSkyColor(vec3 dir, vec3 sunDir, float time) {
    vec3 sun = normalize(sunDir);
    float sunDot = max(dot(dir, sun), 0.0);
    float elevation = dir.y;
    
    vec3 skyColor;
    if (elevation > 0.0) {
        float t = pow(1.0 - elevation, 1.5);
        skyColor = mix(zenithColor, horizonColor, t);
    } else {
        float t = pow(clamp(-elevation,0.0,1.0), .6);
        skyColor = mix(horizonColor, groundColor, t);
    }
    
    float scatter = atmosphereStrength * (1.0 - elevation) * 0.5;
    vec3 atmospheric = mix(vec3(0.0), sunColor * 0.3, scatter);
    skyColor += atmospheric;
    
    float sunDisk = smoothstep(cos(sunSize),cos(sunSize*.75),sunDot)*sunIntensity;
    skyColor += sunColor * sunDisk;
    
    float sunGlow = pow(sunDot, 32.0) * sunIntensity * .16;
    skyColor += sunColor * sunGlow;
    
    return skyColor;
}

float cloudNoise(vec2 p, float time, float speed) {
    p += vec2(time * speed * 0.01, 0.0);
    return fbm(p, 3);
}

void main() {
    vec2 ndc=fragTexCoord*2.0-1.0;
    vec3 dir=normalize(cameraForward+ndc.x*lens.x*cameraRight+ndc.y*lens.y*cameraUp);
    
    float rot = skyRotation * 3.14159265359 / 180.0;
    mat3 rotY = mat3(
        cos(rot), 0.0, sin(rot),
        0.0, 1.0, 0.0,
        -sin(rot), 0.0, cos(rot)
    );
    dir = rotY * dir;
    
    vec3 color = computeSkyColor(dir, sunDir, time);
    
    if (cloudDensity > 0.0) {
        vec2 cloudCoord = vec2(dir.x, dir.z) * cloudScale * 10.0;
        float cloudTime = time * cloudSpeed * 0.5;
        
        float clouds=cloudNoise(cloudCoord,cloudTime,1.0);
        float coverage=.78-.43*cloudDensity;
        clouds=smoothstep(coverage,coverage+.06+.20*cloudSoftness,clouds);
        float cloudMask = smoothstep(-0.1, 0.15, dir.y);
        clouds *= cloudMask;
        clouds *= cloudDensity;
        
        vec3 cloudColor = vec3(1.0, 0.98, 0.95);
        color = mix(color, cloudColor, clouds);
        
        float sunDot = max(dot(normalize(dir), normalize(sunDir)), 0.0);
        float silverLining = pow(sunDot, 16.0) * clouds * 0.3;
        color += sunColor * silverLining;
    }
    
    if (fogDensity > 0.0) {
        float fogAmount = fogDensity * (1.0-smoothstep(-0.2,0.3,dir.y));
        color = mix(color, fogColor, fogAmount);
    }
    
    color = pow(color, vec3(1.0 / 2.2));
    color = clamp(color, 0.0, 1.0);
    
    fragColor = vec4(color, 1.0);
}
)";


void InitProceduralSky() {
    auto& sky = SkyData();
    if (sky.initialized) return;
    if(sky.attempted)return;
    sky.attempted=true;
    
    // The sky uses a batched clip-space quad; no mesh or empty Material.
    
    // Load shader from memory
    sky.shader = LoadShaderFromMemory(skyVertexShader, skyFragmentShader);
    if (sky.shader.id == 0 || sky.shader.id==rlGetShaderIdDefault()) {
        // Fallback: shader compilation failed
        sky.initialized = false;
        return;
    }
    
    // Get uniform locations
    sky.locMvp = GetShaderLocation(sky.shader, "mvp");
    sky.locCameraPos = GetShaderLocation(sky.shader, "cameraPos");
    sky.locSunDir = GetShaderLocation(sky.shader, "sunDir");
    sky.locSunColor = GetShaderLocation(sky.shader, "sunColor");
    sky.locSunSize = GetShaderLocation(sky.shader, "sunSize");
    sky.locSunIntensity = GetShaderLocation(sky.shader, "sunIntensity");
    sky.locZenithColor = GetShaderLocation(sky.shader, "zenithColor");
    sky.locHorizonColor = GetShaderLocation(sky.shader, "horizonColor");
    sky.locGroundColor = GetShaderLocation(sky.shader, "groundColor");
    sky.locAtmosphereStrength = GetShaderLocation(sky.shader, "atmosphereStrength");
    sky.locCloudDensity = GetShaderLocation(sky.shader, "cloudDensity");
    sky.locCloudScale = GetShaderLocation(sky.shader, "cloudScale");
    sky.locCloudSpeed = GetShaderLocation(sky.shader, "cloudSpeed");
    sky.locCloudSoftness = GetShaderLocation(sky.shader, "cloudSoftness");
    sky.locSkyRotation = GetShaderLocation(sky.shader, "skyRotation");
    sky.locTime = GetShaderLocation(sky.shader, "time");
    sky.locFogColor = GetShaderLocation(sky.shader, "fogColor");
    sky.locFogDensity = GetShaderLocation(sky.shader, "fogDensity");
    sky.locViewMatrix = GetShaderLocation(sky.shader, "viewMatrix");
    sky.locProjMatrix = GetShaderLocation(sky.shader, "projMatrix");
    sky.locForward=GetShaderLocation(sky.shader,"cameraForward");
    sky.locRight=GetShaderLocation(sky.shader,"cameraRight");
    sky.locUp=GetShaderLocation(sky.shader,"cameraUp");
    sky.locLens=GetShaderLocation(sky.shader,"lens");
    
    sky.initialized = true;
}

void UnloadProceduralSky() {
    auto& sky = SkyData();
    if (sky.initialized) {
        UnloadShader(sky.shader);
    }
    sky={};
}

bool DrawProceduralSky(const Camera3D& camera, const Lighting* lighting, float deltaTime,int width,int height) {
    auto& sky = SkyData();
    if (!sky.initialized) {
        InitProceduralSky();
        if (!sky.initialized) return false; // renderer falls back to a gradient
    }
    
    if (!lighting || !lighting->skyEnabled()) return false;
    
    sky.timeAccumulator += deltaTime;
    
    // Prepare matrices
    const auto forward=Vector3Normalize(Vector3Subtract(camera.target,camera.position));
    const auto right=Vector3Normalize(Vector3CrossProduct(forward,camera.up));
    const auto up=Vector3CrossProduct(right,forward);
    const float tangent=std::tan(camera.fovy*DEG2RAD*.5f);
    const Vector2 lens{tangent*width/std::max(1,height),tangent};
    
    // Sun direction (normalized, pointing FROM sun TO scene)
    Vector3 sunDir = Vector3Normalize(Vector3Negate(lighting->sunDirection()));
    
    // Cache getter values
    float sunSize = lighting->sunSize();
    float sunIntensity = lighting->sunIntensity();
    float atmosphereStrength = lighting->atmosphereStrength();
    float cloudDensity = lighting->cloudDensity();
    float cloudScale = lighting->cloudScale();
    float cloudSpeed = lighting->cloudSpeed();
    float cloudSoftness = lighting->cloudSoftness();
    float skyRotation = lighting->skyRotation();
    float fogDensity = lighting->fogDensity();
    
    // Begin shader
    BeginShaderMode(sky.shader);
    
    // Set uniforms
    SetShaderValue(sky.shader,sky.locForward,&forward.x,SHADER_UNIFORM_VEC3);
    SetShaderValue(sky.shader,sky.locRight,&right.x,SHADER_UNIFORM_VEC3);
    SetShaderValue(sky.shader,sky.locUp,&up.x,SHADER_UNIFORM_VEC3);
    SetShaderValue(sky.shader,sky.locLens,&lens.x,SHADER_UNIFORM_VEC2);
    SetShaderValue(sky.shader, sky.locCameraPos, &camera.position.x, SHADER_UNIFORM_VEC3);
    SetShaderValue(sky.shader, sky.locSunDir, &sunDir.x, SHADER_UNIFORM_VEC3);
    
    Vector3 sunColorNorm = {
        lighting->sunColor().r / 255.0f,
        lighting->sunColor().g / 255.0f,
        lighting->sunColor().b / 255.0f
    };
    SetShaderValue(sky.shader, sky.locSunColor, &sunColorNorm.x, SHADER_UNIFORM_VEC3);
    SetShaderValue(sky.shader, sky.locSunSize, &sunSize, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sky.shader, sky.locSunIntensity, &sunIntensity, SHADER_UNIFORM_FLOAT);
    
    Vector3 zenithNorm = {
        lighting->skyZenithColor().r / 255.0f,
        lighting->skyZenithColor().g / 255.0f,
        lighting->skyZenithColor().b / 255.0f
    };
    SetShaderValue(sky.shader, sky.locZenithColor, &zenithNorm.x, SHADER_UNIFORM_VEC3);
    
    Vector3 horizonNorm = {
        lighting->skyHorizonColor().r / 255.0f,
        lighting->skyHorizonColor().g / 255.0f,
        lighting->skyHorizonColor().b / 255.0f
    };
    SetShaderValue(sky.shader, sky.locHorizonColor, &horizonNorm.x, SHADER_UNIFORM_VEC3);
    
    Vector3 groundNorm = {
        lighting->skyGroundColor().r / 255.0f,
        lighting->skyGroundColor().g / 255.0f,
        lighting->skyGroundColor().b / 255.0f
    };
    SetShaderValue(sky.shader, sky.locGroundColor, &groundNorm.x, SHADER_UNIFORM_VEC3);
    
    Vector3 fogColorNorm = {
        lighting->fogColor().r / 255.0f,
        lighting->fogColor().g / 255.0f,
        lighting->fogColor().b / 255.0f
    };
    SetShaderValue(sky.shader, sky.locAtmosphereStrength, &atmosphereStrength, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sky.shader, sky.locFogColor, &fogColorNorm.x, SHADER_UNIFORM_VEC3);
    SetShaderValue(sky.shader, sky.locFogDensity, &fogDensity, SHADER_UNIFORM_FLOAT);
    
    SetShaderValue(sky.shader,sky.locCloudDensity,&cloudDensity,SHADER_UNIFORM_FLOAT);
    SetShaderValue(sky.shader,sky.locCloudScale,&cloudScale,SHADER_UNIFORM_FLOAT);
    SetShaderValue(sky.shader,sky.locCloudSpeed,&cloudSpeed,SHADER_UNIFORM_FLOAT);
    SetShaderValue(sky.shader,sky.locCloudSoftness,&cloudSoftness,SHADER_UNIFORM_FLOAT);
    SetShaderValue(sky.shader,sky.locSkyRotation,&skyRotation,SHADER_UNIFORM_FLOAT);
    SetShaderValue(sky.shader,sky.locTime,&sky.timeAccumulator,SHADER_UNIFORM_FLOAT);
    
    // Disable depth test and write for sky
    rlDisableDepthTest();
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    
    // Draw fullscreen quad
    rlSetTexture(rlGetTextureIdDefault());
    rlBegin(RL_QUADS);
    rlVertex3f(-1,-1,0);rlVertex3f(1,-1,0);rlVertex3f(1,1,0);rlVertex3f(-1,1,0);
    rlEnd();
    rlDrawRenderBatchActive();
    
    rlEnableDepthMask();
    rlEnableDepthTest();
    rlEnableBackfaceCulling();
    
    EndShaderMode();
    return true;
}

void DrawShadedBox(Vector3 center, Vector3 size, Quaternion rotation, Color color,
                   Vector3 lightDirection, float sunIntensity, float brightness,
                   float gloss = 0.0f, bool unlit = false) {
    const auto corners=OrientedBox{center,rotation,Vector3Scale(size,.5f)}.corners();

    struct Face {
        int indices[4];
        Vector3 normal;
    };
    const Face faces[6] = {
        {{4, 5, 6, 7}, {0, 1, 0}},   // topo
        {{0, 3, 2, 1}, {0, -1, 0}},  // fundo
        {{0, 4, 7, 3}, {-1, 0, 0}},  // -X
        {{1, 2, 6, 5}, {1, 0, 0}},   // +X
        {{0, 1, 5, 4}, {0, 0, -1}},  // -Z
        {{3, 7, 6, 2}, {0, 0, 1}},   // +Z
    };

    rlBegin(RL_TRIANGLES);
    for (const Face& face : faces) {
        const Vector3 normal = Vector3RotateByQuaternion(face.normal, rotation);
        const Vector3& a = corners[face.indices[0]];
        const Vector3& b = corners[face.indices[1]];
        const Vector3& c = corners[face.indices[2]];
        const Vector3& d = corners[face.indices[3]];
        const Vector3 faceCenter = Vector3Scale(Vector3Add(Vector3Add(a, b), Vector3Add(c, d)), 0.25f);
        if(color.a<255&&Vector3DotProduct(normal,Vector3Subtract(gSurfaceLight.camera,faceCenter))<=0)continue;
        SetColor4(ApplyFog(LitSurface(color, normal, faceCenter, lightDirection, sunIntensity,
                                      brightness, gloss, unlit),
                           faceCenter));
        Triangle(a, b, c);
        Triangle(a, c, d);
    }
    rlEnd();
}

void DrawRotatedBoxWires(Vector3 center, Vector3 size, Quaternion rotation, Color color) {
    const auto world=OrientedBox{center,rotation,Vector3Scale(size,.5f)}.corners();
    static const int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                                     {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (const auto& edge : edges) {
        DrawLine3D(world[edge[0]], world[edge[1]], color);
    }
}

constexpr int kSphereRings=7;
constexpr int kSphereSlices=12;
Vector3 SpherePoint(Vector3 center,float radius,float theta,float phi) {
    return Vector3Add(center,{radius*std::cos(phi)*std::cos(theta),
                             radius*std::sin(phi),radius*std::cos(phi)*std::sin(theta)});
}

void DrawSphereGuideWires(Vector3 center,float radius,Color color) {
    // Use the rendered mesh's exact quad edges. raylib's generic wire sphere
    // uses different latitudes/chords, some inside this faceted surface.
    for(int ring=0;ring<kSphereRings;++ring)for(int slice=0;slice<kSphereSlices;++slice) {
        const float phi=(float(ring)/kSphereRings-.5f)*PI;
        const float nextPhi=(float(ring+1)/kSphereRings-.5f)*PI;
        const float theta=float(slice)/kSphereSlices*2*PI;
        const float nextTheta=float(slice+1)/kSphereSlices*2*PI;
        const auto a=SpherePoint(center,radius,theta,phi);
        if(ring)DrawLine3D(a,SpherePoint(center,radius,nextTheta,phi),color);
        DrawLine3D(a,SpherePoint(center,radius,theta,nextPhi),color);
    }
}

// Esfera facetada com sombreamento por quad. Suficiente para o estilo classico.
void DrawShadedSphere(Vector3 center, float radius, Color color, Vector3 lightDirection,
                      float sunIntensity, float brightness, float gloss = 0.0f,
                      bool unlit = false) {
    constexpr int rings = kSphereRings;
    constexpr int slices = kSphereSlices;
    rlBegin(RL_TRIANGLES);
    for (int ring = 0; ring < rings; ++ring) {
        const float v0 = static_cast<float>(ring) / static_cast<float>(rings);
        const float v1 = static_cast<float>(ring + 1) / static_cast<float>(rings);
        const float phi0 = (v0 - 0.5f) * PI;
        const float phi1 = (v1 - 0.5f) * PI;
        for (int slice = 0; slice < slices; ++slice) {
            const float u0 = static_cast<float>(slice) / static_cast<float>(slices);
            const float u1 = static_cast<float>(slice + 1) / static_cast<float>(slices);
            const float theta0 = u0 * 2.0f * PI;
            const float theta1 = u1 * 2.0f * PI;

            const auto point = [&](float theta, float phi) {
                return SpherePoint(center,radius,theta,phi);
            };
            const Vector3 a = point(theta0, phi0);
            const Vector3 b = point(theta1, phi0);
            const Vector3 c = point(theta1, phi1);
            const Vector3 d = point(theta0, phi1);

            const Vector3 normal =
                Vector3Normalize(Vector3Subtract(Vector3Add(Vector3Add(a, b), Vector3Add(c, d)),
                                                 Vector3Scale(center, 4.0f)));
            const Vector3 quadCenter = Vector3Scale(Vector3Add(Vector3Add(a, b), Vector3Add(c, d)), 0.25f);
            if(color.a<255&&Vector3DotProduct(normal,Vector3Subtract(gSurfaceLight.camera,quadCenter))<=0)continue;
            SetColor4(ApplyFog(LitSurface(color, normal, quadCenter, lightDirection, sunIntensity,
                                          brightness, gloss, unlit),
                               quadCenter));
            Triangle(a, b, c);
            Triangle(a, c, d);
        }
    }
    rlEnd();
}



// Superelipsoide (expoente ~4.5): silhueta de bloco com cantos suavizados.
// Malha fechada, sem emendas, custo comparável à esfera facetada existente.
void DrawShadedRoundedBlocky(Vector3 center, Vector3 size, Quaternion rotation, Color color,
                             Vector3 lightDirection, float sunIntensity, float brightness,
                             float gloss = 0.0f, bool unlit = false) {
    const Vector3 half=Vector3Scale(size,.5f);
    const float radius=std::min({size.x,size.y,size.z})*.16f;
    const Vector3 core{half.x-radius,half.y-radius,half.z-radius};
    // A rounded box with planar face centres and curved edge/corner normals.
    // No polar triangles, trigonometry per frame, or double rotation of normals.
    const Vector3 normals[]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    const Vector3 axes[]={{0,1,0},{0,-1,0},{0,0,1},{0,0,-1},{1,0,0},{-1,0,0}};
    const auto extent=[&](Vector3 axis){return std::fabs(axis.x)*half.x+std::fabs(axis.y)*half.y+std::fabs(axis.z)*half.z;};
    rlBegin(RL_TRIANGLES);
    for(int face=0;face<6;++face) {
        const auto n=normals[face],u=axes[face],v=Vector3CrossProduct(n,u);
        const float eu=extent(u),ev=extent(v);
        const float us[]{-eu,-eu+radius,eu-radius,eu};
        const float vs[]{-ev,-ev+radius,ev-radius,ev};
        const auto vertex=[&](int x,int y) {
            const auto p=Vector3Add(Vector3Scale(n,extent(n)),Vector3Add(Vector3Scale(u,us[x]),Vector3Scale(v,vs[y])));
            const Vector3 anchor{std::clamp(p.x,-core.x,core.x),std::clamp(p.y,-core.y,core.y),std::clamp(p.z,-core.z,core.z)};
            const auto normal=Vector3Normalize(Vector3Subtract(p,anchor));
            const auto position=Vector3Add(anchor,Vector3Scale(normal,radius));
            const auto worldNormal=Vector3RotateByQuaternion(normal,rotation);
            const auto worldPosition=Vector3Add(center,Vector3RotateByQuaternion(position,rotation));
            SetColor4(ApplyFog(LitSurface(color,worldNormal,worldPosition,lightDirection,sunIntensity,
                                          brightness,gloss,unlit),
                               worldPosition));
            Vertex(worldPosition);
        };
        for(int x=0;x<3;++x)for(int y=0;y<3;++y) {
            vertex(x,y);vertex(x+1,y);vertex(x+1,y+1);
            vertex(x,y);vertex(x+1,y+1);vertex(x,y+1);
        }
    }
    rlEnd();
}

// Rosto aplicado a face -Z da cabeca (lazy-load, sem caminho absoluto no
// projeto; o fallback mantem os olhos minimos se a imagem nao existir).
// ---------- Decal/Texture da Part (atributos v3, sem classes novas) ----------
// Atributos lidos: decal_image/decal_face/decal_transparency/decal_rotation/
// decal_tint e texture_image/texture_face/texture_transparency/
// texture_repeat_u/texture_repeat_v/texture_offset_u/texture_offset_v.
// Quad acompanha Position/Rotation/Size da Part automaticamente (usa a
// transform dela). Fog não se aplica a quads texturados nesta versão.
namespace skins {
struct FaceDef {
    const char* name;
    Vector3 normal;      // local
    Vector3 uAxis;       // local: direção do u da imagem
    Vector3 vAxis;       // local: direção do v (para cima da imagem)
    float wExt;          // extensão do quad no eixo u
    float hExt;          // extensão do quad no eixo v
};
const FaceDef kFaces[6] = {
    {"Front",  {0, 0, -1}, {1, 0, 0},  {0, 1, 0}, 1, 1},
    {"Back",   {0, 0,  1}, {-1, 0, 0}, {0, 1, 0}, 1, 1},
    {"Left",   {-1, 0, 0}, {0, 0, -1}, {0, 1, 0}, 1, 1},
    {"Right",  {1, 0,  0}, {0, 0, 1},  {0, 1, 0}, 1, 1},
    {"Top",    {0, 1,  0}, {1, 0, 0},  {0, 0, -1},1, 1},
    {"Bottom", {0, -1, 0}, {-1, 0, 0}, {0, 0, 1}, 1, 1},
};
int FaceIndex(const std::string& face) {
    for (int i = 0; i < 6; ++i) if (face == kFaces[i].name) return i;
    return -1;
}
const std::string* FindText(const Part& part, const char* key) {
    if(!key)return nullptr;
    const auto it = part.attributes().find(key);
    if (it == part.attributes().end()) return nullptr;
    return std::get_if<std::string>(&it->second);
}
bool FindNumber(const Part& part, const char* key, double& out) {
    if(!key)return false;
    const auto it = part.attributes().find(key);
    if (it == part.attributes().end()) return false;
    if (const auto* value = std::get_if<double>(&it->second)) {
        if (!std::isfinite(*value)) return false;
        out = *value; return true;
    }
    return false;
}
Color Tint(const std::string* hex) {
    if (hex == nullptr || hex->size() < 6) return Color{255, 255, 255, 255};
    const auto channel = [&](int i) {
        const auto hexDigit = [](char c) {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'f') return c - 'A' + 10;
            return 0;
        };
        return static_cast<unsigned char>(hexDigit((*hex)[i]) * 16 + hexDigit((*hex)[i + 1]));
    };
    return Color{channel(0), channel(2), channel(4), 255};
}
void DrawFaceQuad(Vector3 position, Quaternion rotationQuat, Vector3 size, int face,
                  const Texture2D& tex, float alpha, Color tint, Vector2 repeatUv,
                  Vector2 offsetUv, float uvRotation, Vector3 lightDirection, float sunIntensity,
                  float brightness, float outward = 0.02f, bool flipU = false) {
    const Vector3 half = Vector3Scale(size, 0.5f);
    const FaceDef& def = kFaces[face];
    // Extensões reais no plano da face.
    const float w = std::fabs(def.uAxis.x) > 0.5f ? half.x :
                    std::fabs(def.uAxis.z) > 0.5f ? half.z : def.wExt;
    const float h = std::fabs(def.vAxis.y) > 0.5f ? half.y :
                    std::fabs(def.vAxis.z) > 0.5f ? half.z : def.hExt;
    // Centro local: face do box + offset para fora (anti z-fight).
    const float faceHalf = std::fabs(def.normal.x) > 0.5f ? half.x :
                           std::fabs(def.normal.y) > 0.5f ? half.y : half.z;
    const Vector3 centerLocal =
        Vector3Add(Vector3Scale(def.normal, faceHalf + outward),
                   Vector3{0, 0, 0});
    const auto rotateUv = [&](float u, float v) {
        // Rotaciona base uv (0..1) por quarter-turns em torno do centro.
        u = (u - 0.5f) * repeatUv.x + offsetUv.x;
        v = (v - 0.5f) * repeatUv.y + offsetUv.y;
        const float angle=uvRotation*DEG2RAD,c=std::cos(angle),s=std::sin(angle);
        const float previousU=u;u=previousU*c+v*s;v=-previousU*s+v*c;
        return std::pair<float, float>{flipU ? .5f-u : u+.5f, v+.5f};
    };
    // Cantos locais: bl, br, tr, tl ao longo de u/v.
    const Vector3 local[4] = {
        centerLocal - Vector3Scale(def.uAxis, w) - Vector3Scale(def.vAxis, h),
        centerLocal + Vector3Scale(def.uAxis, w) - Vector3Scale(def.vAxis, h),
        centerLocal + Vector3Scale(def.uAxis, w) + Vector3Scale(def.vAxis, h),
        centerLocal - Vector3Scale(def.uAxis, w) + Vector3Scale(def.vAxis, h),
    };
    const float cornerUv[4][2] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};
    Vector3 world[4];
    for (int i = 0; i < 4; ++i) {
        world[i] = Vector3Add(position,
                              Vector3RotateByQuaternion(local[i], rotationQuat));
    }
    // The texture is uploaded premultiplied (see game::AssetTexture) and the
    // mip chain filters it in that space, so the quad has to blend the same
    // way. The result is identical to straight alpha, but the filtered
    // colour no longer bleeds the colour stored under alpha = 0.
    // Decals are lit by the face they sit on, so a texture on a shadow side
    // no longer glows brighter than the Part itself.
    const Vector3 worldNormal = Vector3Normalize(Vector3RotateByQuaternion(def.normal, rotationQuat));
    const Vector3 worldCenter = Vector3Add(position,
                                          Vector3RotateByQuaternion(centerLocal, rotationQuat));
    const Color litTint = ApplyFog(
        LitSurface(tint, worldNormal, worldCenter, lightDirection, sunIntensity, brightness, 0.0f, false),
        worldCenter);
    // The blend mode is switched BEFORE the texture: switching it flushes
    // raylib's vertex batch, and the flush resets the batch entry that
    // holds the texture id.
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
    rlSetTexture(tex.id);
    rlBegin(RL_QUADS);
    const auto channel=[alpha](unsigned char value){return static_cast<unsigned char>(std::lround(std::clamp(alpha,0.0f,1.0f)*value));};
    rlColor4ub(channel(litTint.r),channel(litTint.g),channel(litTint.b),static_cast<unsigned char>(lround(std::clamp(alpha, 0.0f, 1.0f) * 255.0f)));
    for (int i = 0; i < 4; ++i) {
        const auto uv = rotateUv(cornerUv[i][0], cornerUv[i][1]);
        rlTexCoord2f(uv.first, uv.second);
        rlVertex3f(world[i].x, world[i].y, world[i].z);
    }
    rlEnd();
    EndBlendMode();
    rlSetTexture(0);
}
void DrawQuad(const Part& part, int face, const Texture2D& tex, float alpha,
              Color tint, Vector2 repeatUv, Vector2 offsetUv, int rotation,
              Vector3 lightDirection, float sunIntensity, float brightness,
              float outward = 0.02f, bool flipU = false) {
    DrawFaceQuad(part.position(), part.rotation(), part.size(), face, tex, alpha, tint,
                 repeatUv, offsetUv, rotation, lightDirection, sunIntensity, brightness,
                 outward, flipU);
}
// Legacy R6 images use Front only; the previous six-face loop was the bug.
// offset just enough to avoid z-fighting while staying glued to the animated
// body transform. The default head face is drawn afterwards, in front.
void DrawBodyImage(const game::RigidTransform& body, Vector3 size,
                   const std::string& image, Vector3 lightDirection,
                   float sunIntensity, float brightness) {
    if (image.empty()) return;
    const std::string resolved = game::ResolveImagePath(game::AssetsDir(), image);
    if (resolved.empty()) return;
    const Texture2D tex = game::AssetTexture(resolved);
    if (tex.id == 0) return;
        DrawFaceQuad(body.position, body.rotation, size, 0, tex, 1.0f,
                     Color{255, 255, 255, 255}, {1, 1}, {0, 0}, 0,
                     lightDirection, sunIntensity, brightness, 0.02f);
}
void DrawLayer(const Part& part, const char* imageKey, const char* faceKey,
               const char* alphaKey, const char* tintKey, const char* rotationKey,
               const char* repeatUKey, const char* repeatVKey, const char* offsetUKey,
               const char* offsetVKey, const std::string& assetsDir,
               Vector3 lightDirection, float sunIntensity, float brightness) {
    const std::string* image = FindText(part, imageKey);
    if (image == nullptr || image->empty()) return;
    int face = 0;
    if (const std::string* faceName = FindText(part, faceKey)) {
        const int index = FaceIndex(*faceName);
        if (index >= 0) face = index;
    }
    double alpha = 0.0f; // Transparency defaults to opaque, not invisible.
    FindNumber(part, alphaKey, alpha);
    double rotation = 0.0f;
    FindNumber(part, rotationKey, rotation);
    double repeatU = 1.0, repeatV = 1.0, offsetU = 0.0, offsetV = 0.0;
    FindNumber(part, repeatUKey, repeatU);
    FindNumber(part, repeatVKey, repeatV);
    FindNumber(part, offsetUKey, offsetU);
    FindNumber(part, offsetVKey, offsetV);
    const Vector2 repeat{static_cast<float>(std::clamp(repeatU, 0.05, 64.0)),
                         static_cast<float>(std::clamp(repeatV, 0.05, 64.0))};
    const Vector2 offset{static_cast<float>(std::clamp(offsetU, -16.0, 16.0)),
                         static_cast<float>(std::clamp(offsetV, -16.0, 16.0))};
    const std::string relative = game::ResolveImagePath(assetsDir, *image);
    if (relative.empty()) return;
    const Texture2D tex = game::AssetTexture(relative);
    if (tex.id == 0) return;
    DrawQuad(part, face, tex, static_cast<float>(1.0f - std::clamp(alpha, 0.0, 1.0)),
             Tint(FindText(part, tintKey)), repeat, offset, (int)std::lround(rotation),
             lightDirection, sunIntensity, brightness,.02f,
             [&](){const auto it=part.attributes().find(std::string(imageKey)=="decal_image"?"decal_flip_u":"texture_flip_u");
                 return it!=part.attributes().end()&&std::holds_alternative<bool>(it->second)&&std::get<bool>(it->second);}());
}
}  // namespace skins

void DrawPartSkins(const Part& part, const std::string& assetsDir,
                   Vector3 lightDirection, float sunIntensity, float brightness) {
    skins::DrawLayer(part, "decal_image", "decal_face", "decal_transparency",
                     "decal_tint", "decal_rotation", nullptr, nullptr, nullptr, nullptr,
                     assetsDir, lightDirection, sunIntensity, brightness);
    skins::DrawLayer(part, "texture_image", "texture_face", "texture_transparency",
                     nullptr, nullptr, "texture_repeat_u", "texture_repeat_v",
                     "texture_offset_u", "texture_offset_v", assetsDir,
                     lightDirection, sunIntensity, brightness);
    for(const auto* child:part.children())if(IsSurfaceClass(child->instanceClass())) {
        const auto& surface=static_cast<const Surface&>(*child);const auto& value=surface.properties();
        if(!value.valid()||value.image.empty())continue;
        const auto path=game::ResolveImagePath(assetsDir,value.image);if(path.empty())continue;
        const auto texture=game::AssetTexture(path);if(!texture.id)continue;
        Vector2 repeat{1,1};
        if(surface.instanceClass()==InstanceClass::Texture) {
            const auto& face=skins::kFaces[value.face];const auto size=part.size();
            const auto extent=[&](Vector3 axis){return std::fabs(axis.x)*size.x+std::fabs(axis.y)*size.y+std::fabs(axis.z)*size.z;};
            repeat={extent(face.uAxis)/value.studsPerTileU,extent(face.vAxis)/value.studsPerTileV};
        }
        skins::DrawFaceQuad(part.position(),part.rotation(),part.size(),value.face,texture,1-value.transparency,value.tint,
            repeat,{value.offsetU,value.offsetV},value.rotation,lightDirection,sunIntensity,brightness,.021f,
            [&](){const auto it=surface.attributes().find("flip_u");
                return it!=surface.attributes().end()&&std::holds_alternative<bool>(it->second)&&std::get<bool>(it->second);}());
    }
    rlDrawRenderBatchActive();
}

Texture2D& FaceTexture() {
    static Texture2D tex{};
    // Sem rosto obrigatório: só desenha a face quando o projeto define uma
    // imagem. Cabeças novas ficam sem marcas faciais impostas;
    // experiências antigas que já têm faceImage continuam intactas.
    const auto& reference=game::ProjectFaceImage();
    tex=reference.empty()?Texture2D{}:game::AssetTexture(reference);
    return tex;
}

void DrawHeadFace(const game::RigidTransform& headWorld, Vector3 headSize) {
    Texture2D& tex = FaceTexture();
    if (tex.id == 0) return;
    const Vector3 half = Vector3Scale(headSize, 0.5f);
    // Decal centralizado com o ASPECTO da imagem preservado dentro da face
    // frontal (não esticada até a silhueta inteira):
    const float faceW = headSize.x, faceH = headSize.y;
    const float imageAspect = float(tex.width) / float(std::max(1, tex.height));
    float quadW = faceW, quadH = faceW / imageAspect;
    if (quadH > faceH) { quadH = faceH; quadW = faceH * imageAspect; }
    quadW *= 0.92f; quadH *= 0.92f;
    const float offset = half.z + 0.034f;
    const Vector3 local[4] = {
        {-quadW * .5f,  quadH * .5f, -offset},  // topo-esquerda
        { quadW * .5f,  quadH * .5f, -offset},  // topo-direita
        { quadW * .5f, -quadH * .5f, -offset},  // baixo-direita
        {-quadW * .5f, -quadH * .5f, -offset},  // baixo-esquerda
    };
    Vector3 world[4];
    for (int i = 0; i < 4; ++i) {
        world[i] = Vector3Add(headWorld.position,
                              Vector3RotateByQuaternion(local[i], headWorld.rotation));
    }
    // Face -Z: o observador na frente olha para +Z, então +X aparece à ESQUERDA;
    // mapear u de +X -> -X deixa a imagem sem espelhamento.
    const float uv[4][2] = {{1, 0}, {0, 0}, {0, 1}, {1, 1}};
    // The blend mode is switched BEFORE the texture: switching it flushes
    // raylib's vertex batch, and the flush resets the batch entry that
    // holds the texture id.
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
    rlSetTexture(tex.id);
    rlBegin(RL_QUADS);
    rlColor4ub(255, 255, 255, 255);
    for (int i = 0; i < 4; ++i) {
        rlTexCoord2f(uv[i][0], uv[i][1]);
        rlVertex3f(world[i].x, world[i].y, world[i].z);
    }
    rlEnd();
    EndBlendMode();
    rlSetTexture(0);
}

}  // namespace

// Static counter for SceneRenderer instances
static int rendererCount = 0;

SceneRenderer::SceneRenderer() {
    ++rendererCount;
    modern_=std::make_unique<ModernMaterials>();target_=std::make_unique<SceneTarget>();
    if(const char* value=std::getenv("LINBLUX_ANTIALIASING")){
        const std::string aa=value;antialiasing_=aa=="off"?Antialiasing::None:aa=="fxaa"?Antialiasing::FXAA:aa=="msaa4"?Antialiasing::MSAA4:Antialiasing::Auto;
    }
    if(const char* value=std::getenv("LINBLUX_RENDER_QUALITY")) {
        const std::string q=value;
        quality_=q=="legacy"?RenderQuality::Legacy:q=="low"?RenderQuality::Low:
            q=="high"?RenderQuality::High:RenderQuality::Medium;
    }
}
bool SceneRenderer::shadowsActive() const {return modern_->shadowActive();}
Antialiasing SceneRenderer::actualAntialiasing() const {return target_->actual();}
bool SceneRenderer::modernReady() const {return modern_->ready();}

SceneRenderer::~SceneRenderer() {
    if(--rendererCount==0) {
        ClearExperienceOverlayResources();
        game::ClearImageTextures();
        UnloadProceduralSky();
    }
}

Color SceneRenderer::MaterialColor(const Part& part) {
    return MaterialColor(part.color(), part.material());
}
Color SceneRenderer::MaterialColor(Color color, PartMaterial material) {
    const float r = static_cast<float>(color.r) / 255.0f;
    const float g = static_cast<float>(color.g) / 255.0f;
    const float b = static_cast<float>(color.b) / 255.0f;
    switch (material) {
        case PartMaterial::SmoothPlastic:
            return color;
        case PartMaterial::Plastic:
            return Shade(color, 1.06f);
        case PartMaterial::Wood:
            return Color{static_cast<unsigned char>(std::min(255.0f, color.r * 1.08f)),
                         static_cast<unsigned char>(std::min(255.0f, color.g * 0.94f)),
                         static_cast<unsigned char>(std::min(255.0f, color.b * 0.8f)), color.a};
        case PartMaterial::Metal: {
            const float gray = 0.45f + 0.55f * (r + g + b) / 3.0f;
            return Shade(Color{static_cast<unsigned char>(gray * 255.0f),
                               static_cast<unsigned char>(gray * 255.0f),
                               static_cast<unsigned char>(std::min(255.0f, gray * 258.0f)), color.a},
                         1.05f);
        }
        case PartMaterial::Concrete:
            return Shade(Color{static_cast<unsigned char>((r * 0.6f + 0.25f) * 255.0f),
                               static_cast<unsigned char>((g * 0.6f + 0.25f) * 255.0f),
                               static_cast<unsigned char>((b * 0.6f + 0.25f) * 255.0f), color.a},
                         0.95f);
        case PartMaterial::Neon:
            return Shade(color, 1.22f);
    }
    return color;
}


void SceneRenderer::drawBaseplate(const Part& baseplate, Vector3 lightDirection,
                                  float sunIntensity, float brightness) {
    // A Baseplate is a plain block. Editing grids are a separate viewport tool.
    DrawShadedBox(baseplate.position(), baseplate.size(), baseplate.rotation(), MaterialColor(baseplate),
                  lightDirection, sunIntensity, brightness,
                  MaterialGloss(baseplate.material()),
                  baseplate.material() == PartMaterial::Neon);
}

void SceneRenderer::drawSelection(const Part& part) const {
    // Exact geometry, not a 2% inflated proxy. Overlay depth policy keeps
    // coplanar outlines readable without changing their world-space bounds.
    rlDrawRenderBatchActive();rlDisableDepthTest();rlDisableDepthMask();
    const Color selection{255, 205, 60, 255};
    if (part.shape() == PartShape::Sphere) {
        DrawSphereWires(part.position(), part.sphereRadius(), 8, 12, selection);
    } else {
        DrawRotatedBoxWires(part.position(), part.size(), part.rotation(),
                            selection);
    }
    rlDrawRenderBatchActive();rlEnableDepthMask();rlEnableDepthTest();
}

void SceneRenderer::drawEditorGuides(const Scene& scene,Camera3D camera,const EditorGeometryView& options,
                                     InstanceId hovered) const {
    if(!options.edges&&!options.bounds&&!options.normals&&!hovered)return;
    BeginMode3D(camera);rlDrawRenderBatchActive();rlEnableDepthTest();rlDisableDepthMask();
    // GL polygon offset does not affect GL_LINES. Bias only the guide pass
    // in clip depth, leaving x/y and world bounds exact. The offset in NDC
    // falls with view distance (clip.w), instead of pulling distant edges
    // through walls with a large constant window-depth offset.
    const Matrix projection=rlGetMatrixProjection();
    Matrix guideProjection=projection;
    guideProjection.m14-=0.0001f;
    rlSetMatrixProjection(guideProjection);
    for(const auto* p:scene.parts()) {
        if(p->color().a==0)continue;
        if((options.edges||p->id()==hovered)&&!scene.selected(p->id())) {
            const Color color=p->id()==hovered?Color{240,230,225,230}:Color{42,46,54,165};
            if(p->shape()==PartShape::Block)DrawRotatedBoxWires(p->position(),p->size(),p->rotation(),color);
            else if(p->shape()==PartShape::Sphere) {
                if(quality_==RenderQuality::Legacy||!modern_->ready()||!modern_->wireSphere(p->position(),p->sphereRadius(),p->rotation(),color))
                    DrawSphereGuideWires(p->position(),p->sphereRadius(),color);
            }
        }
        if(options.bounds) {
            Vector3 low{INFINITY,INFINITY,INFINITY},high{-INFINITY,-INFINITY,-INFINITY};
            if(p->shape()==PartShape::Sphere) {
                const float r=p->sphereRadius();
                low=Vector3Subtract(p->position(),{r,r,r});
                high=Vector3Add(p->position(),{r,r,r});
            } else for(const auto v:p->bounds().corners()) {
                low=Vector3Min(low,v);high=Vector3Max(high,v);
            }
            DrawBoundingBox({low,high},{195,112,224,160});
        }
        if(options.normals&&scene.selected(p->id())&&p->shape()==PartShape::Block) {
            const auto dimensions=p->size();
            for(int i=0;i<3;++i)for(float side:{-1.f,1.f}) {
                Vector3 local{};(&local.x)[i]=side;const auto n=Vector3RotateByQuaternion(local,p->rotation());
                const auto face=Vector3Add(p->position(),Vector3Scale(n,(&dimensions.x)[i]*.5f));
                DrawLine3D(face,Vector3Add(face,Vector3Scale(n,.8f)),{245,110,95,230});
            }
        }
    }
    rlDrawRenderBatchActive();rlSetMatrixProjection(projection);rlEnableDepthMask();EndMode3D();
}


void SceneRenderer::drawRig(const RigRenderState& state, Vector3 light, float intensity, float brightness) {
    if (!state.rig) return;
    const auto& rig = *state.rig;
    const auto world = rig.evaluate(state.pose, state.root);
    for (int i = 0; i < game::kBodyCount; ++i) {
        const auto& body = rig.bodies[i];
        const Color bodyColor = MaterialColor(body.color, body.material);
        const float gloss = MaterialGloss(body.material);
        const bool unlit = body.material == PartMaterial::Neon;
        if (i == game::Head)
            DrawShadedRoundedBlocky(world[i].position, body.size, world[i].rotation, bodyColor,
                                    light, intensity, brightness, gloss, unlit);
        else
            DrawShadedBox(world[i].position, body.size, world[i].rotation, bodyColor, light,
                          intensity, brightness, gloss, unlit);
    }
    // Body decals are drawn after every body so depth testing keeps limbs in
    // front of the torso. They share the evaluated pose, so animation and pose
    // previews move the image with the body automatically.
    rlDrawRenderBatchActive();rlDisableDepthMask();
    for (int i = 0; i < game::kBodyCount; ++i) {
        skins::DrawBodyImage(world[i], rig.bodies[i].size, rig.bodies[i].image,
                             light, intensity, brightness);
        // All body geometry already wrote depth. Coplanar decals test against
        // it without writing depth themselves: vector order is the stable
        // compositing order, including transparent layers on the same face.
        for(const auto& decal:rig.bodies[i].decals) {
            const auto& p=decal.surface;
            if(!p.valid()||p.image.empty())continue;
            const auto tex=game::AssetTexture(game::ResolveImagePath(game::AssetsDir(),p.image));
            if(tex.id)skins::DrawFaceQuad(world[i].position,world[i].rotation,rig.bodies[i].size,p.face,
                tex,1-p.transparency,p.tint,{1,1},{p.offsetU,p.offsetV},p.rotation,light,intensity,brightness,.02f);
        }
    }
    rlDrawRenderBatchActive();rlEnableDepthMask();
    for (int i = 0; i < game::kBodyCount; ++i) {
        if (i == state.selectedBody||state.selectedBody==-2) DrawRotatedBoxWires(world[i].position,
            rig.bodies[i].size, world[i].rotation, GOLD);
    }
    // Preserva só a face explicitamente configurada por projetos legados.
    // Imagens de corpo e adesivos são desenhados pelos componentes acima.
    if (FaceTexture().id != 0) DrawHeadFace(world[game::Head],rig.bodies[game::Head].size);
    if (state.pivots) {
        for (const auto& joint : rig.joints) {
            const auto pivot = game::Compose(world[joint.child],joint.childAnchor);
            DrawSphere(pivot.position, .10f, ORANGE);
            DrawLine3D(pivot.position, world[joint.child].position, YELLOW);
        }
    }
}

void SceneRenderer::drawScene(const Scene& scene,Camera3D camera,int width,int height,InstanceId selectedId,const RigRenderState* rig,bool authoredRigs) {
    const bool resolve=target_->begin(width,height,antialiasing_,quality_);
    drawScenePass(scene,camera,width,height,selectedId,rig,authoredRigs);
    if(resolve)target_->finish();
}

void SceneRenderer::drawScenePass(const Scene& scene, Camera3D camera, int width,
                              int height, InstanceId selectedId, const RigRenderState* rig,
                              bool authoredRigs) {
    const Lighting* lighting = scene.lighting();
    const Color sky = lighting != nullptr ? lighting->skyColor() : Color{120, 180, 240, 255};
    const float sunIntensity = lighting != nullptr ? lighting->sunIntensity() : 1.0f;
    const float brightness = lighting != nullptr ? lighting->brightness() : 1.0f;
    Vector3 lightDirection = lighting != nullptr ? lighting->sunDirection()
                                                 : Vector3{-0.45f, -1.0f, 0.55f};
    lightDirection = Vector3Normalize(Vector3Negate(lightDirection));

    // Fog por distância: editor e Play passam por aqui.
    gFog.camera = camera.position;
    gFog.color = lighting != nullptr ? lighting->fogColor() : sky;
    gFog.density = lighting != nullptr ? lighting->fogDensity() : 0.0f;
    gFog.enabled = gFog.density > 0.0f;

    gSurfaceLight.ambient = lighting != nullptr ? lighting->ambientColor() : Color{150, 150, 160, 255};
    gSurfaceLight.sun = lighting != nullptr ? lighting->sunColor() : Color{255, 240, 200, 255};
    gSurfaceLight.sky = lighting != nullptr ? lighting->skyHorizonColor() : Color{180, 200, 230, 255};
    gSurfaceLight.camera = camera.position;

    const auto& npcVisuals=scene.npcVisuals();
    const auto hasRigVisual=[&](InstanceId id){for(const auto& visual:npcVisuals)if(visual.part==id)return true;return false;};
    const bool modernAvailable=quality_!=RenderQuality::Legacy&&modern_->available();
    const bool wantsShadow=(quality_==RenderQuality::Medium||quality_==RenderQuality::High)&&lighting&&lighting->shadowsEnabled()&&lighting->sunIntensity()>0&&lighting->shadowStrength()>0;
    std::vector<Part*> opaque,transparent;
    std::vector<ShadowCaster> casters;
    for(auto* part:scene.parts()) {
        if(!part||game::InventoryHidden(*part)||hasRigVisual(part->id()))continue;
        const auto* m=modernAvailable&&part->material3D()?&*part->material3D():nullptr;
        const auto alpha=m?m->baseColor.a:part->color().a;if(!alpha)continue;
        const bool textured=m&&!m->baseColorTexture.empty();
        (alpha<255||textured?transparent:opaque).push_back(part);
        if(alpha==255&&wantsShadow){const float d=part->sphereRadius()*2;
            casters.push_back({part->position(),part->shape()==PartShape::Sphere?Vector3{d,d,d}:part->size(),part->rotation(),part->shape()==PartShape::Sphere,part});}
    }
    std::vector<RigRenderState> rigStates;
    if(authoredRigs)for(const auto& asset:scene.gameAssets().rigs)
        if(!rig||!rig->rig||rig->rig->id!=asset.id)rigStates.push_back({&asset,{},asset.root});
    if(rig)rigStates.push_back(*rig);
    for(const auto& visual:npcVisuals)if(const auto* asset=scene.gameAssets().findRig(visual.rigId))rigStates.push_back({asset,visual.pose,visual.root});
    if(wantsShadow)for(const auto& state:rigStates)if(state.rig){const auto world=state.rig->evaluate(state.pose,state.root);
        for(int i=0;i<game::kBodyCount;++i)if(state.rig->bodies[i].color.a==255)
            casters.push_back({world[i].position,state.rig->bodies[i].size,world[i].rotation,false,nullptr});}
    modern_->prepareShadows(casters,camera,lighting,quality_);

    ClearBackground(sky);
    
    // Desenha o céu procedural (sempre, se habilitado)
    if (!lighting || !lighting->skyEnabled() ||
        !DrawProceduralSky(camera,lighting,GetFrameTime(),width,height)) {
        // Fallback: gradiente simples
        DrawRectangleGradientV(0, 0, width, height, sky, Shade(sky, 1.22f));
    }

    BeginMode3D(camera);
    rlEnableDepthTest();
    rlDisableBackfaceCulling();
    const auto drawPart=[&](Part* part,bool translucent) {
        if(quality_!=RenderQuality::Legacy&&part->instanceClass()!=InstanceClass::SpawnLocation&&
           modern_->draw(*part,camera,lighting,quality_,translucent)) {
            // Materials and decals are independent passes; skins below use
            // their existing premultiplied compositing and asset references.
        } else if (part->instanceClass() == InstanceClass::Baseplate) {
            drawBaseplate(*part, lightDirection, sunIntensity, brightness);
        } else if (part->instanceClass() == InstanceClass::SpawnLocation) {
            const Quaternion rotation = part->rotation();
            const auto localBox = [&](Vector3 localCenter, Vector3 size, Color color) {
                const Vector3 world = Vector3Add(
                    part->position(), Vector3RotateByQuaternion(localCenter, rotation));
                DrawShadedBox(world, size, rotation, color, lightDirection, sunIntensity,
                              brightness, 0.10f);
            };
            localBox(Vector3{0.0f, 0.0f, 0.0f}, part->size(), MaterialColor(*part));
            const float top = part->size().y * 0.5f;
            const Color marker = Color{235, 248, 255, 255};
            localBox(Vector3{0.0f, top + 0.08f, 0.0f},
                     Vector3{part->size().x * 0.55f, 0.12f, part->size().z * 0.55f}, marker);
            if (authoredRigs) { // editor marker, not a pole intersecting the spawned avatar
                localBox(Vector3{0.0f, top + 2.2f, 0.0f}, Vector3{0.35f, 4.2f, 0.35f}, marker);
                localBox(Vector3{0.95f, top + 3.6f, 0.0f}, Vector3{1.8f, 1.1f, 0.12f}, Color{0, 120, 255, 255});
            }
        } else if (part->shape() == PartShape::Sphere) {
            DrawShadedSphere(part->position(), part->sphereRadius(), MaterialColor(*part),
                             lightDirection, sunIntensity, brightness,
                             MaterialGloss(part->material()),
                             part->material() == PartMaterial::Neon);
        } else {
            DrawShadedBox(part->position(), part->size(), part->rotation(),
                          MaterialColor(*part), lightDirection, sunIntensity, brightness,
                          MaterialGloss(part->material()),
                          part->material() == PartMaterial::Neon);
        }
    };
    for(auto* part:opaque)drawPart(part,false);
    for(const auto& state:rigStates)drawRig(state,lightDirection,sunIntensity,brightness);
    rlDrawRenderBatchActive();rlDisableDepthMask();
    for(auto* part:opaque)DrawPartSkins(*part,game::AssetsDir(),lightDirection,sunIntensity,brightness);
    std::stable_sort(transparent.begin(),transparent.end(),[&](const Part* a,const Part* b){
        return Vector3DistanceSqr(a->position(),camera.position)>Vector3DistanceSqr(b->position(),camera.position);});
    for(auto* part:transparent){drawPart(part,true);DrawPartSkins(*part,game::AssetsDir(),lightDirection,sunIntensity,brightness);}
    rlDrawRenderBatchActive();rlEnableDepthMask();
    if(auto* selected=scene.findPart(selectedId))drawSelection(*selected);
    EndMode3D();
}

void SceneRenderer::drawCollisionShapes(const Scene& scene,const PhysicsWorld& physics,
                                        Camera3D camera,InstanceId selection) const {
    BeginMode3D(camera);rlDrawRenderBatchActive();rlDisableDepthTest();rlDisableDepthMask();
    for(auto* part:scene.parts()) {
        CollisionGeometry geometry;if(!physics.collisionGeometry(part->id(),geometry))continue;
        float error=0;const auto visual=part->bounds().corners(),actual=geometry.box.corners();
        for(int i=0;i<8;++i)error=std::max(error,Vector3Distance(visual[i],actual[i]));
        const Color color=error>.001f?Color{245,65,210,255}:part->id()==selection?Color{60,255,240,255}:Color{45,205,230,175};
        if(geometry.shape==PartShape::Sphere)DrawSphereWires(geometry.box.center,geometry.box.half.x,12,16,color);
        else DrawRotatedBoxWires(geometry.box.center,Vector3Scale(geometry.box.half,2),geometry.box.rotation,color);
    }
    rlDrawRenderBatchActive();rlEnableDepthMask();rlEnableDepthTest();EndMode3D();
}
}  // namespace loblox
