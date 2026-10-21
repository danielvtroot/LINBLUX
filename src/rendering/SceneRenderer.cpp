#include "rendering/SceneRenderer.h"

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
#include "rendering/EditorCamera.h"

namespace loblox {

namespace {

constexpr int kStudSegments = 8;
constexpr float kStudSpacing = 8.0f;
constexpr float kStudRadius = 1.15f;
constexpr float kStudHeight = 0.45f;
constexpr int kMaxStudsPerAxis = 32;

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
void DrawShadedBox(Vector3 center, Vector3 size, Quaternion rotation, Color color,
                   Vector3 lightDirection, float sunIntensity, float brightness) {
    const Vector3 half = Vector3Scale(size, 0.5f);
    const Vector3 localCorners[8] = {
        {-half.x, -half.y, -half.z}, {half.x, -half.y, -half.z},
        {half.x, -half.y, half.z},   {-half.x, -half.y, half.z},
        {-half.x, half.y, -half.z},  {half.x, half.y, -half.z},
        {half.x, half.y, half.z},    {-half.x, half.y, half.z},
    };
    Vector3 corners[8];
    for (int i = 0; i < 8; ++i) {
        corners[i] = Vector3Add(center, Vector3RotateByQuaternion(localCorners[i], rotation));
    }

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
        const float lambert = std::max(0.0f, Vector3DotProduct(normal, lightDirection));
        const float factor =
            std::clamp((0.5f + 0.5f * sunIntensity * lambert) * brightness, 0.2f, 1.6f);
        SetColor4(Shade(color, factor));
        const Vector3& a = corners[face.indices[0]];
        const Vector3& b = corners[face.indices[1]];
        const Vector3& c = corners[face.indices[2]];
        const Vector3& d = corners[face.indices[3]];
        Triangle(a, b, c);
        Triangle(a, c, d);
    }
    rlEnd();
}

void DrawRotatedBoxWires(Vector3 center, Vector3 size, Quaternion rotation, Color color) {
    const Vector3 half = Vector3Scale(size, 0.5f);
    const Vector3 local[8] = {
        {-half.x, -half.y, -half.z}, {half.x, -half.y, -half.z},
        {half.x, -half.y, half.z},   {-half.x, -half.y, half.z},
        {-half.x, half.y, -half.z},  {half.x, half.y, -half.z},
        {half.x, half.y, half.z},    {-half.x, half.y, half.z},
    };
    Vector3 world[8];
    for (int i = 0; i < 8; ++i) {
        world[i] = Vector3Add(center, Vector3RotateByQuaternion(local[i], rotation));
    }
    static const int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                                     {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (const auto& edge : edges) {
        DrawLine3D(world[edge[0]], world[edge[1]], color);
    }
}

// Esfera facetada com sombreamento por quad. Suficiente para o estilo classico.
void DrawShadedSphere(Vector3 center, float radius, Color color, Vector3 lightDirection,
                      float sunIntensity, float brightness) {
    constexpr int rings = 7;
    constexpr int slices = 12;
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
                return Vector3Add(center, Vector3{radius * std::cos(phi) * std::cos(theta),
                                                  radius * std::sin(phi),
                                                  radius * std::cos(phi) * std::sin(theta)});
            };
            const Vector3 a = point(theta0, phi0);
            const Vector3 b = point(theta1, phi0);
            const Vector3 c = point(theta1, phi1);
            const Vector3 d = point(theta0, phi1);

            const Vector3 normal =
                Vector3Normalize(Vector3Subtract(Vector3Add(Vector3Add(a, b), Vector3Add(c, d)),
                                                 Vector3Scale(center, 4.0f)));
            const float lambert = std::max(0.0f, Vector3DotProduct(normal, lightDirection));
            const float factor =
                std::clamp((0.5f + 0.5f * sunIntensity * lambert) * brightness, 0.2f, 1.6f);
            SetColor4(Shade(color, factor));
            Triangle(a, b, c);
            Triangle(a, c, d);
        }
    }
    rlEnd();
}

