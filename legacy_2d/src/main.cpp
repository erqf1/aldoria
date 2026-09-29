// Legende von Aldoria - kleines RPG in C++ mit raylib
// Karte erkunden, Gegner berühren -> rundenbasierter Kampf. Alles wird ohne Bilddateien gezeichnet.

#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr int TILE = 32;
constexpr int MAP_W = 25;
constexpr int MAP_H = 19;
constexpr int SCREEN_W = MAP_W * TILE;  // 800
constexpr int SCREEN_H = MAP_H * TILE;  // 608
constexpr float HERO_SIZE = 20.0f;
constexpr float ENEMY_SIZE = 22.0f;
constexpr float HERO_SPEED = 125.0f;

std::mt19937 rng{std::random_device{}()};
int rnd(int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); }
bool chance(int percent) { return rnd(1, 100) <= percent; }

// ---------------------------------------------------------------- Zeichenhelfer

Color rgb(int r, int g, int b, int a = 255) {
    return Color{(unsigned char)std::clamp(r, 0, 255), (unsigned char)std::clamp(g, 0, 255),
                 (unsigned char)std::clamp(b, 0, 255), (unsigned char)std::clamp(a, 0, 255)};
}
Vector2 V(float x, float y) { return Vector2{x, y}; }
void rect(float x, float y, float w, float h, Color c) { DrawRectangleRec(Rectangle{x, y, w, h}, c); }
void circ(float x, float y, float r, Color c) { DrawCircleV(V(x, y), r, c); }
void ell(float x, float y, float rx, float ry, Color c) { DrawEllipse((int)x, (int)y, rx, ry, c); }
// raylib verlangt eine bestimmte Windungsrichtung, daher beide Richtungen zeichnen
void tri(Vector2 a, Vector2 b, Vector2 c, Color col) {
    DrawTriangle(a, b, c, col);
    DrawTriangle(a, c, b, col);
}
unsigned hash2(int x, int y) {
    unsigned h = (unsigned)x * 73856093u ^ (unsigned)y * 19349663u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return h;
}
void drawCentered(const std::string& text, int y, int size, Color c) {
    DrawText(text.c_str(), (SCREEN_W - MeasureText(text.c_str(), size)) / 2, y, size, c);
}
void drawShadowText(const std::string& text, int x, int y, int size, Color c) {
    DrawText(text.c_str(), x + 2, y + 2, size, rgb(0, 0, 0, 180));
    DrawText(text.c_str(), x, y, size, c);
}
// Text mit Zeilenumbruch, gibt die Zeilenzahl zurück
int drawWrapped(const std::string& text, int x, int y, int maxW, int size, Color c) {
    std::vector<std::string> words;
    std::string cur;
    for (char ch : text) {
        if (ch == ' ') {
            if (!cur.empty()) words.push_back(cur);
            cur.clear();
        } else {
            cur += ch;
        }
    }
    if (!cur.empty()) words.push_back(cur);

    std::string line;
    int lines = 0;
    auto flush = [&]() {
        DrawText(line.c_str(), x, y + lines * (size + 8), size, c);
        lines++;
    };
    for (const auto& w : words) {
        std::string test = line.empty() ? w : line + " " + w;
        if (MeasureText(test.c_str(), size) > maxW && !line.empty()) {
            flush();
            line = w;
        } else {
            line = test;
        }
    }
    if (!line.empty()) flush();
    return lines;
}
void drawPanel(float x, float y, float w, float h) {
    rect(x, y, w, h, rgb(22, 26, 46, 235));
    DrawRectangleLinesEx(Rectangle{x, y, w, h}, 3, rgb(210, 200, 150));
    DrawRectangleLinesEx(Rectangle{x + 3, y + 3, w - 6, h - 6}, 1, rgb(90, 90, 130));
}
void drawBar(float x, float y, float w, float h, int value, int maxv, Color fill) {
    rect(x, y, w, h, rgb(20, 20, 30));
    float f = maxv > 0 ? std::clamp((float)value / (float)maxv, 0.0f, 1.0f) : 0.0f;
    rect(x, y, w * f, h, fill);
    DrawRectangleLinesEx(Rectangle{x, y, w, h}, 1, rgb(0, 0, 0, 200));
    std::string label = std::to_string(value) + "/" + std::to_string(maxv);
    DrawText(label.c_str(), (int)(x + 5), (int)(y + (h - 10) / 2), 10, WHITE);
}

// ---------------------------------------------------------------- Figuren zeichnen

void drawPerson(float cx, float cy, float s, Color body, Color hair, bool sword, bool flash) {
    auto C = [&](Color c) { return flash ? rgb(255, 255, 255) : c; };
    ell(cx, cy + 15 * s, 9 * s, 3.5f * s, rgb(0, 0, 0, 65));
    rect(cx - 6 * s, cy + 8 * s, 4.5f * s, 7 * s, C(rgb(60, 50, 40)));
    rect(cx + 1.5f * s, cy + 8 * s, 4.5f * s, 7 * s, C(rgb(60, 50, 40)));
    rect(cx - 7 * s, cy - 3 * s, 14 * s, 12 * s, C(body));
    rect(cx - 7 * s, cy + 5 * s, 14 * s, 2 * s, C(rgb(90, 60, 30)));  // Gürtel
    circ(cx, cy - 9 * s, 7 * s, C(rgb(240, 200, 160)));
    rect(cx - 7 * s, cy - 16 * s, 14 * s, 5 * s, C(hair));
    if (!flash) {
        circ(cx - 2.5f * s, cy - 8.5f * s, 1.1f * s, rgb(30, 30, 40));
        circ(cx + 2.5f * s, cy - 8.5f * s, 1.1f * s, rgb(30, 30, 40));
    }
    if (sword) {
        rect(cx + 9 * s, cy - 8 * s, 2.6f * s, 17 * s, C(rgb(220, 225, 235)));
        rect(cx + 7 * s, cy + 6 * s, 6.6f * s, 2 * s, C(rgb(230, 190, 60)));
    }
}

