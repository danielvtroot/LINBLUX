#include "raylib.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

struct Part {
    Vector3 pos, size, velocity;
    Color color;
    bool sphere = false, anchored = true;
};
struct Player { Vector3 pos{0, 1.6f, 0}; float vy=0; bool grounded=false; };

static constexpr float BASE_Y = 0.0f;
static std::vector<Part> parts;
static int selected = -1;
static bool buildMode = true, showHelp = true, physics = true;
static float yaw = 0.0f, pitch = 0.28f;
static Player player;

static Color palette[] = { ORANGE, BLUE, RED, GOLD, PURPLE, SKYBLUE, LIME, GRAY, MAROON };

static BoundingBox partBox(const Part& p) {
    Vector3 h = Vector3Scale(p.size, 0.5f);
    return { Vector3Subtract(p.pos,h), Vector3Add(p.pos,h) };
}
static void addPart(bool sphere) {
    Vector3 forward{ sinf(yaw), 0, cosf(yaw) };
    Vector3 p = Vector3Add(player.pos, Vector3Scale(forward, 4.0f));
    p.y = 1.0f;
    Part part;
    part.pos=p; part.size=sphere ? Vector3{1.4f,1.4f,1.4f} : Vector3{2,2,2};
    part.color=palette[parts.size()% (sizeof(palette)/sizeof(palette[0]))];
    part.sphere=sphere; part.anchored=true;
    parts.push_back(part); selected=(int)parts.size()-1;
}
static void drawPart(const Part& p, bool outline) {
    if (p.sphere) DrawSphere(p.pos, p.size.x*0.5f, p.color);
    else DrawCubeV(p.pos, p.size, p.color);
    if (outline) {
        BoundingBox b=partBox(p);
        DrawBoundingBox(b, YELLOW);
    }
}
static void drawStuds() {
    // Repeating classic-style studs, deliberately simple geometry.
    for (int x=-20;x<=20;x+=2) for (int z=-20;z<=20;z+=2) {
        Vector3 p{(float)x,0.08f,(float)z};
        DrawCube(p,0.34f,0.12f,0.34f, Color{52,157,92,255});
    }
}
static void drawAvatar() {
    Vector3 p=player.pos;
    Color yellow=Color{245,198,35,255}, blue=Color{24,91,170,255}, green=Color{83,143,58,255};
    DrawCube(Vector3{p.x,p.y-0.48f,p.z},0.95f,1.0f,0.48f,blue); // torso
    DrawCube(Vector3{p.x,p.y+0.42f,p.z},0.62f,0.62f,0.62f,yellow); // head
    DrawCube(Vector3{p.x-0.72f,p.y-0.48f,p.z},0.38f,0.95f,0.42f,yellow);
    DrawCube(Vector3{p.x+0.72f,p.y-0.48f,p.z},0.38f,0.95f,0.42f,yellow);
    DrawCube(Vector3{p.x-0.27f,p.y-1.35f,p.z},0.4f,0.8f,0.44f,green);
    DrawCube(Vector3{p.x+0.27f,p.y-1.35f,p.z},0.4f,0.8f,0.44f,green);
}
static void updatePartPhysics(float dt) {
    if (!physics) return;
    for (auto &p:parts) {
        if (p.anchored) continue;
        p.velocity.y -= 22.0f*dt;
        p.pos.y += p.velocity.y*dt;
        float half=p.size.y*0.5f;
        if (p.pos.y-half < 0.14f) { p.pos.y=0.14f+half; p.velocity.y=0; }
        // Basic stable AABB collision against other parts; no expensive solver.
        for (const auto &q:parts) {
            if (&p==&q) continue;
            BoundingBox a=partBox(p), b=partBox(q);
            if (a.max.x>b.min.x && a.min.x<b.max.x && a.max.z>b.min.z && a.min.z<b.max.z && a.min.y<b.max.y && a.max.y>b.min.y) {
                if (p.velocity.y<0 && a.min.y < b.max.y && a.max.y > b.max.y) {
                    p.pos.y=b.max.y+half; p.velocity.y=0;
                }
            }
        }
    }
}
static void selectPart(Camera3D camera) {
    #if RAYLIB_VERSION_MAJOR >= 5
        Ray ray=GetScreenToWorldRay(GetMousePosition(),camera);
    #else
        Ray ray=GetMouseRay(GetMousePosition(),camera);
    #endif
    float closest=1e9f; int found=-1;
    for (int i=0;i<(int)parts.size();i++) {
        BoundingBox b=partBox(parts[i]);
        RayCollision hit=GetRayCollisionBox(ray,b);
        if (hit.hit && hit.distance<closest) { closest=hit.distance; found=i; }
    }
    selected=found;
}
int main() {
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(1024, 640, "Loblox Open-Source 0.2");
    SetTargetFPS(60);
    DisableCursor();
    // Starter playground: anchored platforms and a few loose test objects.
    parts.push_back({{0,0.6f,8},{12,1.2f,4},{0,0,0},Color{143,83,48,255},false,true});
    parts.push_back({{-8,1,2},{3,2,3},{0,0,0},Color{126,75,49,255},false,true});
    parts.push_back({{7,1,5},{3,2,3},{0,0,0},Color{168,168,176,255},false,true});
    parts.push_back({{2,4,7},{1.5f,1.5f,1.5f},{0,0,0},ORANGE,true,false});
    parts.push_back({{4,5,7},{2,2,2},{0,0,0},BLUE,false,false});

    while (!WindowShouldClose()) {
        float dt=std::min(GetFrameTime(),0.033f);
        Vector2 md=GetMouseDelta(); yaw-=md.x*0.0025f; pitch=Clamp(pitch-md.y*0.002f,-0.2f,0.9f);
        if (IsKeyPressed(KEY_H)) showHelp=!showHelp;
        if (IsKeyPressed(KEY_F)) physics=!physics;
        if (IsKeyPressed(KEY_B)) { addPart(false); buildMode=true; }
        if (IsKeyPressed(KEY_N)) { addPart(true); buildMode=true; }
        if (IsKeyPressed(KEY_G) && selected>=0) parts[selected].anchored=!parts[selected].anchored;
        if (IsKeyPressed(KEY_DELETE) && selected>=0) { parts.erase(parts.begin()+selected); selected=-1; }
        if (IsKeyPressed(KEY_ONE) && selected>=0) parts[selected].color=palette[0];
        if (IsKeyPressed(KEY_TWO) && selected>=0) parts[selected].color=palette[1];
        if (IsKeyPressed(KEY_THREE) && selected>=0) parts[selected].color=palette[2];

        Vector3 forward{ sinf(yaw),0,cosf(yaw) }, right{cosf(yaw),0,-sinf(yaw)};
        Vector3 move{0,0,0};
        if (IsKeyDown(KEY_W)) move=Vector3Add(move,forward);
        if (IsKeyDown(KEY_S)) move=Vector3Subtract(move,forward);
        if (IsKeyDown(KEY_D)) move=Vector3Add(move,right);
        if (IsKeyDown(KEY_A)) move=Vector3Subtract(move,right);
        if (Vector3Length(move)>0.01f) { move=Vector3Normalize(move); player.pos=Vector3Add(player.pos,Vector3Scale(move,6.0f*dt)); }
        player.vy-=22.0f*dt; player.pos.y+=player.vy*dt;
        if (player.pos.y<1.6f) { player.pos.y=1.6f; player.vy=0; player.grounded=true; } else player.grounded=false;
        if (IsKeyPressed(KEY_SPACE) && player.grounded) { player.vy=8.5f; player.grounded=false; }
        updatePartPhysics(dt);

        Camera3D camera={0};
        Vector3 target=Vector3Add(player.pos,Vector3{0,0.25f,0});
        float dist=9.0f;
        camera.position=Vector3Add(target,Vector3{-sinf(yaw)*dist*sinf(1.0f-pitch), 3.0f+pitch*3.0f, -cosf(yaw)*dist*sinf(1.0f-pitch)});
        camera.target=target; camera.up={0,1,0}; camera.fovy=65; camera.projection=CAMERA_PERSPECTIVE;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) selectPart(camera);
        if (selected>=0 && IsKeyDown(KEY_LEFT_SHIFT)) { parts[selected].pos=Vector3Add(parts[selected].pos,Vector3Scale(forward,5.0f*dt)); }

        BeginDrawing(); ClearBackground(Color{135,190,235,255});
        BeginMode3D(camera);
        DrawPlane(Vector3{0,-0.03f,0},Vector2{100,100},Color{43,139,78,255});
        drawStuds();
        for (int i=0;i<(int)parts.size();i++) drawPart(parts[i],i==selected);
        drawAvatar();
        DrawGrid(40,1.0f);
        EndMode3D();
        DrawRectangle(10,10,250,22,Fade(BLACK,0.55f)); DrawText("LOBLOX OPEN-SOURCE  |  v0.2",18,15,12,RAYWHITE);
        DrawText(TextFormat("Parts: %i   Physics: %s",(int)parts.size(),physics?"ON":"OFF"),12,38,14,DARKBLUE);
        DrawText("WASD mover  ESPACO pular  Mouse olhar",12,58,13,DARKBLUE);
        DrawText("B bloco  N esfera  clique selecionar  G soltar/fixar",12,76,13,DARKBLUE);
        DrawText("Shift + movimento: mover selecionada  Delete: apagar",12,94,13,DARKBLUE);
        DrawText("1/2/3: cores  F: fisica  H: ajuda",12,112,13,DARKBLUE);
        if (showHelp) {
            DrawRectangle(10,GetScreenHeight()-75,430,62,Fade(BLACK,0.65f));
            DrawText("Protótipo leve para testar construção e gravidade.",20,GetScreenHeight()-65,14,RAYWHITE);
            DrawText("Peças novas começam fixas; pressione G para soltá-las.",20,GetScreenHeight()-43,13,RAYWHITE);
        }
        DrawCircle(GetScreenWidth()/2,GetScreenHeight()/2,2,WHITE);
        EndDrawing();
    }
    CloseWindow(); return 0;
}