Mesh BuildStudMesh(Vector3 size) {
    const int nx = std::clamp(static_cast<int>(std::floor(size.x / kStudSpacing)), 1,
                              kMaxStudsPerAxis);
    const int nz = std::clamp(static_cast<int>(std::floor(size.z / kStudSpacing)), 1,
                              kMaxStudsPerAxis);
    const float topLocal = size.y * 0.5f;
    const int perStudVertices = kStudSegments * 3 + 1;
    const int perStudTriangles = kStudSegments * 3;
    const int vertexCount = nx * nz * perStudVertices;
    const int triangleCount = nx * nz * perStudTriangles;

    std::vector<Vector3> vertices;
    std::vector<Vector3> normals;
    std::vector<Color> colors;
    std::vector<unsigned short> indices;
    vertices.reserve(static_cast<std::size_t>(vertexCount));
    normals.reserve(static_cast<std::size_t>(vertexCount));
    colors.reserve(static_cast<std::size_t>(vertexCount));
    indices.reserve(static_cast<std::size_t>(triangleCount * 3));

    const float startX = -0.5f * static_cast<float>(nx - 1) * kStudSpacing;
    const float startZ = -0.5f * static_cast<float>(nz - 1) * kStudSpacing;
    const Color sideColor{188, 188, 188, 255};
    const Color topColor{255, 255, 255, 255};

    for (int ix = 0; ix < nx; ++ix) {
        for (int iz = 0; iz < nz; ++iz) {
            const float cx = startX + static_cast<float>(ix) * kStudSpacing;
            const float cz = startZ + static_cast<float>(iz) * kStudSpacing;
            const unsigned short base = static_cast<unsigned short>(vertices.size());

            for (int i = 0; i < kStudSegments; ++i) {
                const float angle = 2.0f * PI * static_cast<float>(i) /
                                    static_cast<float>(kStudSegments);
                const Vector3 normal{std::cos(angle), 0.0f, std::sin(angle)};
                vertices.push_back(
                    Vector3{cx + normal.x * kStudRadius, topLocal, cz + normal.z * kStudRadius});
                normals.push_back(normal);
                colors.push_back(sideColor);
            }
            for (int i = 0; i < kStudSegments; ++i) {
                const float angle = 2.0f * PI * static_cast<float>(i) /
                                    static_cast<float>(kStudSegments);
                const Vector3 normal{std::cos(angle), 0.0f, std::sin(angle)};
                vertices.push_back(Vector3{cx + normal.x * kStudRadius, topLocal + kStudHeight,
                                           cz + normal.z * kStudRadius});
                normals.push_back(normal);
                colors.push_back(sideColor);
            }
            for (int i = 0; i < kStudSegments; ++i) {
                const float angle = 2.0f * PI * static_cast<float>(i) /
                                    static_cast<float>(kStudSegments);
                vertices.push_back(Vector3{cx + std::cos(angle) * kStudRadius,
                                           topLocal + kStudHeight,
                                           cz + std::sin(angle) * kStudRadius});
                normals.push_back(Vector3{0.0f, 1.0f, 0.0f});
                colors.push_back(topColor);
            }
            vertices.push_back(Vector3{cx, topLocal + kStudHeight, cz});
            normals.push_back(Vector3{0.0f, 1.0f, 0.0f});
            colors.push_back(topColor);

            const unsigned short center = static_cast<unsigned short>(vertices.size() - 1);
            for (int i = 0; i < kStudSegments; ++i) {
                const unsigned short next =
                    static_cast<unsigned short>((i + 1) % kStudSegments);
                // Laterais.
                indices.push_back(static_cast<unsigned short>(base + i));
                indices.push_back(static_cast<unsigned short>(base + kStudSegments + i));
                indices.push_back(static_cast<unsigned short>(base + kStudSegments + next));
                indices.push_back(static_cast<unsigned short>(base + i));
                indices.push_back(static_cast<unsigned short>(base + kStudSegments + next));
                indices.push_back(static_cast<unsigned short>(base + next));
                // Topo.
                indices.push_back(center);
                indices.push_back(static_cast<unsigned short>(base + 2 * kStudSegments + next));
                indices.push_back(static_cast<unsigned short>(base + 2 * kStudSegments + i));
            }
        }
    }

    Mesh mesh{};
    mesh.vertexCount = static_cast<int>(vertices.size());
    mesh.triangleCount = static_cast<int>(indices.size() / 3);
    mesh.vertices = static_cast<float*>(MemAlloc(sizeof(float) * 3 * vertices.size()));
    mesh.normals = static_cast<float*>(MemAlloc(sizeof(float) * 3 * vertices.size()));
    mesh.colors = static_cast<unsigned char*>(MemAlloc(sizeof(unsigned char) * 4 * vertices.size()));
    mesh.indices = static_cast<unsigned short*>(MemAlloc(sizeof(unsigned short) * indices.size()));
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        mesh.vertices[i * 3 + 0] = vertices[i].x;
        mesh.vertices[i * 3 + 1] = vertices[i].y;
        mesh.vertices[i * 3 + 2] = vertices[i].z;
        mesh.normals[i * 3 + 0] = normals[i].x;
        mesh.normals[i * 3 + 1] = normals[i].y;
        mesh.normals[i * 3 + 2] = normals[i].z;
        mesh.colors[i * 4 + 0] = colors[i].r;
        mesh.colors[i * 4 + 1] = colors[i].g;
        mesh.colors[i * 4 + 2] = colors[i].b;
        mesh.colors[i * 4 + 3] = colors[i].a;
    }
    for (std::size_t i = 0; i < indices.size(); ++i) {
        mesh.indices[i] = indices[i];
    }
    UploadMesh(&mesh, false);
    return mesh;
}


