// Москва: шашки по трассе — X11 + OpenGL 1.x
// Сборка: g++ moscow.cpp -o moscow -lX11 -lGL -lm -std=c++17 -O2
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <set>
#include <algorithm>

static const int   WIN_W   = 1280;
static const int   WIN_H   = 720;
static const float PI      = 3.14159265f;
static const float LANE_W  = 3.5f;
// полосы игрока (правая сторона): x = 2.0, 5.5, 9.0
static const float PLAYER_LANES[3] = { 2.0f, 5.5f, 9.0f };
static const float PLAYER_MIN_X    = 1.3f;
static const float PLAYER_MAX_X    = 9.7f;
// Реальная модель: газ даёт тягу, воздух и качение тормозят
static const float ENGINE_ACCEL = 30.0f;    // м/с² при полном газе
static const float BRAKE_ACCEL  = 60.0f;    // м/с² при тормозе
static const float DRAG_K       = 0.0018f;  // аэродинамика: F = K * v²
static const float ROLL_K       = 0.60f;    // трение качения (постоянное)
static const float SPEED_MAX    = 135.0f;   // аппаратный потолок ~486 км/ч

// ---------- X11/GL ----------
static Display*   dpy = nullptr;
static Window     win;
static GLXContext glctx;
static Atom       wmDelete;
static std::set<int> keysDown;