void drawEnemySprite(int kind, float cx, float cy, float s, float t, bool flash, bool faceLeft) {
    auto C = [&](Color c) { return flash ? rgb(255, 255, 255) : c; };
    float f = faceLeft ? -1.0f : 1.0f;
    switch (kind) {
        case 0: {  // Schleim
            float sq = std::sin(t * 4.0f) * 0.08f;
            float rx = 15 * s * (1 + sq), ry = 10 * s * (1 - sq);
            float by = cy + 15 * s - ry;
            ell(cx, cy + 15 * s, 15 * s, 3.5f * s, rgb(0, 0, 0, 65));
            ell(cx, by, rx, ry, C(rgb(80, 200, 90)));
            ell(cx - 5 * s, by - 4 * s, 5 * s, 3 * s, C(rgb(160, 240, 160)));
            if (!flash) {
                circ(cx - 5 * s, by - 1 * s, 2.6f * s, WHITE);
                circ(cx + 5 * s, by - 1 * s, 2.6f * s, WHITE);
                circ(cx - 5 * s + f * 0.9f * s, by - 1 * s, 1.2f * s, rgb(20, 20, 30));
                circ(cx + 5 * s + f * 0.9f * s, by - 1 * s, 1.2f * s, rgb(20, 20, 30));
            }
            break;
        }
        case 1: {  // Wolf
            Color grey = C(rgb(122, 124, 136)), dark = C(rgb(84, 86, 98)), light = C(rgb(170, 172, 182));
            ell(cx, cy + 15 * s, 17 * s, 3.5f * s, rgb(0, 0, 0, 65));
            rect(cx - 11 * s, cy + 6 * s, 3.5f * s, 9 * s, dark);
            rect(cx - 6 * s, cy + 6 * s, 3.5f * s, 9 * s, dark);
            rect(cx + 4 * s, cy + 6 * s, 3.5f * s, 9 * s, dark);
            rect(cx + 9 * s, cy + 6 * s, 3.5f * s, 9 * s, dark);
            tri(V(cx - f * 13 * s, cy), V(cx - f * 25 * s, cy - 9 * s), V(cx - f * 14 * s, cy + 5 * s), dark);
            ell(cx, cy + 2 * s, 15 * s, 8 * s, grey);
            circ(cx + f * 13 * s, cy - 4 * s, 7 * s, grey);
            ell(cx + f * 19 * s, cy - 2 * s, 5 * s, 3 * s, light);
            tri(V(cx + f * 9 * s, cy - 9 * s), V(cx + f * 11 * s, cy - 18 * s), V(cx + f * 14 * s, cy - 9 * s), dark);
            tri(V(cx + f * 13 * s, cy - 9 * s), V(cx + f * 16 * s, cy - 17 * s), V(cx + f * 18 * s, cy - 8 * s), dark);
            if (!flash) {
                circ(cx + f * 23 * s, cy - 3 * s, 1.4f * s, rgb(20, 20, 20));
                circ(cx + f * 14 * s, cy - 5 * s, 1.5f * s, rgb(230, 40, 40));
            }
            break;
        }
        case 2: {  // Skelett
            Color bone = C(rgb(232, 230, 214)), shade = C(rgb(190, 188, 170));
            ell(cx, cy + 15 * s, 10 * s, 3.5f * s, rgb(0, 0, 0, 65));
            rect(cx - 5 * s, cy + 6 * s, 3 * s, 9 * s, bone);
            rect(cx + 2 * s, cy + 6 * s, 3 * s, 9 * s, bone);
            rect(cx - 6 * s, cy + 3.5f * s, 12 * s, 3 * s, shade);
            rect(cx - 1 * s, cy - 6 * s, 2 * s, 10 * s, bone);
            for (int i = 0; i < 3; i++) rect(cx - 7 * s, cy - 6 * s + i * 3.6f * s, 14 * s, 2.2f * s, bone);
            rect(cx - 10 * s, cy - 5 * s, 3 * s, 12 * s, bone);
            rect(cx + 7 * s, cy - 5 * s, 3 * s, 12 * s, bone);
            circ(cx, cy - 13 * s, 7 * s, bone);
            rect(cx - 4 * s, cy - 9 * s, 8 * s, 3 * s, bone);
            if (!flash) {
                circ(cx - 2.8f * s, cy - 13 * s, 1.9f * s, rgb(20, 20, 30));
                circ(cx + 2.8f * s, cy - 13 * s, 1.9f * s, rgb(20, 20, 30));
                circ(cx - 2.8f * s, cy - 13 * s, 0.7f * s, rgb(230, 60, 60));
                circ(cx + 2.8f * s, cy - 13 * s, 0.7f * s, rgb(230, 60, 60));
            }
            rect(cx + f * 11 * s - 1 * s, cy - 16 * s, 2.4f * s, 22 * s, C(rgb(150, 155, 170)));
            break;
        }
        default: {  // Drache
            Color red = C(rgb(178, 44, 44)), dark = C(rgb(120, 28, 34)), belly = C(rgb(236, 196, 96));
            ell(cx, cy + 15 * s, 27 * s, 4 * s, rgb(0, 0, 0, 65));
            tri(V(cx - f * 20 * s, cy), V(cx - f * 40 * s, cy + 12 * s), V(cx - f * 18 * s, cy + 12 * s), dark);
            tri(V(cx - f * 2 * s, cy - 4 * s), V(cx - f * 14 * s, cy - 34 * s), V(cx + f * 8 * s, cy - 6 * s), dark);
            rect(cx - 13 * s, cy + 8 * s, 6 * s, 8 * s, dark);
            rect(cx + 5 * s, cy + 8 * s, 6 * s, 8 * s, dark);
            ell(cx, cy + 2 * s, 22 * s, 14 * s, red);
            ell(cx + f * 2 * s, cy + 6 * s, 15 * s, 8 * s, belly);
            tri(V(cx + f * 4 * s, cy - 4 * s), V(cx + f * 8 * s, cy - 40 * s), V(cx + f * 18 * s, cy - 8 * s), red);
            for (int i = 0; i < 4; i++) {
                float sx = cx - f * (14 - i * 7) * s;
                tri(V(sx - 3 * s, cy - 11 * s), V(sx, cy - 18 * s), V(sx + 3 * s, cy - 11 * s), dark);
            }
            circ(cx + f * 21 * s, cy - 10 * s, 9 * s, red);
            ell(cx + f * 29 * s, cy - 8 * s, 7 * s, 4.5f * s, C(rgb(200, 66, 60)));
            tri(V(cx + f * 15 * s, cy - 16 * s), V(cx + f * 12 * s, cy - 27 * s), V(cx + f * 20 * s, cy - 18 * s), belly);
            if (!flash) {
                circ(cx + f * 23 * s, cy - 13 * s, 2.4f * s, rgb(255, 230, 60));
                circ(cx + f * 23.6f * s, cy - 13 * s, 1.0f * s, rgb(20, 10, 10));
                circ(cx + f * 34 * s, cy - 9.5f * s, 1.0f * s, rgb(40, 10, 10));
            }
            break;
        }
    }
}