// Superelipsoide (expoente ~4.5): silhueta de bloco com cantos suavizados.
// Malha fechada, sem emendas, custo comparável à esfera facetada existente.
void DrawShadedRoundedBlocky(Vector3 center, Vector3 size, Quaternion rotation, Color color,
                             Vector3 lightDirection, float sunIntensity, float brightness) {
    constexpr int rings = 6;
    constexpr int slices = 14;
    constexpr float exponent = 0.444f;  // 2/n com n ~4.5
    const Vector3 half = Vector3Scale(size, 0.5f);
    const auto blocky = [&](Vector3 d) {
        Vector3 p;
        p.x = half.x * std::copysign(std::pow(std::fabs(d.x), exponent), d.x);
        p.y = half.y * std::copysign(std::pow(std::fabs(d.y), exponent), d.y);
        p.z = half.z * std::copysign(std::pow(std::fabs(d.z), exponent), d.z);
        return p;
    };
    rlBegin(RL_TRIANGLES);
    for (int ring = 0; ring < rings; ++ring) {
        const float v0 = (float)ring / rings, v1 = (float)(ring + 1) / rings;
        const float phi0 = (v0 - 0.5f) * PI, phi1 = (v1 - 0.5f) * PI;
        for (int slice = 0; slice < slices; ++slice) {
            const float u0 = (float)slice / slices, u1 = (float)(slice + 1) / slices;
            const float theta0 = u0 * 2.0f * PI, theta1 = u1 * 2.0f * PI;
            const Vector3 d00{std::cos(theta0) * std::cos(phi0), std::sin(phi0),
                              std::sin(theta0) * std::cos(phi0)};
            const Vector3 d10{std::cos(theta1) * std::cos(phi0), std::sin(phi0),
                              std::sin(theta1) * std::cos(phi0)};
            const Vector3 d01{std::cos(theta0) * std::cos(phi1), std::sin(phi1),
                              std::sin(theta0) * std::cos(phi1)};
            const Vector3 d11{std::cos(theta1) * std::cos(phi1), std::sin(phi1),
                              std::sin(theta1) * std::cos(phi1)};
            const Vector3 p00 = Vector3Add(center, Vector3RotateByQuaternion(blocky(d00), rotation));
            const Vector3 p10 = Vector3Add(center, Vector3RotateByQuaternion(blocky(d10), rotation));
            const Vector3 p01 = Vector3Add(center, Vector3RotateByQuaternion(blocky(d01), rotation));
            const Vector3 p11 = Vector3Add(center, Vector3RotateByQuaternion(blocky(d11), rotation));
            Vector3 normal = Vector3Normalize(Vector3CrossProduct(
                Vector3Subtract(p10, p00), Vector3Subtract(p01, p00)));
            normal = Vector3RotateByQuaternion(normal, rotation);
            const float lambert = std::max(0.0f, Vector3DotProduct(normal, lightDirection));
            const float factor = std::clamp((0.5f + 0.5f * sunIntensity * lambert) * brightness, 0.2f, 1.6f);
            SetColor4(Shade(color, factor));
            Triangle(p00, p10, p11);
            Triangle(p00, p11, p01);
        }
    }
    rlEnd();
}

// Rosto aplicado a face -Z da cabeca (lazy-load, sem caminho absoluto no
// projeto; o fallback mantem os olhos minimos se a imagem nao existir).
Texture2D& FaceTexture() {
    static Texture2D tex{};
    static bool attempted = false;
    if (!attempted) {
        attempted = true;
        for (const char* path : {"rosto1.png", "../rosto1.png", "../../rosto1.png", "../../../rosto1.png"}) {
            if (FileExists(path)) { tex = LoadTexture(path); break; }
        }
    }
    return tex;
}

