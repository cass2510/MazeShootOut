#include "raylib.h"
#include <vector>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <queue>

static constexpr float PI_F = 3.14159265358979323846f;
inline Vector3 Vector3Add(const Vector3& a, const Vector3& b) { return Vector3{ a.x + b.x, a.y + b.y, a.z + b.z }; }
inline Vector3 Vector3Subtract(const Vector3& a, const Vector3& b) { return Vector3{ a.x - b.x, a.y - b.y, a.z - b.z }; }
inline Vector3 Vector3Scale(const Vector3& v, float s) { return Vector3{ v.x * s, v.y * s, v.z * s }; }
inline float Vector3Length(const Vector3& v) { return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z); }
inline Vector3 Vector3Normalize(const Vector3& v) { float len = Vector3Length(v); return (len > 1e-6f) ? Vector3Scale(v, 1.0f / len) : Vector3{ 0,0,0 }; }
inline float Vector3Distance(const Vector3& a, const Vector3& b) { return Vector3Length(Vector3Subtract(a, b)); }
inline float Deg2Rad(float deg) { return deg * (PI_F / 180.0f); }

#ifndef MATERIAL_MAP_DIFFUSE
#define MATERIAL_MAP_DIFFUSE 0
#endif
static Model  s_CubeModel = { 0 };
static bool   s_CubeModelInit = false;
void DrawCubeTexture(Texture2D texture, Vector3 position,
    float width, float height, float length,
    Color tint)
{
    if (!s_CubeModelInit) {
        Mesh cubeMesh = GenMeshCube(1, 1, 1);
        s_CubeModel = LoadModelFromMesh(cubeMesh);
        s_CubeModel.materialCount = 1;
        s_CubeModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture;
        s_CubeModelInit = true;
    }
    s_CubeModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texture;
    DrawModelEx(s_CubeModel,
        position,
        Vector3{ 0.0f, 1.0f, 0.0f },
        0.0f,
        Vector3{ width, height, length },
        tint);
}
void UnloadCubeTextureModel() {
    if (s_CubeModelInit) {
        UnloadModel(s_CubeModel);
        s_CubeModelInit = false;
    }
}

// 게임 상수
const int   MAP_SIZE = 20;
const float PLAYER_HEIGHT = 1.0f;
const float PLAYER_SPEED = 0.08f;
const float BULLET_SPEED = 0.2f;
const float ENEMY_SPEED_W = 0.04f;
const float ENEMY_SPEED_S = 0.02f;
const int   MAX_HEALTH = 100;

// 맵 데이터 (0=빈,1=벽)
static int mapData[MAP_SIZE][MAP_SIZE];
static Texture2D wallTex;

// 오브젝트 정의
struct Bullet {
    Vector3 pos, dir;
    bool active;
    std::vector<Vector3> trail;
};
struct Enemy { Vector3 pos; int health; bool alive; int type; float fireCooldown; };
struct Item { Vector3 pos; bool picked; };
struct Player { Vector3 pos; float yaw; int health, score; };
struct Impact {
    Vector3 pos;
    double  time; // 생성된 시간
};
std::vector<Impact> impacts;
struct BloodParticle {
    Vector3 pos, vel;
    float life; // 0.0~1.0
};

static float damageEffect = 0.0f;  // 0.0~1.0
static std::vector<Bullet> enemyBullets;
std::vector<BloodParticle> bloods;

// 맵 생성
void GenerateMap() {
    for (int z = 0; z < MAP_SIZE; z++)
        for (int x = 0; x < MAP_SIZE; x++)
            mapData[z][x] = (z == 0 || z == MAP_SIZE - 1 || x == 0 || x == MAP_SIZE - 1)
            ? 1 : (rand() % 100 < 30 ? 1 : 0);
    mapData[1][1] = 0;
    mapData[MAP_SIZE - 2][MAP_SIZE - 2] = 0;
}

bool HasPath(int sx, int sz, int ex, int ez) {
    bool visited[MAP_SIZE][MAP_SIZE] = { false };
    std::queue<std::pair<int, int>> q;
    q.push({ sx, sz });

    while (!q.empty()) {
        // 구조화된 바인딩 대신 명시적 접근
        auto current = q.front();
        int x = current.first;
        int z = current.second;
        q.pop();

        if (x == ex && z == ez) return true;

        const int dx[] = { 1, -1, 0, 0 };
        const int dz[] = { 0, 0, 1, -1 };
        for (int i = 0; i < 4; ++i) {
            int nx = x + dx[i];
            int nz = z + dz[i];
            if (nx >= 0 && nx < MAP_SIZE && nz >= 0 && nz < MAP_SIZE &&
                mapData[nz][nx] == 0 && !visited[nz][nx]) {
                visited[nz][nx] = true;
                q.push({ nx, nz });
            }
        }
    }
    return false;
}