// ---------- Вспомогательное ----------
static float frand(float lo, float hi) {
    return lo + (hi - lo) * ((float)std::rand() / (float)RAND_MAX);
}
static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// ---------- Шрифт 5x7, ASCII 32..90 ----------
static const unsigned char FONT[][5] = {
{0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},
{0x14,0x7F,0x14,0x7F,0x14},{0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},
{0x36,0x49,0x55,0x22,0x50},{0x00,0x05,0x03,0x00,0x00},{0x00,0x1C,0x22,0x41,0x00},
{0x00,0x41,0x22,0x1C,0x00},{0x14,0x08,0x3E,0x08,0x14},{0x08,0x08,0x3E,0x08,0x08},
{0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},
{0x20,0x10,0x08,0x04,0x02},{0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},
{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},{0x18,0x14,0x12,0x7F,0x10},
{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
{0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},{0x00,0x36,0x36,0x00,0x00},
{0x00,0x56,0x36,0x00,0x00},{0x08,0x14,0x22,0x41,0x00},{0x14,0x14,0x14,0x14,0x14},
{0x00,0x41,0x22,0x14,0x08},{0x02,0x01,0x51,0x09,0x06},{0x32,0x49,0x79,0x41,0x3E},
{0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
{0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
{0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},
{0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
{0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
{0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
{0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},
{0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
{0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},};

// ---------- 3D-примитивы ----------
static void drawBox(float cx, float cy, float cz,
                    float hx, float hy, float hz,
                    float r, float g, float b) {
    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    glNormal3f(0,0, 1);
    glVertex3f(cx-hx, cy-hy, cz+hz); glVertex3f(cx+hx, cy-hy, cz+hz);
    glVertex3f(cx+hx, cy+hy, cz+hz); glVertex3f(cx-hx, cy+hy, cz+hz);
    glNormal3f(0,0,-1);
    glVertex3f(cx+hx, cy-hy, cz-hz); glVertex3f(cx-hx, cy-hy, cz-hz);
    glVertex3f(cx-hx, cy+hy, cz-hz); glVertex3f(cx+hx, cy+hy, cz-hz);
    glNormal3f(-1,0,0);
    glVertex3f(cx-hx, cy-hy, cz-hz); glVertex3f(cx-hx, cy-hy, cz+hz);
    glVertex3f(cx-hx, cy+hy, cz+hz); glVertex3f(cx-hx, cy+hy, cz-hz);
    glNormal3f(1,0,0);
    glVertex3f(cx+hx, cy-hy, cz+hz); glVertex3f(cx+hx, cy-hy, cz-hz);
    glVertex3f(cx+hx, cy+hy, cz-hz); glVertex3f(cx+hx, cy+hy, cz+hz);
    glNormal3f(0,1,0);
    glVertex3f(cx-hx, cy+hy, cz+hz); glVertex3f(cx+hx, cy+hy, cz+hz);
    glVertex3f(cx+hx, cy+hy, cz-hz); glVertex3f(cx-hx, cy+hy, cz-hz);
    glNormal3f(0,-1,0);
    glVertex3f(cx-hx, cy-hy, cz-hz); glVertex3f(cx+hx, cy-hy, cz-hz);
    glVertex3f(cx+hx, cy-hy, cz+hz); glVertex3f(cx-hx, cy-hy, cz+hz);
    glEnd();
    }
// ---------- 2D-текст ----------
static void drawChar(char c, float x, float y, float s) {
    if (c < 32 || c > 90) c = '?';
    const unsigned char* g = FONT[c - 32];
    for (int col = 0; col < 5; ++col)
        for (int row = 0; row < 7; ++row)
            if (g[col] & (1 << row)) {
                glBegin(GL_QUADS);
                glVertex2f(x+col*s,       y+row*s);
                glVertex2f(x+(col+1)*s,   y+row*s);
                glVertex2f(x+(col+1)*s,   y+(row+1)*s);
                glVertex2f(x+col*s,       y+(row+1)*s);
                glEnd();
            }
}
static void drawText(const char* s, float x, float y, float sc) {
    while (*s) { drawChar(*s, x, y, sc); x += 6*sc; ++s; }
}
static float textWidth(const char* s, float sc) {
    return (float)strlen(s) * 6.0f * sc;
}

// ---------- Игровое состояние ----------
static float playerX    = 5.5f;
static float playerVX   = 0.0f;
static float speed      = 45.0f;
static float distance   = 0.0f;
static int   lives      = 3;
static bool  gameOver   = false;
static float shake      = 0.0f;
static float timeAlive  = 0.0f;
static float bestDist   = 0.0f;
static int   carsPassed = 0;

struct Car {
    float x, z;
    float vabs;      // абсолютная скорость
    bool  oncoming;  // встречка?
    float r, g, b;
};
struct Building {
    float x, z;
    float w, h, d;
    int   type;      // 0=панелька 1=сталинка 2=Сити 3=Останкино 4=офис
};

static std::vector<Car>      cars;
static std::vector<Building> buildings;
static float carSpawnTimer = 0.0f;

// ---------- Создание объектов ----------
static const float ROAD_INNER = 15.0f;  // от центра дороги до ближайшей стены

static Building makeBuilding(float z) {
    Building b;
    b.z = z;
    bool left = (std::rand() & 1);
    int  type = std::rand() % 5;
    float sideX = left ? -1.0f : 1.0f;

    // сначала размеры — потом позиция
    switch (type) {
        case 0: b.w = frand(40, 80); b.h = frand(15, 25); b.d = frand(12, 20); break;
        case 1: b.w = frand(20, 28); b.h = frand(70, 95); b.d = frand(20, 28); break;
        case 2: b.w = frand(22, 34); b.h = frand(110,180); b.d = frand(22, 34); break;
        case 3: b.w = 6;             b.h = frand(200,260); b.d = 6;            break;
        default:b.w = frand(20, 40); b.h = frand(30, 60); b.d = frand(20, 40); break;
    }

    // x ставим так, чтобы *внутренняя стена* была за ROAD_INNER,
    // плюс случайный "отступ назад" для разнообразия рядов
    float back = frand(2.0f, 30.0f);
    b.x = sideX * (ROAD_INNER + b.w * 0.5f + back);
    b.type = type;
    return b;
}

static const float CAR_PALETTE[][3] = {
    {0.85f,0.10f,0.10f}, {0.10f,0.25f,0.85f}, {0.08f,0.08f,0.10f},
    {0.92f,0.92f,0.90f}, {0.55f,0.55f,0.60f}, {0.90f,0.78f,0.10f},
    {0.10f,0.60f,0.35f}, {0.65f,0.65f,0.70f},
};

static Car makeCar(float z, bool oncoming) {
    Car c;
    c.z = z;
    c.oncoming = oncoming;
    if (oncoming) {
        c.x = -(2.0f + (std::rand()%3) * LANE_W);
        c.vabs = frand(28, 42);
    } else {
        c.x = PLAYER_LANES[std::rand() % 3];
        c.vabs = frand(18, 32);
    }
    int k = std::rand() % (sizeof(CAR_PALETTE)/sizeof(CAR_PALETTE[0]));
    c.r = CAR_PALETTE[k][0];
    c.g = CAR_PALETTE[k][1];
    c.b = CAR_PALETTE[k][2];
    return c;
}

// ---------- Сброс ----------
static void resetGame() {
    playerX = 5.5f; playerVX = 0.0f;
    speed = 15.0f; distance = 0.0f;
    lives = 3; gameOver = false;
    shake = 0.0f; timeAlive = 0.0f; carsPassed = 0;
    cars.clear(); buildings.clear();
    carSpawnTimer = 0.3f;
    // прогрев зданий
    float z = -180.0f;
    while (z > -900.0f) {
        buildings.push_back(makeBuilding(z));
        z -= frand(55, 90);
    }
}

// ---------- Обновление ----------
static void update(float dt) {
    if (gameOver) {
        if (keysDown.count(XK_r)) resetGame();
        shake *= expf(-3.0f * dt);
        return;
    }
    timeAlive += dt;

    // Скорость
    // --- Реальная физика скорости ---
    float accel = 0.0f;

    // Газ
    if (keysDown.count(XK_w) || keysDown.count(XK_Up))
        accel += ENGINE_ACCEL;

    // Тормоз
    if (keysDown.count(XK_s) || keysDown.count(XK_Down))
        accel -= BRAKE_ACCEL;

    // Сопротивление: воздух (v²) + качение (константа)
    float drag = DRAG_K * speed * speed + ROLL_K;
    accel -= drag;

    // Интегрирование
    speed += accel * dt;
    if (speed < 0.0f)        speed = 0.0f;
    if (speed > SPEED_MAX)   speed = SPEED_MAX;
    
    // Руль
    float targetVX = 0.0f;
    if (keysDown.count(XK_a) || keysDown.count(XK_Left))  targetVX -= 9.5f;
    if (keysDown.count(XK_d) || keysDown.count(XK_Right)) targetVX += 9.5f;
    playerVX += (targetVX - playerVX) * (1.0f - expf(-7.0f * dt));
    playerX  += playerVX * dt;
    playerX   = clampf(playerX, PLAYER_MIN_X, PLAYER_MAX_X);

    // --- Машины потока ---
    for (auto& c : cars) {
        // Попутные: если vabs > speed — уезжают вперёд (z уменьшается)
        //           если speed > vabs — ты их догоняешь (z растёт)
        // Встречные: всегда летят навстречу (z += speed + vabs)
        float rel = c.oncoming ? (speed + c.vabs) : (speed - c.vabs);
        c.z += rel * dt;
    }
    // Убрать уехавших
    // Удаляем тех, кто уехал далеко вперёд (z ушёл за -400)
    // и тех, кто уже проехал позади камеры (z > 12)
    cars.erase(std::remove_if(cars.begin(), cars.end(),
        [](const Car& c){ return c.z > 12.0f || c.z < -400.0f; }), cars.end());
    // Спавн
    carSpawnTimer -= dt;
    if (carSpawnTimer <= 0.0f) {
        carSpawnTimer = frand(0.35f, 1.10f);
        bool oncoming = (std::rand() % 100) < 45;
        cars.push_back(makeCar(-320.0f - frand(0, 60), oncoming));
    }

    // --- Здания ---
    for (auto& b : buildings) b.z += speed * dt;
    buildings.erase(std::remove_if(buildings.begin(), buildings.end(),
        [](const Building& b){ return b.z > 80.0f; }), buildings.end());

    // Спавн новых зданий впереди, пока их мало
    float minZ = 0.0f;
    for (auto& b : buildings) if (b.z < minZ) minZ = b.z;
    while (minZ > -900.0f) {
        minZ -= frand(55, 90);
        buildings.push_back(makeBuilding(minZ));
    }

    // --- Столкновения (только с попутными) ---
    for (auto& c : cars) {
        if (c.oncoming) continue;
        if (fabsf(c.x - playerX) < 2.0f && fabsf(c.z) < 4.2f) {
            lives--;
            shake = 1.2f;
            c.z = 100.0f;   // убрать
            if (lives <= 0) {
                gameOver = true;
                if (distance > bestDist) bestDist = distance;
            }
        }
    }

    // --- Тряска ---
    shake *= expf(-3.0f * dt);
    shake += speed * 0.0004f * dt;
}

// ---------- Отрисовка машины ----------
static void drawCar(float x, float z, float r, float g, float b,
                    bool player, bool oncoming) {
    float W = 1.9f, H = 1.35f, D = 4.4f;

    // Низ (колёса)
    float wy = 0.28f;
    drawBox(x - W/2, wy, z - D*0.30f, 0.15f, 0.28f, 0.30f, 0.05f,0.05f,0.05f);
    drawBox(x + W/2, wy, z - D*0.30f, 0.15f, 0.28f, 0.30f, 0.05f,0.05f,0.05f);
    drawBox(x - W/2, wy, z + D*0.30f, 0.15f, 0.28f, 0.30f, 0.05f,0.05f,0.05f);
    drawBox(x + W/2, wy, z + D*0.30f, 0.15f, 0.28f, 0.30f, 0.05f,0.05f,0.05f);

    // Кузов
    drawBox(x, 0.55f, z, W/2, 0.30f, D/2, r, g, b);
    // Крыша / стёкла
    drawBox(x, 1.02f, z, W*0.42f, 0.30f, D*0.28f, 0.10f, 0.12f, 0.18f);

    // Фары / габариты
    float frontZ = (player || !oncoming) ? (z - D/2 + 0.05f) : (z + D/2 - 0.05f);
    float backZ  = (player || !oncoming) ? (z + D/2 - 0.05f) : (z - D/2 + 0.05f);
    drawBox(x - 0.55f, 0.55f, frontZ, 0.20f, 0.13f, 0.05f, 1.0f, 0.95f, 0.75f);
    drawBox(x + 0.55f, 0.55f, frontZ, 0.20f, 0.13f, 0.05f, 1.0f, 0.95f, 0.75f);
    drawBox(x - 0.60f, 0.60f, backZ,  0.22f, 0.12f, 0.05f, 1.0f, 0.10f, 0.10f);
    drawBox(x + 0.60f, 0.60f, backZ,  0.22f, 0.12f, 0.05f, 1.0f, 0.10f, 0.10f);
}

// ---------- Отрисовка здания ----------
static void drawBuilding(const Building& b) {
    switch (b.type) {
    case 0: // панелька
        drawBox(b.x, b.h/2, b.z, b.w/2, b.h/2, b.d/2, 0.86f, 0.84f, 0.78f);
        drawBox(b.x, b.h*0.55f, b.z, b.w/2 + 0.02f, b.h*0.18f, b.d/2 + 0.02f,
                0.32f, 0.40f, 0.52f);
        drawBox(b.x, b.h*0.18f, b.z, b.w/2 + 0.02f, b.h*0.10f, b.d/2 + 0.02f,
                0.32f, 0.40f, 0.52f);
        break;
    case 1: // сталинка
        drawBox(b.x, b.h*0.40f, b.z, b.w/2,       b.h*0.40f, b.d/2,       0.78f,0.71f,0.62f);
        drawBox(b.x, b.h*0.82f, b.z, b.w*0.32f/2, b.h*0.10f, b.d*0.32f/2, 0.78f,0.71f,0.62f);
        drawBox(b.x, b.h*0.97f, b.z, b.w*0.15f/2, b.h*0.06f, b.d*0.15f/2,0.78f,0.71f,0.62f);
        drawBox(b.x, b.h*1.06f, b.z, 0.4f,         b.h*0.05f, 0.4f,       0.75f,0.10f,0.10f);
        break;
    case 2: // Москва-Сити
        drawBox(b.x, b.h*0.45f, b.z, b.w/2,         b.h*0.45f, b.d/2,       0.55f,0.68f,0.85f);
        drawBox(b.x, b.h*0.85f, b.z, b.w*0.42f/2,   b.h*0.15f, b.d*0.42f/2, 0.70f,0.82f,0.95f);
        drawBox(b.x, b.h*1.00f, b.z, b.w*0.10f/2,   b.h*0.05f, b.d*0.10f/2, 0.85f,0.92f,1.00f);
        break;
    case 3: // Останкино
        drawBox(b.x, b.h*0.30f, b.z, 3.0f,        b.h*0.30f, 3.0f,        0.72f,0.72f,0.76f);
        drawBox(b.x, b.h*0.60f, b.z, 1.6f,        b.h*0.30f, 1.6f,        0.72f,0.72f,0.76f);
        drawBox(b.x, b.h*0.82f, b.z, 1.0f,        b.h*0.22f, 1.0f,        0.72f,0.72f,0.76f);
        drawBox(b.x, b.h*0.66f, b.z, 8.0f,        3.5f,      8.0f,        0.62f,0.62f,0.68f);
        drawBox(b.x, b.h*0.99f, b.z, 0.6f,        5.0f,      0.6f,        0.95f,0.25f,0.20f);
        break;
    default: // офис
        drawBox(b.x, b.h/2, b.z, b.w/2, b.h/2, b.d/2, 0.52f, 0.58f, 0.66f);
        drawBox(b.x, b.h*0.90f, b.z, b.w/2+0.02f, b.h*0.04f, b.d/2+0.02f,
                0.92f, 0.82f, 0.42f);
        break;
    }
}

// ---------- Сцена ----------
static void setupPerspective() {
    float fovY = 70.0f + speed * 0.05f;
    float zN = 0.15f, zF = 800.0f;
    float fH = tanf(fovY * 0.5f * PI / 180.0f) * zN;
    float fW = fH * (float)WIN_W / (float)WIN_H;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-fW, fW, -fH, fH, zN, zF);
    glMatrixMode(GL_MODELVIEW);
}

static void render3D() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glShadeModel(GL_SMOOTH);

    // Туман в цвет неба
    GLfloat fogC[4] = { 0.72f, 0.80f, 0.92f, 1.0f };
    glFogfv(GL_FOG_COLOR, fogC);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, 120.0f);
    glFogf(GL_FOG_END,   520.0f);
    glEnable(GL_FOG);

    glClearColor(fogC[0], fogC[1], fogC[2], 1.0f);

    // Камера от третьего лица
    float camY = 4.6f + cosf(timeAlive * 27.0f) * shake * 0.15f;
    float camZ = 8.5f;
    float camX = playerX + sinf(timeAlive * 31.0f) * shake * 0.12f;
    float pitch = -12.0f + speed * 0.04f;

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef(pitch, 1, 0, 0);
    glTranslatef(-camX, -camY, -camZ);

    // Источник света (небо)
    GLfloat lpos[] = { 0.3f, 1.0f, 0.2f, 0.0f };
    GLfloat lamb[] = { 0.55f, 0.55f, 0.58f, 1.0f };
    GLfloat ldif[] = { 0.85f, 0.85f, 0.85f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, lpos);
    glLightfv(GL_LIGHT0, GL_AMBIENT,  lamb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  ldif);

    // Земля
    glDisable(GL_COLOR_MATERIAL);
    glColor3f(0.35f, 0.42f, 0.30f);
    glBegin(GL_QUADS);
    glNormal3f(0,1,0);
    glVertex3f(-400, 0, -800); glVertex3f( 400, 0, -800);
    glVertex3f( 400, 0,  100); glVertex3f(-400, 0,  100);
    glEnd();
    glEnable(GL_COLOR_MATERIAL);

    // Асфальт (вся трасса)
    drawBox(0, -0.05f, -350, 10.5f, 0.05f, 450.0f, 0.24f, 0.24f, 0.26f);

    // Разметка: прерывистая, скроллится по distance
    float dashPeriod = 12.0f, dashLen = 6.0f;
    float scroll = fmodf(distance, dashPeriod);
    float lanesX[2] = { 3.75f, 7.25f };
    glDisable(GL_LIGHTING);
    glColor3f(0.92f, 0.92f, 0.92f);
    for (float zBase = -500.0f - scroll; zBase < 60.0f; zBase += dashPeriod) {
        for (int i = 0; i < 2; ++i) {
            float x = lanesX[i];
            glBegin(GL_QUADS);
            glVertex3f(x-0.08f, 0.02f, zBase);
            glVertex3f(x+0.08f, 0.02f, zBase);
            glVertex3f(x+0.08f, 0.02f, zBase+dashLen);
            glVertex3f(x-0.08f, 0.02f, zBase+dashLen);
            glEnd();
        }
        // сплошные с двух сторон
        glBegin(GL_QUADS);
        glVertex3f( 1.15f, 0.02f, zBase); glVertex3f( 1.30f, 0.02f, zBase);
        glVertex3f( 1.30f, 0.02f, zBase+dashPeriod); glVertex3f( 1.15f, 0.02f, zBase+dashPeriod);
        glVertex3f(10.20f, 0.02f, zBase); glVertex3f(10.35f, 0.02f, zBase);
        glVertex3f(10.35f, 0.02f, zBase+dashPeriod); glVertex3f(10.20f, 0.02f, zBase+dashPeriod);
        glEnd();
    }
    glEnable(GL_LIGHTING);

    // Осевой отбойник
    drawBox(0, 0.55f, -350, 0.28f, 0.55f, 450.0f, 0.80f, 0.80f, 0.82f);
    drawBox(0, 1.15f, -350, 0.12f, 0.10f, 450.0f, 0.60f, 0.60f, 0.62f);
    // Боковые отбойники
    drawBox( 10.7f, 0.55f, -350, 0.20f, 0.55f, 450.0f, 0.80f, 0.80f, 0.82f);
    drawBox(-10.7f, 0.55f, -350, 0.20f, 0.55f, 450.0f, 0.80f, 0.80f, 0.82f);

    // Фонари вдоль дороги
    float lightPeriod = 45.0f;
    float lscroll = fmodf(distance, lightPeriod);
    for (float z = -500.0f - lscroll; z < 60.0f; z += lightPeriod) {
        for (int s = -1; s <= 1; s += 2) {
            float px = s * 11.4f;
            float armX = s * 8.5f;
            drawBox(px, 4.0f, z, 0.15f, 4.0f, 0.15f, 0.20f, 0.20f, 0.22f);
            drawBox((px+armX)/2.0f, 8.0f, z,
                    fabsf(armX-px)/2.0f, 0.10f, 0.10f, 0.20f, 0.20f, 0.22f);
            drawBox(armX, 7.85f, z, 0.5f, 0.15f, 0.3f, 1.0f, 0.95f, 0.70f);
        }
    }

    // Здания
    for (auto& b : buildings) drawBuilding(b);

    // Попутные и встречные машины
    for (auto& c : cars) drawCar(c.x, c.z, c.r, c.g, c.b, false, c.oncoming);

    // Игрок
    drawCar(playerX, 0.0f, 0.95f, 0.30f, 0.05f, true, false);

    glDisable(GL_FOG);
}

static void renderHUD() {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WIN_W, WIN_H, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);

    char buf[96];

    // Скорость в км/ч
    int kmh = (int)(speed * 3.6f);
    glColor3f(1, 1, 1);
    snprintf(buf, sizeof(buf), "SPEED: %d KM/H", kmh);
    drawText(buf, 20, 20, 3.0f);

    // Пройдено метров
    glColor3f(0.95f, 0.95f, 0.95f);
    snprintf(buf, sizeof(buf), "DIST:  %d M", (int)distance);
    drawText(buf, 20, 55, 2.0f);

    // Обгонено
    glColor3f(0.85f, 0.95f, 0.85f);
    snprintf(buf, sizeof(buf), "CARS:  %d", carsPassed);
    drawText(buf, 20, 82, 2.0f);

    // Жизни
    glColor3f(0.95f, 0.2f, 0.2f);
    snprintf(buf, sizeof(buf), "LIVES: %d", lives);
    drawText(buf, WIN_W - (int)textWidth(buf, 3.0f) - 20, 20, 3.0f);

    // Лучший результат
    glColor3f(0.85f, 0.85f, 0.85f);
    snprintf(buf, sizeof(buf), "BEST:  %d M", (int)bestDist);
    drawText(buf, WIN_W - (int)textWidth(buf, 2.0f) - 20, 60, 2.0f);

    // Подсказка
    glColor3f(0.15f, 0.15f, 0.15f);
    drawText("WASD/ARROWS - DRIVE", 20, WIN_H - 40, 2.0f);

    // Game Over
    if (gameOver) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0, 0, 0, 0.65f);
        glBegin(GL_QUADS);
        glVertex2f(0, WIN_H/2 - 130);
        glVertex2f(WIN_W, WIN_H/2 - 130);
        glVertex2f(WIN_W, WIN_H/2 + 130);
        glVertex2f(0, WIN_H/2 + 130);
        glEnd();
        glDisable(GL_BLEND);

        glColor3f(1, 0.15f, 0.15f);
        const char* m1 = "GAME OVER";
        drawText(m1, (WIN_W - textWidth(m1, 6.0f))/2, WIN_H/2 - 90, 6.0f);

        glColor3f(1, 1, 1);
        snprintf(buf, sizeof(buf), "DIST: %d M   CARS: %d", (int)distance, carsPassed);
        drawText(buf, (WIN_W - textWidth(buf, 3.0f))/2, WIN_H/2 - 5, 3.0f);

        glColor3f(0.85f, 0.85f, 0.85f);
        const char* m2 = "PRESS R TO RESTART";
        drawText(m2, (WIN_W - textWidth(m2, 2.5f))/2, WIN_H/2 + 55, 2.5f);
    }
}

// ---------- main ----------
int main() {
    std::srand((unsigned)std::time(nullptr));

    dpy = XOpenDisplay(nullptr);
    if (!dpy) { fprintf(stderr, "Нет X display\n"); return 1; }
    int screen = DefaultScreen(dpy);

    Bool supp;
    XkbSetDetectableAutoRepeat(dpy, True, &supp);

    static int attribs[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
    XVisualInfo* vi = glXChooseVisual(dpy, screen, attribs);
    if (!vi) { fprintf(stderr, "Нет GLX visual\n"); return 1; }

    Colormap cmap = XCreateColormap(dpy,
        RootWindow(dpy, vi->screen), vi->visual, AllocNone);

    XSetWindowAttributes swa = {};
    swa.colormap   = cmap;
    swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                     StructureNotifyMask | FocusChangeMask;

    win = XCreateWindow(dpy, RootWindow(dpy, vi->screen),
                        0, 0, WIN_W, WIN_H, 0,
                        vi->depth, InputOutput, vi->visual,
                        CWColormap | CWEventMask, &swa);
    XStoreName(dpy, win, "Moscow Traffic - Shashki");

    wmDelete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wmDelete, 1);
    XMapWindow(dpy, win);

    glctx = glXCreateContext(dpy, vi, nullptr, GL_TRUE);
    glXMakeCurrent(dpy, win, glctx);
    XFree(vi);

    glViewport(0, 0, WIN_W, WIN_H);
    glEnable(GL_DEPTH_TEST);

    resetGame();

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double last = ts.tv_sec + ts.tv_nsec * 1e-9;

    bool running = true;
    while (running) {
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            switch (ev.type) {
            case ClientMessage:
                if ((Atom)ev.xclient.data.l[0] == wmDelete) running = false;
                break;
            case KeyPress: {
                KeySym ks = XLookupKeysym(&ev.xkey, 0);
                if (ks == XK_Escape) running = false;
                keysDown.insert(ks);
            } break;
            case KeyRelease:
                keysDown.erase(XLookupKeysym(&ev.xkey, 0));
                break;
            }
        }

        clock_gettime(CLOCK_MONOTONIC, &ts);
        double now = ts.tv_sec + ts.tv_nsec * 1e-9;
        float dt = (float)(now - last);
        last = now;
        if (dt > 0.05f) dt = 0.05f;

        update(dt);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        setupPerspective();
        render3D();
        renderHUD();

        glXSwapBuffers(dpy, win);
    }

    glXMakeCurrent(dpy, None, nullptr);
    glXDestroyContext(dpy, glctx);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}