void DrawHeadFace(const game::RigidTransform& headWorld, Vector3 headSize) {
    Texture2D& tex = FaceTexture();
    const Vector3 half = Vector3Scale(headSize, 0.5f);
    const float offset = half.z + 0.02f;
    const Vector3 local[4] = {
        {-half.x,  half.y, -offset},  // topo-esquerda
        { half.x,  half.y, -offset},  // topo-direita
        { half.x, -half.y, -offset},  // baixo-direita
        {-half.x, -half.y, -offset},  // baixo-esquerda
    };
    Vector3 world[4];
    for (int i = 0; i < 4; ++i) {
        world[i] = Vector3Add(headWorld.position,
                              Vector3RotateByQuaternion(local[i], headWorld.rotation));
    }
    rlSetTexture(tex.id);
    rlBegin(RL_QUADS);
    rlColor4ub(255, 255, 255, 255);
    const float uv[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (int i = 0; i < 4; ++i) {
        rlTexCoord2f(uv[i][0], uv[i][1]);
        rlVertex3f(world[i].x, world[i].y, world[i].z);
    }
    rlEnd();
    rlSetTexture(0);
}

}  // namespace

SceneRenderer::SceneRenderer() {
    studMaterial_ = LoadMaterialDefault();
}

SceneRenderer::~SceneRenderer() {
    unloadStudMeshes();
    // LoadMaterialDefault allocates the maps even though shader/texture are
    // shared defaults. UnloadMaterial preserves those defaults and frees maps.
    UnloadMaterial(studMaterial_);
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

void SceneRenderer::ensureStudMesh(const Part& baseplate) {
    StudMeshEntry& entry = studMeshes_[baseplate.id()];
    if (entry.valid && NearlyEqual(entry.size, baseplate.size(), 0.01f)) {
        return;
    }
    if (entry.valid) {
        UnloadMesh(entry.mesh);
        entry.mesh = Mesh{};
        entry.valid = false;
    }
    entry.mesh = BuildStudMesh(baseplate.size());
    entry.size = baseplate.size();
    entry.valid = true;
}

void SceneRenderer::unloadStudMeshes() {
    for (auto& pair : studMeshes_) {
        if (pair.second.valid) {
            UnloadMesh(pair.second.mesh);
        }
    }
    studMeshes_.clear();
}

void SceneRenderer::drawBaseplate(const Part& baseplate, Vector3 lightDirection,
                                  float sunIntensity, float brightness) {
    const Quaternion rotation = baseplate.rotation();
    DrawShadedBox(baseplate.position(), baseplate.size(), rotation, MaterialColor(baseplate),
                  lightDirection, sunIntensity, brightness);

    ensureStudMesh(baseplate);
    const StudMeshEntry& entry = studMeshes_[baseplate.id()];
    if (entry.valid) {
        studMaterial_.maps[MATERIAL_MAP_DIFFUSE].color = MaterialColor(baseplate);
        const Matrix transform = MatrixMultiply(
            QuaternionToMatrix(rotation),
            MatrixTranslate(baseplate.position().x, baseplate.position().y,
                            baseplate.position().z));
        DrawMesh(entry.mesh, studMaterial_, transform);
        studMaterial_.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
    }

    // Grade discreta por cima do piso (referencia espacial para construcao).
    // Os pontos sao locais a Baseplate: se ela for rotacionada, grade e studs
    // acompanham a mesma orientacao.
    const Vector3 half = Vector3Scale(baseplate.size(), 0.5f);
    const float top = half.y + 0.02f;
    const float minX = -half.x;
    const float maxX = half.x;
    const float minZ = -half.z;
    const float maxZ = half.z;
    const Color gridColor{132, 132, 138, 255};
    const float spacing = 16.0f;
    const int stepsX = static_cast<int>((maxX - minX) / spacing);
    const int stepsZ = static_cast<int>((maxZ - minZ) / spacing);
    const auto worldPoint = [&](float x, float z) {
        return Vector3Add(baseplate.position(),
                          Vector3RotateByQuaternion(Vector3{x, top, z}, rotation));
    };
    for (int i = 0; i <= stepsX; ++i) {
        const float x = minX + static_cast<float>(i) * spacing;
        DrawLine3D(worldPoint(x, minZ), worldPoint(x, maxZ), gridColor);
    }
    for (int i = 0; i <= stepsZ; ++i) {
        const float z = minZ + static_cast<float>(i) * spacing;
        DrawLine3D(worldPoint(minX, z), worldPoint(maxX, z), gridColor);
    }
}

void SceneRenderer::drawSelection(const Part& part) const {
    const Color selection{255, 205, 60, 255};
    if (part.shape() == PartShape::Sphere) {
        DrawSphereWires(part.position(), part.sphereRadius() * 1.04f, 8, 12, selection);
    } else {
        DrawRotatedBoxWires(part.position(), Vector3Scale(part.size(), 1.02f), part.rotation(),
                            selection);
    }
}

void SceneRenderer::drawScene(const Scene& scene, const EditorCamera& camera, int width,
                              int height, InstanceId selectedId, const RigRenderState* rig) {
    drawScene(scene, camera.camera(), width, height, selectedId, rig);
}

void SceneRenderer::drawRig(const RigRenderState& state, Vector3 light, float intensity, float brightness) {
    if (!state.rig) return;
    const auto& rig = *state.rig;
    const auto world = rig.evaluate(state.pose, state.root);
    for (int i = 0; i < game::kBodyCount; ++i) {
        const auto& body = rig.bodies[i];
        const Color bodyColor = MaterialColor(body.color, body.material);
        const float bodyIntensity = body.material == PartMaterial::Neon ? 0.f : intensity;
        const float bodyBrightness = body.material == PartMaterial::Neon ? 1.4f : brightness;
        if (i == game::Head)
            DrawShadedRoundedBlocky(world[i].position, body.size, world[i].rotation, bodyColor,
                                    light, bodyIntensity, bodyBrightness);
        else
            DrawShadedBox(world[i].position, body.size, world[i].rotation, bodyColor, light,
                          bodyIntensity, bodyBrightness);
        if (i == state.selectedBody) DrawRotatedBoxWires(world[i].position,
            Vector3Scale(body.size, 1.02f), world[i].rotation, GOLD);
    }
    // Rosto do usuário quando existir; sem arquivo, mantém os olhos mínimos.
    if (FaceTexture().id != 0) {
        DrawHeadFace(world[game::Head], rig.bodies[game::Head].size);
    } else {
        // Original minimal face marks the -Z front without external textures.
        for (float side : {-1.f, 1.f}) {
            const auto eye = game::Compose(world[game::Head], {{side*.35f, .08f, -rig.bodies[game::Head].size.z*.5f-.015f}});
            DrawShadedBox(eye.position, {.13f, .15f, .035f}, eye.rotation, DARKGRAY, light, 0, 1);
        }
    }
    if (state.pivots) {
        for (const auto& joint : rig.joints) {
            const auto pivot = game::Compose(joint.parent < 0 ? state.root : world[joint.parent], joint.parentAnchor);
            DrawSphere(pivot.position, .10f, ORANGE);
            DrawLine3D(pivot.position, world[joint.child].position, YELLOW);
        }
    }
}

void SceneRenderer::drawScene(const Scene& scene, Camera3D camera, int width,
                              int height, InstanceId selectedId, const RigRenderState* rig,
                              bool authoredRigs) {
    const Lighting* lighting = scene.lighting();
    const Color sky = lighting != nullptr ? lighting->skyColor() : Color{120, 180, 240, 255};
    const float sunIntensity = lighting != nullptr ? lighting->sunIntensity() : 1.0f;
    const float brightness = lighting != nullptr ? lighting->brightness() : 1.0f;
    Vector3 lightDirection = lighting != nullptr ? lighting->sunDirection()
                                                 : Vector3{-0.45f, -1.0f, -0.35f};
    lightDirection = Vector3Normalize(Vector3Negate(lightDirection));

    ClearBackground(sky);
    DrawRectangleGradientV(0, 0, width, height, sky, Shade(sky, 1.22f));

    BeginMode3D(camera);
    rlEnableDepthTest();
    rlDisableBackfaceCulling();
    for (Part* part : scene.parts()) {
        if (part == nullptr) {
            continue;
        }
        if (part->instanceClass() == InstanceClass::Baseplate) {
            drawBaseplate(*part, lightDirection, sunIntensity, brightness);
        } else if (part->instanceClass() == InstanceClass::SpawnLocation) {
            const Quaternion rotation = part->rotation();
            const auto localBox = [&](Vector3 localCenter, Vector3 size, Color color) {
                const Vector3 world = Vector3Add(
                    part->position(), Vector3RotateByQuaternion(localCenter, rotation));
                DrawShadedBox(world, size, rotation, color, lightDirection, sunIntensity,
                              brightness);
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
                             lightDirection, sunIntensity, brightness);
        } else {
            DrawShadedBox(part->position(), part->size(), part->rotation(),
                          MaterialColor(*part), lightDirection, sunIntensity, brightness);
        }

        if (part->id() == selectedId) {
            drawSelection(*part);
        }
    }
    if (authoredRigs) for (const auto& asset : scene.gameAssets().rigs) {
        if (rig && rig->rig && rig->rig->id == asset.id) continue;
        drawRig({&asset, {}, asset.root}, lightDirection, sunIntensity, brightness);
    }
    if (rig) drawRig(*rig, lightDirection, sunIntensity, brightness);
    EndMode3D();
}

}  // namespace loblox