// ---------------------------------------------------------------- Welt

enum class Tile { Grass, Flower, Tree, Water, Path, House, Bridge };

struct EnemyKind {
    const char* name;
    int hp, atk, xp;
    float battleScale, worldScale, speed;
};
constexpr EnemyKind KINDS[] = {
    {"Schleim", 28, 6, 14, 3.6f, 0.80f, 32.0f},
    {"Wolf", 44, 10, 26, 3.2f, 0.72f, 58.0f},
    {"Skelett", 62, 13, 42, 3.5f, 0.80f, 42.0f},
    {"Drache", 170, 18, 300, 2.9f, 0.62f, 0.0f},
};

struct SpawnDef {
    int x, y, kind;
};
const SpawnDef ENEMY_SPAWNS[] = {
    {7, 14, 0}, {9, 3, 0}, {4, 15, 0},  // Schleime westlich des Flusses
    {15, 4, 1}, {16, 14, 1}, {18, 12, 1},  // Wölfe im Wald
    {19, 6, 2}, {19, 15, 2},              // Skelette
    {22, 2, 3},                           // der Drache
};

struct World {
    Tile tiles[MAP_H][MAP_W];

    Tile at(int x, int y) const {
        if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) return Tile::Tree;
        return tiles[y][x];
    }
    bool solid(int x, int y) const {
        Tile t = at(x, y);
        return t == Tile::Tree || t == Tile::Water || t == Tile::House;
    }
    bool blocked(float x, float y, float w, float h) const {
        int x0 = (int)std::floor(x / TILE), x1 = (int)std::floor((x + w - 0.01f) / TILE);
        int y0 = (int)std::floor(y / TILE), y1 = (int)std::floor((y + h - 0.01f) / TILE);
        for (int ty = y0; ty <= y1; ty++)
            for (int tx = x0; tx <= x1; tx++)
                if (solid(tx, ty)) return true;
        return false;
    }
};

World buildWorld() {
    World w;
    std::mt19937 r(1337);  // feste Startzahl -> immer dieselbe Karte
    auto roll = [&](int percent) { return (int)(r() % 100) < percent; };

    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++) w.tiles[y][x] = roll(9) ? Tile::Flower : Tile::Grass;

    for (int x = 0; x < MAP_W; x++) w.tiles[0][x] = w.tiles[MAP_H - 1][x] = Tile::Tree;
    for (int y = 0; y < MAP_H; y++) w.tiles[y][0] = w.tiles[y][MAP_W - 1] = Tile::Tree;

    // Fluss mit Brücke
    for (int y = 1; y < MAP_H - 1; y++) w.tiles[y][11] = w.tiles[y][12] = Tile::Water;
    for (int y = 8; y <= 9; y++) w.tiles[y][11] = w.tiles[y][12] = Tile::Bridge;

    // Weg
    for (int x = 2; x <= 10; x++) w.tiles[9][x] = Tile::Path;
    for (int x = 13; x <= 21; x++) w.tiles[9][x] = Tile::Path;
    for (int y = 1; y <= 9; y++) w.tiles[y][22] = Tile::Path;

    // Haus
    for (int y = 3; y <= 4; y++)
        for (int x = 3; x <= 5; x++) w.tiles[y][x] = Tile::House;

    // Bäume verteilen, aber Wege und Aufenthaltsorte freihalten
    auto keepClear = [&](int x, int y) {
        if (x >= 1 && x <= 8 && y >= 7 && y <= 11) return true;    // Startgebiet
        if (x >= 2 && x <= 7 && y >= 2 && y <= 7) return true;     // Haus + Ältester
        if (x >= 13 && y >= 8 && y <= 10) return true;             // Weg nach Osten
        if (x >= 21 && y <= 10) return true;                       // Weg zum Drachen
        if (x >= 13 && x <= 15 && y >= 10 && y <= 12) return true;  // Heilerin
        for (const auto& s : ENEMY_SPAWNS)
            if (std::abs(s.x - x) <= 1 && std::abs(s.y - y) <= 1) return true;
        return false;
    };
    for (int y = 1; y < MAP_H - 1; y++) {
        for (int x = 1; x < MAP_W - 1; x++) {
            Tile t = w.tiles[y][x];
            if (t != Tile::Grass && t != Tile::Flower) continue;
            if (keepClear(x, y)) continue;
            if (roll(x <= 10 ? 7 : 22)) w.tiles[y][x] = Tile::Tree;
        }
    }
    return w;
}

// ---------------------------------------------------------------- Spielobjekte

struct Hero {
    Vector2 pos{};
    int level = 1, hp = 40, maxHp = 40, mp = 10, maxMp = 10, atk = 8, xp = 0, xpNext = 30;
    float immune = 0;
    bool walking = false;
    bool faceLeft = false;
};

struct Enemy {
    int kind = 0;
    Vector2 pos{}, home{}, dir{};
    float turnTimer = 0;
    bool alive = true;
    float respawn = 0;  // < 0: kommt nie wieder
    bool faceLeft = true;
};

struct Npc {
    int tx, ty;
    std::string name;
    std::vector<std::string> lines;
    bool heals;
    Color robe, hair;
};

struct Dialog {
    std::string name;
    std::vector<std::string> lines;
    size_t idx = 0;
};

struct FloatText {
    Vector2 pos;
    std::string text;
    Color color;
    float life;
};

struct Battle {
    enum class Phase { Intro, Menu, PlayerMsg, EnemyMsg, WinMsg, LoseMsg };
    Phase phase = Phase::Intro;
    int enemyIndex = -1;
    int kind = 0;
    int enemyHp = 0, enemyMaxHp = 0;
    int selected = 0;
    bool fled = false;
    std::deque<std::string> msgs;
    float enemyFlash = 0, heroFlash = 0, shake = 0;
    std::string notice;
    float noticeTimer = 0;
    std::vector<FloatText> floats;
};