// 3D 맵 그리기
void DrawMap3D() {
    for (int z = 0; z < MAP_SIZE; z++)
        for (int x = 0; x < MAP_SIZE; x++)
            if (mapData[z][x] == 1)
                DrawCubeTexture(wallTex,
                    Vector3{ x + 0.5f, 1.0f, z + 0.5f },  // y=1.0f (중앙), 높이=2.0f
                    1.0f, 2.0f, 1.0f, WHITE);
}

void DrawMiniMap(const Player& player,
    const std::vector<Enemy>& enemies,
    const std::vector<Item>& items,
    const Vector3& exitPos)
{
    const float s = 4.0f * 1.5f; // ← 한 칸 크기를 1.5배로
    const int sz = MAP_SIZE * s; // ← 전체 미니맵 크기도 1.5배
    const int margin = 10;
    // 오른쪽 위에 위치하도록 x좌표(m), y좌표(n) 계산
    const int m = GetScreenWidth() - sz - margin;
    const int n = margin;

    DrawRectangle(m - 1, n - 1, sz + 2, sz + 2, BLACK);
    for (int z = 0; z < MAP_SIZE; z++)
        for (int x = 0; x < MAP_SIZE; x++) {
            Color c = (mapData[z][x] == 1 ? DARKGRAY : Fade(GRAY, 0.2f));
            DrawRectangle(m + x * s, n + z * s, s, s, c);
        }
    for (auto& it : items)
        if (!it.picked)
            DrawRectangle(m + int(it.pos.x * s) + 1, n + int(it.pos.z * s) + 1, s - 2, s - 2, SKYBLUE);
    for (auto& e : enemies)
        if (e.alive) {
            Color c = (e.type == 0 ? YELLOW : MAGENTA);
            DrawRectangle(m + int(e.pos.x * s) + 1, n + int(e.pos.z * s) + 1, s - 2, s - 2, c);
        }
    DrawRectangle(m + int(exitPos.x * s) + 1, n + int(exitPos.z * s) + 1, s - 2, s - 2, GOLD);
    DrawRectangle(m + int(player.pos.x * s) + 1, n + int(player.pos.z * s) + 1, s - 2, s - 2, LIME);
}

//보스가 플레이어를 볼 수 있는지 검사

bool HasLOS(Vector3 a, Vector3 b) {
    Vector3 dir = Vector3Normalize(Vector3Subtract(b, a));
    float dist = Vector3Distance(a, b);
    for (float d = 0.5f; d < dist; d += 0.2f) {
        Vector3 p = Vector3Add(a, Vector3Scale(dir, d));
        int mx = (int)p.x, mz = (int)p.z;
        if (mx < 0 || mx >= MAP_SIZE || mz < 0 || mz >= MAP_SIZE) return false;
        if (mapData[mz][mx] == 1) return false;
    }
    return true;
}

Texture2D bossTex;
Texture2D itemTex;
Texture2D enemyTex;
Texture2D exitTex;

