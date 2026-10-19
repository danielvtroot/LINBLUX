// Testes headless (sem janela/OpenGL). Executados pelo CTest.
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "objects/Baseplate.h"
#include "objects/SpawnLocation.h"
#include "physics/PhysicsWorld.h"
#include "scene/Scene.h"
#include "serialization/ProjectIO.h"

namespace {

int gChecks = 0;
int gFailures = 0;
int gSectionFailures = 0;
const char* gSection = "";

void Section(const char* name) {
    gSection = name;
    gSectionFailures = 0;
    std::printf("\n[%s]\n", name);
}

void CheckTrue(bool condition, const char* expression, int line) {
    ++gChecks;
    if (!condition) {
        ++gFailures;
        ++gSectionFailures;
        std::printf("  FAIL linha %d: %s\n", line, expression);
    }
}

void CheckNear(float actual, float expected, float epsilon, const char* expression, int line) {
    ++gChecks;
    if (!(std::fabs(actual - expected) <= epsilon)) {
        ++gFailures;
        ++gSectionFailures;
        std::printf("  FAIL linha %d: %s (obtido %.4f, esperado %.4f +/- %.4f)\n", line,
                    expression, actual, expected, epsilon);
    }
}

void CheckString(const std::string& actual, const std::string& expected, const char* expression,
                 int line) {
    ++gChecks;
    if (actual != expected) {
        ++gFailures;
        ++gSectionFailures;
        std::printf("  FAIL linha %d: %s (obtido '%s', esperado '%s')\n", line, expression,
                    actual.c_str(), expected.c_str());
    }
}

void EndSection() {
    if (gSectionFailures == 0) {
        std::printf("  ok\n");
    }
}

#define CHECK(condition) CheckTrue((condition), #condition, __LINE__)
#define CHECK_NEAR(actual, expected, epsilon) \
    CheckNear((actual), (expected), (epsilon), #actual, __LINE__)
#define CHECK_STR(actual, expected) CheckString((actual), (expected), #actual, __LINE__)

void Simulate(loblox::PhysicsWorld& physics, loblox::Scene& scene, float seconds) {
    const float step = 1.0f / 60.0f;
    const int steps = static_cast<int>(seconds / step) + 1;
    for (int i = 0; i < steps; ++i) {
        physics.simulate(scene, step);
    }
}

void TestDefaultScene() {
    Section("Cena padrao e hierarquia");
    loblox::Scene scene;
    scene.buildDefaultScene();

    CHECK(scene.workspace() != nullptr);
    CHECK(scene.lighting() != nullptr);
    CHECK_STR(scene.workspace()->name(), "Workspace");
    CHECK_STR(scene.lighting()->name(), "Lighting");
    CHECK(scene.workspace()->name() == "Workspace");
    CHECK(scene.workspace()->children().size() == 2);

    loblox::Baseplate* baseplate = nullptr;
    loblox::SpawnLocation* spawn = nullptr;
    for (loblox::Part* part : scene.parts()) {
        if (part->instanceClass() == loblox::InstanceClass::Baseplate) {
            baseplate = static_cast<loblox::Baseplate*>(part);
        } else if (part->instanceClass() == loblox::InstanceClass::SpawnLocation) {
            spawn = static_cast<loblox::SpawnLocation*>(part);
        }
    }
    CHECK(baseplate != nullptr);
    CHECK(spawn != nullptr);
    if (baseplate != nullptr) {
        CHECK(baseplate->anchored());
        CHECK(baseplate->canCollide());
        CHECK(!baseplate->deletable());
        CHECK_STR(baseplate->className(), "Baseplate");
        CHECK_NEAR(baseplate->position().y, -0.5f, 0.001f);
        CHECK(baseplate->parent() == scene.workspace());
    }
    if (spawn != nullptr) {
        CHECK(spawn->anchored());
        CHECK_STR(spawn->className(), "SpawnLocation");
        CHECK(spawn->parent() == scene.workspace());
    }
    CHECK(!scene.remove(baseplate));
    CHECK(scene.parts().size() == 2);
    EndSection();
}

void TestCreateAndRemove() {
    Section("Criacao, nomes, propriedades e remocao");
    loblox::Scene scene;
    scene.buildDefaultScene();

    loblox::Part* part = scene.createPart(loblox::PartShape::Block, loblox::V3(0, 4, 0));
    CHECK(part != nullptr);
    if (part == nullptr) {
        EndSection();
        return;
    }
    CHECK_STR(part->name(), "Part");
    CHECK_STR(part->className(), "Part");
    CHECK(part->parent() == scene.workspace());
    CHECK(scene.find(part->id()) == part);
    CHECK(scene.parts().size() == 3);

    loblox::Part* second = scene.createPart(loblox::PartShape::Sphere, loblox::V3(8, 4, 0));
    CHECK(second != nullptr);
    if (second != nullptr) {
        CHECK_STR(second->name(), "Sphere");
        CHECK(second->id() != part->id());
        CHECK(second->shape() == loblox::PartShape::Sphere);
    }

    part->setPosition(loblox::V3(1, 2, 3));
    part->setSize(loblox::V3(5, 6, 7));
    part->setRotationEulerDegrees(loblox::V3(0, 45, 0));
    part->setColor(Color{10, 20, 30, 255});
    part->setMaterial(loblox::PartMaterial::Wood);
    part->setAnchored(true);
    part->setCanCollide(false);

    CHECK_NEAR(part->position().x, 1.0f, 0.001f);
    CHECK_NEAR(part->size().z, 7.0f, 0.001f);
    CHECK(part->material() == loblox::PartMaterial::Wood);
    CHECK(part->anchored());
    CHECK(!part->canCollide());
    CHECK_NEAR(part->rotationEulerDegrees().y, 45.0f, 0.01f);

    part->setSize(loblox::V3(0.0f, -4.0f, 0.01f));
    CHECK(part->size().x >= loblox::kMinPartSize);
    CHECK(part->size().y >= loblox::kMinPartSize);
    CHECK(part->size().z >= loblox::kMinPartSize);

    scene.select(part);
    CHECK(scene.selection() == part);
    const size_t countBefore = scene.objects().size();
    const loblox::InstanceId removedId = part->id();
    CHECK(scene.remove(part));
    CHECK(scene.objects().size() == countBefore - 1);
    CHECK(scene.selection() == nullptr);
    CHECK(scene.find(removedId) == nullptr);
    EndSection();
}

void TestRemovalListener() {
    Section("Listener de remocao (fisica)");
    loblox::Scene scene;
    loblox::Lighting* lighting = scene.lighting();
    loblox::Part* part = scene.createPart(loblox::PartShape::Block, loblox::V3(0, 4, 0));
    int removedParts = 0;
    scene.addRemovalListener([&removedParts](loblox::Instance* object) {
        if (object->isPart()) {
            ++removedParts;
        }
    });
    CHECK(scene.remove(part));
    CHECK(removedParts == 1);
    CHECK(scene.remove(lighting) == false);
    CHECK(removedParts == 1);
    EndSection();
}

void TestAnchoredAndDynamic() {
    Section("Anchored x dinamico");
    loblox::Scene scene;
    scene.buildDefaultScene();

    loblox::PhysicsWorld physics;
    physics.attach(scene);
    CHECK(physics.bodyCount() == scene.parts().size());

    loblox::Part* anchored = scene.createPart(loblox::PartShape::Block, loblox::V3(-10, 20, 0));
    anchored->setAnchored(true);
    physics.addPart(anchored);
    CHECK(!physics.isDynamic(anchored->id()));
    Simulate(physics, scene, 1.5f);
    CHECK_NEAR(anchored->position().y, 20.0f, 0.001f);

    loblox::Part* dynamic = scene.createPart(loblox::PartShape::Block, loblox::V3(10, 20, 0));
    dynamic->setAnchored(false);
    physics.addPart(dynamic);
    CHECK(physics.isDynamic(dynamic->id()));
    Simulate(physics, scene, 2.5f);
    // Baseplate topo em y=0; caixa 4x4x4 repousa com centro em y=2.
    CHECK_NEAR(dynamic->position().y, 2.0f, 0.25f);
    CHECK(std::fabs(dynamic->position().x) < 25.0f);

    // Alternar anchored no meio da simulacao: deve parar imediatamente.
    dynamic->setAnchored(true);
    physics.applyChanges(dynamic);
    CHECK(!physics.isDynamic(dynamic->id()));
    const float stoppedY = dynamic->position().y;
    Simulate(physics, scene, 1.0f);
    CHECK_NEAR(dynamic->position().y, stoppedY, 0.001f);

    // Voltar a dinamico deve cair de novo.
    dynamic->setAnchored(false);
    dynamic->setPosition(loblox::V3(10, 12, 0));
    physics.applyChanges(dynamic);
    Simulate(physics, scene, 2.5f);
    CHECK_NEAR(dynamic->position().y, 2.0f, 0.25f);
    EndSection();
}

void TestCanCollide() {
    Section("CanCollide");
    loblox::Scene scene;
    scene.buildDefaultScene();
    loblox::PhysicsWorld physics;
    physics.attach(scene);

    loblox::Part* ghost = scene.createPart(loblox::PartShape::Block, loblox::V3(20, 12, 0));
    ghost->setAnchored(false);
    ghost->setCanCollide(false);
    physics.addPart(ghost);
    Simulate(physics, scene, 2.0f);
    CHECK(ghost->position().y < -5.0f);

    loblox::Part* solid = scene.createPart(loblox::PartShape::Block, loblox::V3(-20, 12, 0));
    solid->setAnchored(false);
    physics.addPart(solid);
    Simulate(physics, scene, 2.5f);
    CHECK_NEAR(solid->position().y, 2.0f, 0.25f);
    EndSection();
}

void TestTransformSyncAndRaycast() {
    Section("Sincronizacao transformacao/fisica e raycast");
    loblox::Scene scene;
    scene.buildDefaultScene();
    loblox::PhysicsWorld physics;
    physics.attach(scene);

    loblox::Part* part = scene.createPart(loblox::PartShape::Block, loblox::V3(0, 10, 0));
    part->setAnchored(true);
    physics.addPart(part);
    physics.simulate(scene, 1.0f / 60.0f);
    CHECK_NEAR(physics.bodyPosition(part->id()).y, 10.0f, 0.001f);

    part->setPosition(loblox::V3(4, 6, 2));
    physics.syncScene(scene);
    CHECK_NEAR(physics.bodyPosition(part->id()).y, 6.0f, 0.001f);
    physics.simulate(scene, 1.0f / 60.0f);
    CHECK_NEAR(part->position().y, 6.0f, 0.001f);

    loblox::Part* hit = nullptr;
    Vector3 point;
    const bool found = physics.raycast(loblox::V3(4, 20, 2), loblox::V3(0, -1, 0), 30.0f, &hit,
                                       &point);
    CHECK(found);
    CHECK(hit == part);
    if (found) {
        CHECK_NEAR(point.y, 8.0f, 0.1f);
    }

    // Mover a part move o corpo; raio antigo nao deve mais acertar a part.
    part->setPosition(loblox::V3(40, 6, 2));
    physics.syncScene(scene);
    loblox::Part* hitAfter = nullptr;
    physics.raycast(loblox::V3(4, 20, 2), loblox::V3(0, -1, 0), 30.0f, &hitAfter);
    CHECK(hitAfter != part);

    // Sem corpos duplicados ao re-adicionar a mesma Part.
    const size_t count = physics.bodyCount();
    physics.addPart(part);
    CHECK(physics.bodyCount() == count);

    // Exclusao remove o corpo e o raio deixa de acertar.
    const loblox::InstanceId id = part->id();
    scene.remove(part);
    CHECK(!physics.hasBody(id));
    CHECK(physics.bodyCount() == count - 1);
    loblox::Part* hitDeleted = nullptr;
    physics.raycast(loblox::V3(40, 20, 2), loblox::V3(0, -1, 0), 30.0f, &hitDeleted);
    CHECK(hitDeleted != part || hitDeleted == nullptr);
    EndSection();
}

void TestResizeKeepsSingleBody() {
    Section("Redimensionar mantem um corpo unico");
    loblox::Scene scene;
    scene.buildDefaultScene();
    loblox::PhysicsWorld physics;
    physics.attach(scene);

    loblox::Part* part = scene.createPart(loblox::PartShape::Block, loblox::V3(25, 12, 0));
    part->setAnchored(false);
    physics.addPart(part);
    const size_t bodies = physics.bodyCount();
    Simulate(physics, scene, 2.5f);
    CHECK_NEAR(part->position().y, 2.0f, 0.25f);

    part->setPosition(loblox::V3(25, 14, 0));
    part->setSize(loblox::V3(8, 2, 8));
    physics.simulate(scene, 1.0f / 60.0f);
    CHECK(physics.bodyCount() == bodies);
    CHECK(physics.isDynamic(part->id()));

    Simulate(physics, scene, 2.5f);
    CHECK_NEAR(part->position().y, 1.0f, 0.25f);
    CHECK(physics.bodyCount() == bodies);
    EndSection();
}

void TestProjectRoundTrip() {
    Section("Salvar/carregar JSON");
    loblox::Scene scene;
    scene.buildDefaultScene();

    loblox::Part* block = scene.createPart(loblox::PartShape::Block, loblox::V3(3, 5, -2),
                                           loblox::V3(6, 2, 4), "MinhaPeca");
    block->setRotationEulerDegrees(loblox::V3(10, 20, 30));
    block->setColor(Color{200, 50, 25, 255});
    block->setMaterial(loblox::PartMaterial::Metal);
    block->setAnchored(false);

    loblox::Part* sphere = scene.createPart(loblox::PartShape::Sphere, loblox::V3(-7, 3, 1),
                                            loblox::V3(3, 3, 3), "Bola");
    sphere->setCanCollide(false);
    sphere->setAnchored(true);
    sphere->setColor(Color{20, 220, 90, 255});
    sphere->setMaterial(loblox::PartMaterial::Neon);

    scene.lighting()->setSkyColor(Color{30, 40, 90, 255});
    scene.lighting()->setSunIntensity(1.7f);
    scene.lighting()->setBrightness(0.8f);

    const std::string path = "/tmp/loblox_test_project.json";
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    std::filesystem::remove(path + ".tmp", ignored);

    std::string error;
    CHECK(loblox::projectio::save(scene, path, error));
    CHECK(error.empty());
    CHECK(std::filesystem::exists(path));
    CHECK(!std::filesystem::exists(path + ".tmp"));

    std::ifstream in(path);
    std::ostringstream text;
    text << in.rdbuf();
    CHECK(text.str().find("\"format\": \"loblox-project\"") != std::string::npos);
    CHECK(text.str().find("\"version\": 1") != std::string::npos);

    loblox::Scene loaded;
    CHECK(loblox::projectio::load(loaded, path, error));
    CHECK(error.empty());
    CHECK(loaded.parts().size() == scene.parts().size());
    CHECK(loaded.objects().size() == scene.objects().size());
    CHECK_STR(loaded.workspace()->name(), scene.workspace()->name());
    CHECK_STR(loaded.lighting()->name(), scene.lighting()->name());

    loblox::Part* loadedBlock = loaded.findPart(block->id());
    CHECK(loadedBlock != nullptr);
    if (loadedBlock != nullptr) {
        CHECK_STR(loadedBlock->name(), "MinhaPeca");
        CHECK(loadedBlock->shape() == loblox::PartShape::Block);
        CHECK_NEAR(loadedBlock->position().x, 3.0f, 0.001f);
        CHECK_NEAR(loadedBlock->position().y, 5.0f, 0.001f);
        CHECK_NEAR(loadedBlock->position().z, -2.0f, 0.001f);
        CHECK_NEAR(loadedBlock->size().x, 6.0f, 0.001f);
        CHECK_NEAR(loadedBlock->size().z, 4.0f, 0.001f);
        CHECK_NEAR(loadedBlock->rotationEulerDegrees().y, 20.0f, 0.01f);
        CHECK_NEAR(static_cast<float>(loadedBlock->color().r), 200.0f, 0.5f);
        CHECK(loadedBlock->material() == loblox::PartMaterial::Metal);
        CHECK(!loadedBlock->anchored());
        CHECK(loadedBlock->canCollide());
        CHECK(loadedBlock->parent() == loaded.workspace());
    }

    loblox::Part* loadedSphere = loaded.findPart(sphere->id());
    CHECK(loadedSphere != nullptr);
    if (loadedSphere != nullptr) {
        CHECK(loadedSphere->shape() == loblox::PartShape::Sphere);
        CHECK(!loadedSphere->canCollide());
        CHECK(loadedSphere->material() == loblox::PartMaterial::Neon);
    }

    CHECK_NEAR(static_cast<float>(loaded.lighting()->skyColor().b), 90.0f, 0.5f);
    CHECK_NEAR(loaded.lighting()->sunIntensity(), 1.7f, 0.001f);
    CHECK_NEAR(loaded.lighting()->brightness(), 0.8f, 0.001f);

    // A cena recarregada precisa reconstruir corpos fisicos coerentes.
    loblox::PhysicsWorld physics;
    physics.attach(loaded);
    CHECK(physics.bodyCount() == loaded.parts().size());
    loblox::Part* falling = loaded.findPart(block->id());
    CHECK(falling != nullptr);
    if (falling != nullptr) {
        CHECK(physics.isDynamic(falling->id()));
        Simulate(physics, loaded, 2.5f);
        CHECK(falling->position().y < 5.0f);
        CHECK(falling->position().y > -5.0f);
    }

    // Ids continuam unicos depois de carregar.
    loblox::Part* extra = loaded.createPart(loblox::PartShape::Block, loblox::V3(0, 10, 0));
    CHECK(extra != nullptr);
    if (extra != nullptr) {
        CHECK(loaded.findPart(extra->id()) == extra);
        CHECK(extra->id() != block->id());
        CHECK(extra->id() != sphere->id());
    }
    EndSection();
}

void TestProjectValidation() {
    Section("Validacao de projetos invalidos");
    loblox::Scene scene;
    scene.buildDefaultScene();
    const size_t originalParts = scene.parts().size();
    loblox::PhysicsWorld physics;
    physics.attach(scene);

    std::string error;
    CHECK(!loblox::projectio::fromJson(scene, "{ isto nao e json", error));
    CHECK(!error.empty());
    CHECK(scene.parts().size() == originalParts);

    CHECK(!loblox::projectio::fromJson(scene, "{\"format\":\"loblox-project\",\"version\":99,\"objects\":[]}", error));
    CHECK(error.find("versao") != std::string::npos);

    CHECK(!loblox::projectio::fromJson(scene,
                                      "{\"format\":\"loblox-project\",\"version\":1,"
                                      "\"objects\":[{\"id\":1,\"class\":\"Workspace\","
                                      "\"name\":\"Workspace\",\"parent\":0}]}",
                                      error));
    CHECK(error.find("Lighting") != std::string::npos);

    // Duas Parts com o mesmo id.
    const std::string duplicateId =
        R"({"format":"loblox-project","version":1,"objects":[
        {"id":1,"class":"Workspace","name":"Workspace","parent":0},
        {"id":2,"class":"Lighting","name":"Lighting","parent":0,
         "skyColor":[1,2,3,255],"ambientColor":[1,2,3,255],
         "sunDirection":[0,-1,0],"sunIntensity":1,"brightness":1},
        {"id":3,"class":"Part","name":"A","parent":1,"shape":"Block",
         "position":[0,4,0],"rotation":[0,0,0],"size":[4,4,4],
         "color":[1,2,3,255],"material":"Plastic","anchored":false,"canCollide":true},
        {"id":3,"class":"Part","name":"B","parent":1,"shape":"Block",
         "position":[0,4,0],"rotation":[0,0,0],"size":[4,4,4],
         "color":[1,2,3,255],"material":"Plastic","anchored":false,"canCollide":true}]})";
    CHECK(!loblox::projectio::fromJson(scene, duplicateId, error));
    CHECK(error.find("duplicado") != std::string::npos);

    // Tamanho invalido.
    const std::string badSize =
        R"({"format":"loblox-project","version":1,"objects":[
        {"id":1,"class":"Workspace","name":"Workspace","parent":0},
        {"id":2,"class":"Lighting","name":"Lighting","parent":0,
         "skyColor":[1,2,3,255],"ambientColor":[1,2,3,255],
         "sunDirection":[0,-1,0],"sunIntensity":1,"brightness":1},
        {"id":3,"class":"Part","name":"A","parent":1,"shape":"Block",
         "position":[0,4,0],"rotation":[0,0,0],"size":[4,-4,4],
         "color":[1,2,3,255],"material":"Plastic","anchored":false,"canCollide":true}]})";
    CHECK(!loblox::projectio::fromJson(scene, badSize, error));
    CHECK(error.find("tamanho") != std::string::npos || error.find("size") != std::string::npos);

    // Part fora do Workspace (parent aponta para Lighting).
    const std::string badParent =
        R"({"format":"loblox-project","version":1,"objects":[
        {"id":1,"class":"Workspace","name":"Workspace","parent":0},
        {"id":2,"class":"Lighting","name":"Lighting","parent":0,
         "skyColor":[1,2,3,255],"ambientColor":[1,2,3,255],
         "sunDirection":[0,-1,0],"sunIntensity":1,"brightness":1},
        {"id":3,"class":"Part","name":"A","parent":2,"shape":"Block",
         "position":[0,4,0],"rotation":[0,0,0],"size":[4,4,4],
         "color":[1,2,3,255],"material":"Plastic","anchored":false,"canCollide":true}]})";
    CHECK(!loblox::projectio::fromJson(scene, badParent, error));

    // Id de Part conflitando com um servico da cena atual: rejeitar antes de
    // limpar a cena (o arquivo tem ids unicos, mas usa Workspace/Lighting 7/8).
    const std::string serviceConflict =
        R"({"format":"loblox-project","version":1,"objects":[
        {"id":7,"class":"Workspace","name":"Workspace","parent":0},
        {"id":8,"class":"Lighting","name":"Lighting","parent":0,
         "skyColor":[1,2,3,255],"ambientColor":[1,2,3,255],
         "sunDirection":[0,-1,0],"sunIntensity":1,"brightness":1},
        {"id":1,"class":"Part","name":"Conflito","parent":7,"shape":"Block",
         "position":[0,4,0],"rotation":[0,0,0],"size":[4,4,4],
         "color":[1,2,3,255],"material":"Plastic","anchored":false,"canCollide":true}]})";
    CHECK(!loblox::projectio::fromJson(scene, serviceConflict, error));
    CHECK(error.find("servico") != std::string::npos);

    // Falha nao deve deixar a cena vazia nem corpos orfaos.
    CHECK(scene.parts().size() == originalParts);
    CHECK(physics.bodyCount() == originalParts);

    // Arquivo inexistente.
    CHECK(!loblox::projectio::load(scene, "/tmp/nao_existe_loblox.json", error));
    CHECK(!error.empty());
    EndSection();
}