enum class State { Title, Overworld, Dialog, Transition, Battle, GameOver, Victory };

bool confirmPressed() { return IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_E); }

// ---------------------------------------------------------------- Spiel

class Game {
public:
    Game() : world(buildWorld()) {
        initNpcs();
        resetHero();
        spawnEnemies();
    }

    void update(float dt) {
        time += dt;
        switch (state) {
            case State::Title:
                if (confirmPressed()) state = State::Overworld;
                break;
            case State::Overworld: updateOverworld(dt); break;
            case State::Dialog:
                if (confirmPressed() && ++dialog.idx >= dialog.lines.size()) state = State::Overworld;
                break;
            case State::Transition:
                transT += dt;
                if (transT >= 0.8f) beginBattle();
                break;
            case State::Battle: updateBattle(dt); break;
            case State::GameOver:
                if (confirmPressed()) respawnHero();
                break;
            case State::Victory:
                if (confirmPressed()) state = State::Overworld;
                break;
        }
    }

    void draw() {
        switch (state) {
            case State::Title: drawTitle(); break;
            case State::Overworld:
            case State::Dialog:
                drawWorld();
                drawHud();
                if (state == State::Dialog) drawDialog();
                break;
            case State::Transition: drawTransition(); break;
            case State::Battle: drawBattle(); break;
            case State::GameOver: drawGameOver(); break;
            case State::Victory:
                drawWorld();
                drawVictory();
                break;
        }
    }

private:
    World world;
    Hero hero;
    std::vector<Enemy> enemies;
    std::vector<Npc> npcs;
    State state = State::Title;
    Battle battle;
    Dialog dialog;
    float time = 0;
    float transT = 0;
    int pendingEnemy = -1;

    // ------------------------------------------------------------ Aufbau

    void initNpcs() {
        npcs.push_back({6, 6, "Ältester Boran",
                        {"Willkommen in Aldoria, junger Held!",
                         "Ein Drache haust im Osten, hinter dem Fluss. Seit er da ist, traut sich niemand mehr aus dem Dorf.",
                         "Sammle Erfahrung im Kampf gegen Schleime und Wölfe. Erst wenn du stark genug bist, stellst du dich dem Drachen.",
                         "Laufen: WASD oder Pfeiltasten. Sprechen: E oder Leertaste."},
                        false, rgb(120, 70, 150), rgb(230, 230, 235)});
        npcs.push_back({14, 11, "Heilerin Lyra",
                        {"Du siehst erschöpft aus. Ruh dich an meinem Feuer aus.",
                         "So, deine Lebenskraft und dein Mana sind wieder voll. Komm jederzeit wieder vorbei!"},
                        true, rgb(230, 230, 240), rgb(210, 150, 60)});
    }

    void resetHero() {
        hero.pos = V(4 * TILE + 6, 9 * TILE + 6);
    }

    void spawnEnemies() {
        enemies.clear();
        for (const auto& s : ENEMY_SPAWNS) {
            world.tiles[s.y][s.x] = Tile::Grass;
            Enemy e;
            e.kind = s.kind;
            e.home = e.pos = V(s.x * TILE + 5, s.y * TILE + 5);
            e.turnTimer = (float)rnd(0, 100) / 100.0f;
            enemies.push_back(e);
        }
    }

    void respawnHero() {
        hero.hp = hero.maxHp;
        hero.mp = hero.maxMp;
        hero.immune = 2.5f;
        resetHero();
        state = State::Overworld;
    }

    // ------------------------------------------------------------ Oberwelt

    void moveWithCollision(Vector2& p, float dx, float dy, float size) {
        if (!world.blocked(p.x + dx, p.y, size, size)) p.x += dx;
        if (!world.blocked(p.x, p.y + dy, size, size)) p.y += dy;
    }

    int nearbyNpc() const {
        Vector2 hc = V(hero.pos.x + HERO_SIZE / 2, hero.pos.y + HERO_SIZE / 2);
        for (size_t i = 0; i < npcs.size(); i++) {
            Vector2 nc = V(npcs[i].tx * TILE + TILE / 2.0f, npcs[i].ty * TILE + TILE / 2.0f);
            if (std::hypot(hc.x - nc.x, hc.y - nc.y) < 50.0f) return (int)i;
        }
        return -1;
    }