int main() {
    srand((unsigned)time(NULL));
    InitWindow(1024, 768, "MazeShootOut");
    SetTargetFPS(60);

    bossTex = LoadTexture("boss.png");
    itemTex = LoadTexture("aid.png");
    enemyTex = LoadTexture("enemy.png");
    exitTex = LoadTexture("exit.png");

    do {
        GenerateMap();
    } while (!HasPath(1, 1, MAP_SIZE - 2, MAP_SIZE - 2));
    wallTex = LoadTexture("brick.png");

    Player player = { Vector3{1.5f,PLAYER_HEIGHT,1.5f}, 0.0f, MAX_HEALTH, 0 };
    Camera3D camera = {};
    camera.position = player.pos;
    camera.target = Vector3Add(player.pos, Vector3{ 0,0,1 });
    camera.up = Vector3{ 0,1,0 };
    camera.fovy = 70;
    camera.projection = CAMERA_PERSPECTIVE;

    DisableCursor();

    std::vector<Enemy> enemies;
    std::vector<Item>  items;
    std::vector<Bullet> bullets;

    int cntE = 0, cntI = 0;
    int bossCount = 0;
    while (cntE < 10) {
        int x = rand() % (MAP_SIZE - 2) + 1;
        int z = rand() % (MAP_SIZE - 2) + 1;
        if (mapData[z][x] == 0 && (x > 2 || z > 2)) {
            int tp = rand() % 100 < 80 ? 0 : 1; // 80% 일반, 20% 보스
            // 마지막 두 마리는 무조건 보스로 강제
            if (cntE >= 8) tp = 1;
            int hp = tp == 0 ? 20 : 50;
            enemies.push_back({ Vector3{ x + 0.5f,PLAYER_HEIGHT,z + 0.5f }, hp, true, tp });
            if (tp == 1) bossCount++;
            cntE++;
        }
    }
    // 혹시라도 보스가 2마리 미만이면 보스만 추가 생성
    while (bossCount < 2) {
        int x = rand() % (MAP_SIZE - 2) + 1;
        int z = rand() % (MAP_SIZE - 2) + 1;
        if (mapData[z][x] == 0 && (x > 2 || z > 2)) {
            int tp = 1;
            int hp = 50;
            enemies.push_back({ Vector3{ x + 0.5f,PLAYER_HEIGHT,z + 0.5f }, hp, true, tp });
            bossCount++;
        }
    }
    while (cntI < 5) {
        int x = rand() % (MAP_SIZE - 2) + 1, z = rand() % (MAP_SIZE - 2) + 1;
        if (mapData[z][x] == 0) {
            items.push_back({ Vector3{ x + 0.5f,0.5f,z + 0.5f },false });
            cntI++;
        }
    }
    Vector3 exitPos = Vector3{ (float)MAP_SIZE - 1.5f,0.5f,(float)MAP_SIZE - 1.5f };

    bool gameOver = false, gameClear = false;
    double startTime = GetTime();
    double clearTime = 0.0; // 클리어 타임 저장 변수
    float contactTimer = 0.0f;

    while (!WindowShouldClose()) {
        // ① 프레임당 경과 시간 계산 (딱 한 번만)
        float dt = GetFrameTime();
        damageEffect -= dt * 2.0f;
        if (damageEffect < 0.0f) damageEffect = 0.0f;

        // ② 충돌 데미지 타이머 감소
        if (contactTimer > 0.0f) {
            contactTimer -= dt;
            if (contactTimer < 0.0f) contactTimer = 0.0f;
        }

        // ── 플레이어 입력 & 이동 ───────────────────────────────
        if (!gameOver && !gameClear) {
            Vector2 md = GetMouseDelta();
            player.yaw -= md.x * 0.2f;

            Vector3 fw = { sinf(Deg2Rad(player.yaw)), 0, cosf(Deg2Rad(player.yaw)) };
            Vector3 rt = { fw.z, 0, -fw.x }, mv = { 0,0,0 };

            if (IsKeyDown(KEY_W)) mv = Vector3Add(mv, fw);
            if (IsKeyDown(KEY_S)) mv = Vector3Subtract(mv, fw);
            if (IsKeyDown(KEY_A)) mv = Vector3Add(mv, rt);
            if (IsKeyDown(KEY_D)) mv = Vector3Subtract(mv, rt);

            Vector3 np = Vector3Add(player.pos, Vector3Scale(mv, PLAYER_SPEED));
            int nx = (int)np.x, nz = (int)np.z;
            if (mapData[nz][nx] != 1) player.pos = np;

            camera.position = player.pos;
            camera.target = Vector3Add(player.pos, fw);

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                bullets.push_back({ player.pos, fw, true });
        }

        for (auto& e : enemies) {
            if (!e.alive || e.type != 1) continue;  // 보스만
            if (HasLOS(e.pos, player.pos)) {
                e.fireCooldown -= dt;
                if (e.fireCooldown <= 0.0f) {
                    Vector3 dir = Vector3Normalize(Vector3Subtract(player.pos, e.pos));
                    enemyBullets.push_back({ e.pos, dir, true });
                    // 쿨다운 재설정(2~4초)
                    e.fireCooldown = 1.0f + (rand() % 100) / 100.0f * 2.0f;
                }
            }
        }

        // 총알
        for (auto& b : bullets) {
            if (!b.active) continue;
            b.trail.push_back(b.pos); // 이동 전 위치 저장
            b.pos = Vector3Add(b.pos, Vector3Scale(b.dir, BULLET_SPEED));
            // trail 길이 제한 (예: 10개)
            if (b.trail.size() > 10) b.trail.erase(b.trail.begin());
            int bx = (int)b.pos.x, bz = (int)b.pos.z;
            if (bx < 0 || bx >= MAP_SIZE || bz < 0 || bz >= MAP_SIZE || mapData[bz][bx] == 1) {
                b.active = false;
                impacts.push_back({ b.pos, GetTime() }); // ← 임팩트 저장
            }
        }

        // 적 총알
        for (auto& b : enemyBullets) {
            if (!b.active) continue;
            b.trail.push_back(b.pos);
            if (b.trail.size() > 10) b.trail.erase(b.trail.begin());

            // ── Movement ──────────────────────────
            b.pos = Vector3Add(b.pos, Vector3Scale(b.dir, BULLET_SPEED * 0.8f));
            int bx = (int)b.pos.x, bz = (int)b.pos.z;
            if (bx < 0 || bx >= MAP_SIZE || bz < 0 || bz >= MAP_SIZE || mapData[bz][bx] == 1) {
                b.active = false;
                impacts.push_back({ b.pos, GetTime() });
                continue;
            }

            // ── Player hit ─────────────────────────
            if (Vector3Distance(b.pos, player.pos) < 0.6f) {
                b.active = false;
                player.health -= 15;
                impacts.push_back({ b.pos, GetTime() });
                if (player.health <= 0) gameOver = true;
                damageEffect = 1.0f;
            }

            for (int i = 0; i < 12; ++i) {
                float angle = ((float)i / 12.0f) * 2 * PI_F + GetRandomValue(-10, 10) * 0.1f;
                float speed = 0.1f + GetRandomValue(0, 10) * 0.01f;
                Vector3 dir = Vector3Normalize(Vector3{
                    cosf(angle),
                    GetRandomValue(0,10) * 0.02f,
                    sinf(angle)
                    });
                bloods.push_back({ b.pos, Vector3Scale(dir, speed), 1.0f });
            }
        }

        // 적
        for (auto& e : enemies) if (e.alive) {
            Vector3 tp = Vector3Subtract(player.pos, e.pos);
            float d = Vector3Length(tp), sp = e.type == 0 ? ENEMY_SPEED_W : ENEMY_SPEED_S;
            if (d > 0.3f) {
                Vector3 nd = Vector3Scale(Vector3Normalize(tp), sp);
                Vector3 np2 = Vector3Add(e.pos, nd);
                int ex = (int)np2.x, ez = (int)np2.z;
                if (mapData[ez][ex] != 1) e.pos = np2;
            }
            float dist = Vector3Distance(player.pos, e.pos);
            if (dist < 0.7f) {
                if (contactTimer <= 0.0f) {
                    player.health -= 10;       // 10HP 데미지
                    contactTimer = 0.5f;       // 0.5초 재발사 대기
                    if (player.health <= 0) gameOver = true;
                    damageEffect = 1.0f;
                }
            }
        }


        // 충돌
        for (auto& b : bullets) if (b.active)
            for (auto& e : enemies) if (e.alive && Vector3Distance(b.pos, e.pos) < 0.6f) {
                e.health -= 15; b.active = false;
                if (e.health <= 0) { e.alive = false; player.score += 100; }
                for (int i = 0; i < 12; ++i) {
                    float angle = ((float)i / 12.0f) * 2 * PI_F + GetRandomValue(-10, 10) * 0.1f;
                    // y축 분산: -0.2 ~ +0.2
                    float pitch = ((GetRandomValue(0, 100) / 100.0f) - 0.5f) * 0.4f;
                    Vector3 dir = Vector3Normalize(Vector3{
                         cosf(angle),
                         pitch,
                         sinf(angle)
                    });
                    // 속도: 0.05 ~ 0.1
                    float speed = 0.05f + GetRandomValue(0, 10) * 0.005f;
                    bloods.push_back({ b.pos, Vector3Scale(dir, speed), 1.0f });
                }
            }


        //피 효과
        for (auto& b : bloods) {
            b.pos = Vector3Add(b.pos, b.vel);
            b.vel = Vector3Scale(b.vel, 0.92f); // 점점 느려짐
            b.life -= 0.03f;
        }
        bloods.erase(std::remove_if(bloods.begin(), bloods.end(), [](auto& b) {return b.life <= 0.0f;}), bloods.end());

        // 아이템
        for (auto& it : items) if (!it.picked && Vector3Distance(it.pos, player.pos) < 0.7f) {
            it.picked = true;
            player.health = std::min(player.health + 30, MAX_HEALTH);
            player.score += 50;
        }

        // 탈출
        if (!gameOver && !gameClear && Vector3Distance(exitPos, player.pos) < 0.8f) {
            gameClear = true;
            clearTime = GetTime() - startTime; // 클리어 순간의 시간 저장
        }

        // 렌더
        BeginDrawing();
        ClearBackground(BLACK);
        BeginMode3D(camera);
        DrawPlane(Vector3{ MAP_SIZE / 2.0f,0,MAP_SIZE / 2.0f }, Vector2{ (float)MAP_SIZE,(float)MAP_SIZE }, DARKBROWN);
        DrawMap3D();
        for (auto& it : items) if (!it.picked) {
            float itemSize = 0.5f; // 아이템 billboard 크기
            DrawBillboard(camera, itemTex, it.pos, itemSize, WHITE);
        }
        for (auto& e : enemies) if (e.alive) {
            if (e.type == 1) {
                float bossSize = 1.6f;
                Vector3 bossPos = e.pos;
                bossPos.y = bossSize / 2.0f;
                DrawBillboard(camera, bossTex, bossPos, bossSize, WHITE);
            }
            else {
                float enemySize = 1.4f;
                Vector3 enemyPos = e.pos;
                enemyPos.y = enemySize / 2.0f;
                DrawBillboard(camera, enemyTex, enemyPos, enemySize, WHITE);
            }
        }
        for (auto& b : bullets) {
            if (b.active) {
                // 트레일(궤적) 그리기
                for (size_t i = 1; i < b.trail.size(); ++i) {
                    DrawLine3D(b.trail[i - 1], b.trail[i], Fade(YELLOW, 0.4f + 0.6f * i / b.trail.size()));
                }
                // 총알 본체 (Glow 효과)
                DrawSphere(b.pos, 0.05f, YELLOW); // 중심
                DrawSphere(b.pos, 0.08f, Fade(YELLOW, 0.3f)); // Glow
            }
        }

        //적 총알 렌더링
        for (auto& b : enemyBullets) {
            if (!b.active) continue;
            for (size_t i = 1; i < b.trail.size(); ++i) {
                DrawLine3D(b.trail[i - 1], b.trail[i],
                    Fade(RED, 0.4f + 0.6f * (float)i / b.trail.size()));
            }
            DrawSphere(b.pos, 0.05f, RED);
            DrawSphere(b.pos, 0.10f, Fade(RED, 0.3f));
        }

        double now = GetTime();
        for (auto it = impacts.begin(); it != impacts.end(); ) {
            if (now - it->time > 0.2) it = impacts.erase(it); // 0.2초 후 삭제
            else {
                DrawSphere(it->pos, 0.25f, ORANGE); // 임팩트 이펙트
                ++it;
            }
        }

        //피 렌더링
        for (auto& b : bloods)
            DrawSphere(b.pos, 0.02f * b.life, Fade(RED, b.life));

        {
            float exitSize = 1.4f;     // 원하는 크기
            Vector3 ePos = exitPos;
            ePos.y = exitSize * 0.5f;
            DrawBillboard(camera, exitTex, ePos, exitSize, WHITE);
        }
        EndMode3D();

        // HUD
        DrawRectangle(0, 0, 280, 100, Fade(BLACK, 0.5f));
        DrawText(TextFormat("HP: %d/%d", player.health, MAX_HEALTH), 10, 10, 20, LIME);
        DrawText(TextFormat("Score: %d", player.score), 10, 35, 20, WHITE);
        int ac = std::count_if(enemies.begin(), enemies.end(), [](auto& e) {return e.alive;});
        DrawText(TextFormat("Enemies: %d", ac), 10, 60, 20, RED);
        DrawText("ESC to exit", 10, 85, 15, GRAY);
        DrawMiniMap(player, enemies, items, exitPos);
        if (gameOver) {
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.7f));
            DrawText("GAME OVER", 360, 360, 50, RED);
        }
        if (gameClear) {
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.7f));
            DrawText("STAGE CLEAR!", 330, 330, 50, GOLD);
            DrawText(TextFormat("Time: %.1fs", clearTime), 380, 400, 25, WHITE); // 고정된 시간만 사용
        }

        if (damageEffect > 0.0f) {
            // 상단 모서리부터 투명→빨강 그라데이션
            Color c1 = Fade(RED, damageEffect * 0.6f);
            Color c2 = Fade(RED, damageEffect * 0.0f);
            Rectangle full = { 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() };
            DrawRectangleGradientEx(full,
                c1,  // top-left
                c1,  // top-right
                c2,  // bottom-left
                c2   // bottom-right
            );
        }
        EndDrawing();
    }

    UnloadCubeTextureModel();
    CloseWindow();

    UnloadTexture(wallTex);
    UnloadTexture(bossTex);
    UnloadTexture(itemTex);
    UnloadTexture(enemyTex);
    UnloadTexture(exitTex);

    return 0;
}