void TestBaseplatePhysics() {
    Section("Baseplate na fisica e protecao");
    loblox::Scene scene;
    scene.buildDefaultScene();
    loblox::PhysicsWorld physics;
    physics.attach(scene);

    loblox::Baseplate* baseplate = nullptr;
    for (loblox::Part* part : scene.parts()) {
        if (part->instanceClass() == loblox::InstanceClass::Baseplate) {
            baseplate = static_cast<loblox::Baseplate*>(part);
        }
    }
    CHECK(baseplate != nullptr);
    if (baseplate == nullptr) {
        EndSection();
        return;
    }
    Simulate(physics, scene, 1.0f);
    CHECK_NEAR(baseplate->position().y, -0.5f, 0.001f);

    // Nao pode ser desancorada nem perder colisao: a API protege alem da UI.
    baseplate->setAnchored(false);
    baseplate->setCanCollide(false);
    CHECK(baseplate->anchored());
    CHECK(baseplate->canCollide());
    physics.syncScene(scene);
    Simulate(physics, scene, 1.0f);
    CHECK_NEAR(baseplate->position().y, -0.5f, 0.001f);

    // O editor pode mover a baseplate manualmente (ancorada continua estatica).
    baseplate->setPosition(loblox::V3(0, -2.0f, 0));
    physics.applyChanges(baseplate);
    Simulate(physics, scene, 0.5f);
    CHECK_NEAR(baseplate->position().y, -2.0f, 0.001f);

    // Exclusao protegida mesmo com listener de fisica registrado.
    CHECK(!scene.remove(baseplate));
    CHECK(physics.hasBody(baseplate->id()));
    EndSection();
}