    void updateOverworld(float dt) {
        Vector2 d = V(0, 0);
        if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) d.y -= 1;
        if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) d.y += 1;
        if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) d.x -= 1;
        if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) d.x += 1;
        hero.walking = d.x != 0 || d.y != 0;
        if (hero.walking) {
            float len = std::sqrt(d.x * d.x + d.y * d.y);
            float step = HERO_SPEED * dt / len;
            moveWithCollision(hero.pos, d.x * step, d.y * step, HERO_SIZE);
            if (d.x != 0) hero.faceLeft = d.x < 0;
        }
        hero.immune = std::max(0.0f, hero.immune - dt);

        updateEnemies(dt);
        if (state != State::Overworld) return;

        if (IsKeyPressed(KEY_E) || IsKeyPressed(KEY_SPACE)) {
            int n = nearbyNpc();
            if (n >= 0) openDialog(n);
        }
    }

    void openDialog(int npcIndex) {
        const Npc& n = npcs[npcIndex];
        dialog = Dialog{n.name, n.lines, 0};
        if (n.heals) {
            hero.hp = hero.maxHp;
            hero.mp = hero.maxMp;
        }
        if (n.name == "Ältester Boran") {
            if (hero.level >= 5)
                dialog.lines = {"Du bist stark geworden! Vielleicht ist es Zeit für den Drachen im Osten."};
        }
        state = State::Dialog;
    }

    void updateEnemies(float dt) {
        static const Vector2 DIRS[8] = {{1, 0},  {-1, 0}, {0, 1},  {0, -1},
                                        {0.7f, 0.7f}, {-0.7f, 0.7f}, {0.7f, -0.7f}, {-0.7f, -0.7f}};
        Vector2 hc = V(hero.pos.x + HERO_SIZE / 2, hero.pos.y + HERO_SIZE / 2);

        for (size_t i = 0; i < enemies.size(); i++) {
            Enemy& e = enemies[i];
            const EnemyKind& k = KINDS[e.kind];

            if (!e.alive) {
                if (e.respawn > 0) {
                    e.respawn -= dt;
                    float dist = std::hypot(hc.x - e.home.x, hc.y - e.home.y);
                    if (e.respawn <= 0 && dist > 120) {
                        e.alive = true;
                        e.pos = e.home;
                    } else if (e.respawn <= 0) {
                        e.respawn = 1.0f;
                    }
                }
                continue;
            }

            if (k.speed > 0) {
                e.turnTimer -= dt;
                if (e.turnTimer <= 0) {
                    e.turnTimer = 0.8f + (float)rnd(0, 120) / 100.0f;
                    float away = std::hypot(e.pos.x - e.home.x, e.pos.y - e.home.y);
                    if (away > 110) {
                        Vector2 back = V(e.home.x - e.pos.x, e.home.y - e.pos.y);
                        e.dir = V(back.x / away, back.y / away);
                    } else if (chance(35)) {
                        e.dir = V(0, 0);
                    } else {
                        e.dir = DIRS[rnd(0, 7)];
                    }
                }
                moveWithCollision(e.pos, e.dir.x * k.speed * dt, e.dir.y * k.speed * dt, ENEMY_SIZE);
                if (e.dir.x != 0) e.faceLeft = e.dir.x < 0;
            }

            Vector2 ec = V(e.pos.x + ENEMY_SIZE / 2, e.pos.y + ENEMY_SIZE / 2);
            float reach = e.kind == 3 ? 34.0f : 24.0f;
            if (hero.immune <= 0 && std::hypot(hc.x - ec.x, hc.y - ec.y) < reach) {
                pendingEnemy = (int)i;
                transT = 0;
                state = State::Transition;
                return;
            }
        }
    }

    // ------------------------------------------------------------ Kampf

    void beginBattle() {
        const Enemy& e = enemies[pendingEnemy];
        battle = Battle{};
        battle.enemyIndex = pendingEnemy;
        battle.kind = e.kind;
        battle.enemyHp = battle.enemyMaxHp = KINDS[e.kind].hp;
        if (e.kind == 3)
            battle.msgs.push_back("Der Drache breitet seine Flügel aus und brüllt! Das ist der Endkampf!");
        else
            battle.msgs.push_back(std::string("Ein wilder ") + KINDS[e.kind].name + " greift an!");
        battle.phase = Battle::Phase::Intro;
        state = State::Battle;
    }

    void floatText(float x, float y, const std::string& text, Color c) {
        battle.floats.push_back({V(x, y), text, c, 1.0f});
    }

    const Vector2 HERO_BATTLE_POS = V(200, 292);
    const Vector2 ENEMY_BATTLE_POS = V(585, 285);

    void updateBattle(float dt) {
        Battle& b = battle;
        b.enemyFlash = std::max(0.0f, b.enemyFlash - dt);
        b.heroFlash = std::max(0.0f, b.heroFlash - dt);
        b.shake = std::max(0.0f, b.shake - dt);
        b.noticeTimer = std::max(0.0f, b.noticeTimer - dt);
        for (auto& f : b.floats) {
            f.life -= dt;
            f.pos.y -= 36 * dt;
        }
        b.floats.erase(std::remove_if(b.floats.begin(), b.floats.end(), [](const FloatText& f) { return f.life <= 0; }),
                       b.floats.end());

        if (b.phase == Battle::Phase::Menu) {
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A) || IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))
                b.selected ^= 1;
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W) || IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
                b.selected ^= 2;
            if (confirmPressed()) playerAct(b.selected);
        } else if (confirmPressed()) {
            advanceMessage();
        }
    }

    void playerAct(int choice) {
        Battle& b = battle;
        const EnemyKind& k = KINDS[b.kind];
        b.msgs.clear();
        b.fled = false;

        switch (choice) {
            case 0: {  // Angriff
                int dmg = hero.atk + rnd(-2, 3);
                bool crit = chance(12);
                if (crit) dmg = dmg * 3 / 2;
                b.enemyHp = std::max(0, b.enemyHp - dmg);
                b.enemyFlash = 0.3f;
                floatText(ENEMY_BATTLE_POS.x, ENEMY_BATTLE_POS.y - 80, "-" + std::to_string(dmg), crit ? rgb(255, 210, 60) : WHITE);
                b.msgs.push_back(std::string(crit ? "Kritischer Treffer! " : "Du greifst an! ") + k.name + " erleidet " +
                                 std::to_string(dmg) + " Schaden.");
                break;
            }
            case 1: {  // Feuerball
                if (hero.mp < 5) return notice("Nicht genug MP!");
                hero.mp -= 5;
                int dmg = (int)(hero.atk * 1.8f) + rnd(0, 4);
                b.enemyHp = std::max(0, b.enemyHp - dmg);
                b.enemyFlash = 0.4f;
                floatText(ENEMY_BATTLE_POS.x, ENEMY_BATTLE_POS.y - 80, "-" + std::to_string(dmg), rgb(255, 150, 50));
                b.msgs.push_back("Du schleuderst einen Feuerball! " + std::string(k.name) + " erleidet " + std::to_string(dmg) + " Schaden.");
                break;
            }
            case 2: {  // Heilen
                if (hero.mp < 4) return notice("Nicht genug MP!");
                hero.mp -= 4;
                int heal = std::min(12 + hero.level * 6, hero.maxHp - hero.hp);
                hero.hp += heal;
                floatText(HERO_BATTLE_POS.x, HERO_BATTLE_POS.y - 90, "+" + std::to_string(heal), rgb(110, 230, 120));
                b.msgs.push_back("Du wirkst Heilung und erholst " + std::to_string(heal) + " HP.");
                break;
            }
            default: {  // Fliehen
                if (b.kind == 3) {
                    b.msgs.push_back("Vor dem Drachen gibt es kein Entkommen!");
                } else if (chance(60)) {
                    b.msgs.push_back("Du bist geflohen!");
                    b.fled = true;
                } else {
                    b.msgs.push_back("Die Flucht ist misslungen!");
                }
                break;
            }
        }
        b.phase = Battle::Phase::PlayerMsg;
    }

    void notice(const std::string& text) {
        battle.notice = text;
        battle.noticeTimer = 1.5f;
    }

    void enemyTurn() {
        Battle& b = battle;
        const EnemyKind& k = KINDS[b.kind];
        b.msgs.clear();
        if (chance(8)) {
            b.msgs.push_back(std::string(k.name) + " verfehlt dich!");
        } else {
            bool breath = b.kind == 3 && chance(25);
            int dmg = std::max(1, k.atk + rnd(-2, 2));
            if (breath) dmg = dmg * 8 / 5;
            hero.hp = std::max(0, hero.hp - dmg);
            b.heroFlash = 0.3f;
            b.shake = 0.25f;
            floatText(HERO_BATTLE_POS.x, HERO_BATTLE_POS.y - 90, "-" + std::to_string(dmg), rgb(255, 90, 90));
            if (breath)
                b.msgs.push_back("Der Drache speit Feuer! Du erleidest " + std::to_string(dmg) + " Schaden.");
            else
                b.msgs.push_back(std::string(k.name) + " greift an! Du erleidest " + std::to_string(dmg) + " Schaden.");
        }
        b.phase = Battle::Phase::EnemyMsg;
    }

    void winBattle() {
        Battle& b = battle;
        const EnemyKind& k = KINDS[b.kind];
        b.msgs.clear();
        b.msgs.push_back(std::string(k.name) + " wurde besiegt!");
        b.msgs.push_back("Du erhältst " + std::to_string(k.xp) + " Erfahrungspunkte.");
        hero.xp += k.xp;
        while (hero.xp >= hero.xpNext) {
            hero.xp -= hero.xpNext;
            hero.level++;
            hero.maxHp += 12;
            hero.maxMp += 3;
            hero.atk += 3;
            hero.hp = hero.maxHp;
            hero.mp = hero.maxMp;
            hero.xpNext = hero.xpNext * 3 / 2 + 10;
            b.msgs.push_back("Level-Up! Du bist jetzt Level " + std::to_string(hero.level) + ". HP, MP und Angriff steigen!");
        }
        b.phase = Battle::Phase::WinMsg;
    }

    void endBattle(bool won) {
        Enemy& e = enemies[battle.enemyIndex];
        hero.immune = 1.5f;
        if (won) {
            e.alive = false;
            e.respawn = e.kind == 3 ? -1.0f : 45.0f;
        }
        state = (won && e.kind == 3) ? State::Victory : State::Overworld;
    }

    // Leertaste während eine Nachricht angezeigt wird
    void advanceMessage() {
        Battle& b = battle;
        if (!b.msgs.empty()) b.msgs.pop_front();
        if (!b.msgs.empty()) return;

        switch (b.phase) {
            case Battle::Phase::Intro: b.phase = Battle::Phase::Menu; break;
            case Battle::Phase::PlayerMsg:
                if (b.enemyHp <= 0)
                    winBattle();
                else if (b.fled)
                    endBattle(false);
                else
                    enemyTurn();
                break;
            case Battle::Phase::EnemyMsg:
                if (hero.hp <= 0) {
                    b.msgs.push_back("Du wurdest besiegt...");
                    b.phase = Battle::Phase::LoseMsg;
                } else {
                    b.phase = Battle::Phase::Menu;
                }
                break;
            case Battle::Phase::WinMsg: endBattle(true); break;
            case Battle::Phase::LoseMsg: state = State::GameOver; break;
            default: break;
        }
    }

    // ------------------------------------------------------------ Zeichnen: Oberwelt

    void drawTile(int x, int y) {
        float px = (float)x * TILE, py = (float)y * TILE;
        unsigned h = hash2(x, y);
        int v = (int)(h % 10);
        Color grass = rgb(84 + v, 158 + v, 74 + v / 2);
        switch (world.at(x, y)) {
            case Tile::Grass:
                rect(px, py, TILE, TILE, grass);
                if (h % 5 == 0) rect(px + 6 + (float)((h >> 3) % 18), py + 6 + (float)((h >> 6) % 18), 2, 5, rgb(62, 134, 62));
                break;
            case Tile::Flower: {
                rect(px, py, TILE, TILE, grass);
                Color petals[3] = {rgb(250, 240, 120), rgb(245, 140, 170), rgb(250, 250, 250)};
                Color c = petals[h % 3];
                circ(px + 10, py + 12, 3, c);
                circ(px + 22, py + 21, 3, petals[(h >> 2) % 3]);
                circ(px + 10, py + 12, 1, rgb(220, 150, 30));
                break;
            }
            case Tile::Tree:
                rect(px, py, TILE, TILE, rgb(66, 130, 62));
                rect(px + 13, py + 18, 6, 13, rgb(96, 66, 40));
                circ(px + 16, py + 13, 13, rgb(38, 102, 54));
                circ(px + 10, py + 16, 9, rgb(48, 118, 60));
                circ(px + 22, py + 15, 9, rgb(44, 110, 58));
                circ(px + 12, py + 8, 4, rgb(86, 156, 84));
                break;
            case Tile::Water: {
                rect(px, py, TILE, TILE, rgb(52, 118, 196));
                float w = std::sin(time * 2.0f + (float)x * 1.3f + (float)y * 0.7f);
                rect(px + 4 + w * 3, py + 8, 14, 2, rgb(140, 190, 245, 170));
                rect(px + 12 - w * 3, py + 21, 12, 2, rgb(140, 190, 245, 170));
                break;
            }
            case Tile::Path:
                rect(px, py, TILE, TILE, rgb(202, 170, 114));
                rect(px + 5 + (float)(h % 16), py + 8 + (float)((h >> 4) % 14), 3, 2, rgb(170, 140, 92));
                rect(px + 20 - (float)((h >> 7) % 10), py + 22 - (float)((h >> 3) % 8), 3, 2, rgb(224, 196, 140));
                break;
            case Tile::House: {
                bool top = world.at(x, y - 1) != Tile::House;
                if (top) {
                    rect(px, py, TILE, TILE, rgb(178, 62, 52));
                    for (int i = 0; i < 4; i++) rect(px, py + 3 + (float)i * 8, TILE, 2, rgb(140, 44, 40));
                } else {
                    rect(px, py, TILE, TILE, rgb(228, 208, 170));
                    if (x == 4) {
                        rect(px + 7, py + 5, 18, 27, rgb(102, 66, 38));
                        circ(px + 21, py + 20, 1.6f, rgb(240, 200, 80));
                    } else {
                        rect(px + 8, py + 8, 16, 14, rgb(150, 205, 235));
                        rect(px + 15, py + 8, 2, 14, rgb(110, 76, 44));
                        rect(px + 8, py + 14, 16, 2, rgb(110, 76, 44));
                    }
                }
                break;
            }
            case Tile::Bridge:
                rect(px, py, TILE, TILE, rgb(52, 118, 196));
                for (int i = 0; i < 4; i++) {
                    rect(px + (float)i * 8, py, 7, TILE, rgb(146, 104, 62));
                    rect(px + (float)i * 8, py, 7, 2, rgb(180, 136, 88));
                }
                break;
        }
    }

    void drawWorld() {
        for (int y = 0; y < MAP_H; y++)
            for (int x = 0; x < MAP_W; x++) drawTile(x, y);

        int near = state == State::Overworld ? nearbyNpc() : -1;

        // Figuren von oben nach unten sortiert, damit sie sich sinnvoll überlappen
        struct Item {
            float y;
            int type;  // 0 npc, 1 enemy, 2 hero
            int idx;
        };
        std::vector<Item> items;
        for (size_t i = 0; i < npcs.size(); i++) items.push_back({(float)npcs[i].ty * TILE, 0, (int)i});
        for (size_t i = 0; i < enemies.size(); i++)
            if (enemies[i].alive) items.push_back({enemies[i].pos.y, 1, (int)i});
        items.push_back({hero.pos.y, 2, 0});
        std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.y < b.y; });

        for (const auto& it : items) {
            if (it.type == 0) {
                const Npc& n = npcs[it.idx];
                float cx = n.tx * TILE + TILE / 2.0f, cy = n.ty * TILE + 28 - 15 * 0.9f;
                drawPerson(cx, cy, 0.9f, n.robe, n.hair, false, false);
                if (n.heals) {
                    float bob = std::sin(time * 3) * 2;
                    rect(cx - 1.5f, cy - 26 + bob, 3, 9, rgb(255, 90, 110));
                    rect(cx - 4.5f, cy - 23 + bob, 9, 3, rgb(255, 90, 110));
                }
                if (near == it.idx) {
                    drawShadowText("[E] Sprechen", (int)cx - 42, (int)cy - 46, 10, WHITE);
                }
            } else if (it.type == 1) {
                const Enemy& e = enemies[it.idx];
                const EnemyKind& k = KINDS[e.kind];
                float cx = e.pos.x + ENEMY_SIZE / 2, cy = e.pos.y + ENEMY_SIZE - 15 * k.worldScale;
                drawEnemySprite(e.kind, cx, cy, k.worldScale, time + (float)it.idx, false, e.faceLeft);
            } else {
                bool blink = hero.immune > 0 && std::fmod(time, 0.2f) < 0.1f;
                float bob = hero.walking ? std::sin(time * 14) * 1.2f : 0.0f;
                if (!blink)
                    drawPerson(hero.pos.x + HERO_SIZE / 2, hero.pos.y + HERO_SIZE - 15 * 0.85f + bob, 0.85f,
                               rgb(58, 92, 205), rgb(110, 66, 34), true, false);
            }
        }
    }

    void drawHud() {
        drawPanel(8, 8, 190, 86);
        drawShadowText("Held  Lv " + std::to_string(hero.level), 20, 16, 20, WHITE);
        DrawText("HP", 20, 44, 10, rgb(240, 150, 150));
        drawBar(40, 40, 146, 16, hero.hp, hero.maxHp, rgb(210, 60, 70));
        DrawText("MP", 20, 62, 10, rgb(150, 190, 250));
        drawBar(40, 58, 146, 16, hero.mp, hero.maxMp, rgb(70, 110, 220));
        DrawText("EP", 20, 80, 10, rgb(240, 220, 130));
        drawBar(40, 76, 146, 12, hero.xp, hero.xpNext, rgb(220, 180, 60));
        if (state == State::Overworld)
            drawShadowText("WASD / Pfeile: Laufen     E: Sprechen", SCREEN_W - 350, SCREEN_H - 22, 10, rgb(255, 255, 255));
    }

    void drawDialog() {
        drawPanel(30, SCREEN_H - 170, SCREEN_W - 60, 150);
        drawShadowText(dialog.name, 56, SCREEN_H - 156, 20, rgb(250, 220, 120));
        drawWrapped(dialog.lines[dialog.idx], 56, SCREEN_H - 124, SCREEN_W - 120, 20, WHITE);
        if (std::fmod(time, 1.0f) < 0.6f) DrawText("Weiter: Leertaste", SCREEN_W - 180, SCREEN_H - 40, 10, rgb(200, 200, 220));
    }

    void drawTransition() {
        drawWorld();
        float t = std::clamp(transT / 0.8f, 0.0f, 1.0f);
        if (((int)(transT * 14)) % 2 == 0 && t < 0.6f) rect(0, 0, SCREEN_W, SCREEN_H, rgb(255, 255, 255, 110));
        float bar = SCREEN_H * 0.5f * std::clamp((t - 0.3f) / 0.7f, 0.0f, 1.0f);
        rect(0, 0, SCREEN_W, bar, BLACK);
        rect(0, SCREEN_H - bar, SCREEN_W, bar, BLACK);
    }

    // ------------------------------------------------------------ Zeichnen: Kampf

    void drawBattle() {
        Battle& b = battle;
        const EnemyKind& k = KINDS[b.kind];

        // Kulisse
        DrawRectangleGradientV(0, 0, SCREEN_W, 300, rgb(96, 160, 225), rgb(205, 232, 250));
        tri(V(60, 300), V(190, 190), V(320, 300), rgb(122, 134, 168));
        tri(V(250, 300), V(410, 205), V(570, 300), rgb(142, 154, 186));
        tri(V(470, 300), V(640, 175), V(790, 300), rgb(112, 124, 158));
        DrawRectangleGradientV(0, 300, SCREEN_W, 120, rgb(100, 172, 86), rgb(58, 118, 58));

        float ox = b.shake > 0 ? (float)GetRandomValue(-5, 5) : 0.0f;
        float oy = b.shake > 0 ? (float)GetRandomValue(-3, 3) : 0.0f;

        drawPerson(HERO_BATTLE_POS.x + ox, HERO_BATTLE_POS.y + oy, 3.3f, rgb(58, 92, 205), rgb(110, 66, 34), true,
                   b.heroFlash > 0);
        if (b.phase != Battle::Phase::WinMsg)
            drawEnemySprite(b.kind, ENEMY_BATTLE_POS.x, ENEMY_BATTLE_POS.y + std::sin(time * 2.0f) * 3, k.battleScale, time,
                            b.enemyFlash > 0, true);

        for (const auto& f : b.floats) {
            int a = (int)(255 * std::clamp(f.life * 2.0f, 0.0f, 1.0f));
            DrawText(f.text.c_str(), (int)f.pos.x + 2, (int)f.pos.y + 2, 40, rgb(0, 0, 0, a));
            DrawText(f.text.c_str(), (int)f.pos.x, (int)f.pos.y, 40, rgb(f.color.r, f.color.g, f.color.b, a));
        }

        // Gegner-Info
        drawPanel(16, 14, 260, 62);
        drawShadowText(k.name, 30, 22, 20, WHITE);
        drawBar(30, 48, 232, 16, b.enemyHp, b.enemyMaxHp, rgb(210, 60, 70));

        // Unteres Menü
        drawPanel(16, 432, 470, 160);
        if (b.phase == Battle::Phase::Menu) {
            const char* names[4] = {"Angriff", "Feuerball  (5 MP)", "Heilen  (4 MP)", "Fliehen"};
            int cost[4] = {0, 5, 4, 0};
            for (int i = 0; i < 4; i++) {
                int col = i % 2, row = i / 2;
                float x = 34 + col * 230.0f, y = 450 + row * 46.0f;
                bool sel = b.selected == i;
                bool ok = hero.mp >= cost[i];
                if (sel) rect(x - 6, y - 6, 220, 36, rgb(70, 88, 160));
                Color c = !ok ? rgb(120, 120, 140) : (sel ? WHITE : rgb(210, 210, 225));
                DrawText(names[i], (int)x + (sel ? 14 : 4), (int)y, 20, c);
                if (sel) DrawText(">", (int)x + 2, (int)y, 20, rgb(250, 220, 120));
            }
            if (b.noticeTimer > 0)
                DrawText(b.notice.c_str(), 34, 548, 20, rgb(250, 130, 130));
            else
                DrawText("Pfeile: wählen     Leertaste/Enter: bestätigen", 34, 560, 10, rgb(170, 170, 200));
        } else if (!b.msgs.empty()) {
            drawWrapped(b.msgs.front(), 36, 452, 430, 20, WHITE);
            if (std::fmod(time, 1.0f) < 0.6f) DrawText("Weiter: Leertaste", 34, 566, 10, rgb(200, 200, 220));
        }

        // Helden-Status
        drawPanel(502, 432, 282, 160);
        drawShadowText("Held  Lv " + std::to_string(hero.level), 520, 446, 20, WHITE);
        DrawText("HP", 520, 484, 20, rgb(240, 150, 150));
        drawBar(560, 482, 206, 22, hero.hp, hero.maxHp, rgb(210, 60, 70));
        DrawText("MP", 520, 516, 20, rgb(150, 190, 250));
        drawBar(560, 514, 206, 22, hero.mp, hero.maxMp, rgb(70, 110, 220));
        DrawText("EP", 520, 548, 20, rgb(240, 220, 130));
        drawBar(560, 546, 206, 22, hero.xp, hero.xpNext, rgb(220, 180, 60));
    }

    // ------------------------------------------------------------ Zeichnen: Bildschirme

    void drawTitle() {
        DrawRectangleGradientV(0, 0, SCREEN_W, SCREEN_H, rgb(24, 32, 70), rgb(90, 60, 110));
        for (int i = 0; i < 60; i++) {
            unsigned h = hash2(i, 7);
            float tw = 0.5f + 0.5f * std::sin(time * 2 + (float)i);
            circ((float)(h % SCREEN_W), (float)((h >> 10) % 300), 1.4f, rgb(255, 255, 240, (int)(80 + 150 * tw)));
        }
        // Berge und Drache im Hintergrund
        tri(V(-40, 520), V(180, 330), V(400, 520), rgb(40, 40, 76));
        tri(V(260, 520), V(520, 290), V(820, 520), rgb(34, 34, 66));
        rect(0, 480, SCREEN_W, SCREEN_H - 480, rgb(24, 40, 44));
        drawEnemySprite(3, 610, 350 + std::sin(time) * 6, 3.4f, time, false, true);
        drawPerson(190, 440, 3.6f, rgb(58, 92, 205), rgb(110, 66, 34), true, false);

        drawCentered("LEGENDE VON", 90, 30, rgb(230, 210, 150));
        drawCentered("ALDORIA", 130, 80, rgb(250, 230, 160));
        drawCentered("Ein kleines RPG in C++", 225, 20, rgb(190, 190, 225));
        if (std::fmod(time, 1.2f) < 0.8f) drawCentered("Drücke ENTER oder LEERTASTE", 540, 20, WHITE);
    }

    void drawGameOver() {
        DrawRectangleGradientV(0, 0, SCREEN_W, SCREEN_H, rgb(40, 8, 12), rgb(10, 4, 6));
        drawCentered("DU BIST GEFALLEN", 200, 60, rgb(220, 70, 70));
        drawCentered("Dein Held erwacht im Dorf, geheilt und bereit für einen neuen Versuch.", 300, 20, rgb(220, 200, 200));
        if (std::fmod(time, 1.2f) < 0.8f) drawCentered("ENTER: Weiter", 400, 20, WHITE);
    }

    void drawVictory() {
        rect(0, 0, SCREEN_W, SCREEN_H, rgb(0, 0, 0, 170));
        drawCentered("SIEG!", 150, 80, rgb(250, 220, 100));
        drawCentered("Der Drache ist besiegt - Aldoria ist gerettet!", 260, 20, WHITE);
        drawCentered("Du erreichtest Level " + std::to_string(hero.level), 300, 20, rgb(200, 220, 255));
        drawCentered("ENTER: Weiter erkunden        ESC: Beenden", 400, 20, rgb(230, 230, 240));
    }
};

}  // namespace

int main() {
    InitWindow(SCREEN_W, SCREEN_H, "Legende von Aldoria");
    SetTargetFPS(60);

    Game game;
    while (!WindowShouldClose()) {
        game.update(std::min(GetFrameTime(), 0.05f));
        BeginDrawing();
        ClearBackground(BLACK);
        game.draw();
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