void TestClearObjects() {
    Section("Limpeza de cena (carregar projeto)");
    loblox::Scene scene;
    scene.buildDefaultScene();
    loblox::PhysicsWorld physics;
    physics.attach(scene);
    scene.createPart(loblox::PartShape::Block, loblox::V3(0, 4, 0));
    scene.createPart(loblox::PartShape::Sphere, loblox::V3(6, 4, 0));
    // O sync cria corpos que faltam (partes criadas pela UI/loader).
    physics.simulate(scene, 0.0f);
    CHECK(physics.bodyCount() == 4);

    scene.clearObjects();
    CHECK(scene.parts().empty());
    CHECK(scene.workspace()->children().empty());
    CHECK(physics.bodyCount() == 0);
    CHECK(scene.selection() == nullptr);

    // A cena continua utilizavel.
    loblox::Part* part = scene.createPart(loblox::PartShape::Block, loblox::V3(0, 2, 0));
    CHECK(part != nullptr);
    physics.addPart(part);
    CHECK(physics.bodyCount() == 1);
    EndSection();
}

}  // namespace

int main() {
    std::printf("loblox tests\n============\n");
    TestDefaultScene();
    TestCreateAndRemove();
    TestRemovalListener();
    TestAnchoredAndDynamic();
    TestCanCollide();
    TestTransformSyncAndRaycast();
    TestResizeKeepsSingleBody();
    TestProjectRoundTrip();
    TestProjectValidation();
    TestBaseplatePhysics();
    TestClearObjects();

    std::printf("\n%d checagens, %d falhas\n", gChecks, gFailures);
    if (gFailures != 0) {
        std::printf("RESULTADO: FALHOU\n");
        return 1;
    }
    std::printf("RESULTADO: OK\n");
    return 0;
}
