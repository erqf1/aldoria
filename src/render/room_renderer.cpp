#include "render/room_renderer.h"

#include <algorithm>
#include <cmath>

#include "raymath.h"

namespace aldoria {
namespace {

Color lerpColor(Color a, Color b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    auto l = [t](unsigned char x, unsigned char y) { return (unsigned char)(x + (y - x) * t); };
    return {l(a.r, b.r), l(a.g, b.g), l(a.b, b.b), a.a};
}

Color alpha(Color c, int a) {
    c.a = (unsigned char)std::clamp(a, 0, 255);
    return c;
}

// Kleine Zufallszahl aus der Position (0..1), damit Bäume nicht alle gleich aussehen
float hash01(float x, float z) {
    float s = std::sin(x * 12.9898f + z * 78.233f) * 43758.5453f;
    return s - std::floor(s);
}

// Körper mit dem Material einer Rolle des Themas ("wall", "floor", ...). Gibt es keins, bleibt es einfarbig.
struct Roles {
    const LitRenderer& r;
    void box(const char* role, Vector3 c, Vector3 size, Color color, float yaw = 0.0f) const {
        r.useMaterial(r.role(role));
        r.box(c, size, color, yaw);
        r.clearMaterial();
    }
    void cylinder(const char* role, Vector3 base, float radius, float height, Color color) const {
        r.useMaterial(r.role(role));
        r.cylinder(base, radius, height, color);
        r.clearMaterial();
    }
    void ellipsoid(const char* role, Vector3 c, Vector3 radii, Color color, float yaw = 0.0f) const {
        r.useMaterial(r.role(role));
        r.ellipsoid(c, radii, color, yaw);
        r.clearMaterial();
    }
};

const char* roleForBox(const LevelBox& b) {
    const std::string& t = b.tag;
    if (t == "wall" || t == "wall_low") return "wall";
    if (t == "pillar") return "pillar";
    if (t == "ruin") return "wall";
    if (t == "step") return "floor2";
    if (t == "island" || t == "island_side") return "island";
    if (t.rfind("door", 0) == 0) return "wood";
    if (t.rfind("platform", 0) == 0) return "platform";
    if (t == "anvil" || t == "dais" || t == "dais_step") return "accent";
    if (t.rfind("switch", 0) == 0) return "trim";
    return "accent";
}

void drawDoorFrame(const LitRenderer& r, const LevelDoor& d, const LevelBox& box, float time) {
    Roles m{r};
    bool alongX = box.size.x > box.size.z;  // Wand verläuft entlang X
    float width = alongX ? box.size.x : box.size.z;
    float thick = alongX ? box.size.z : box.size.x;
    Color wood{120, 96, 72, 255};
    float post = 0.55f;
    for (float s : {-1.0f, 1.0f}) {
        Vector3 c = box.center;
        Vector3 sz;
        if (alongX) {
            c.x += s * (width * 0.5f + post * 0.25f);
            sz = {post, box.size.y + 0.3f, thick + 0.4f};
        } else {
            c.z += s * (width * 0.5f + post * 0.25f);
            sz = {thick + 0.4f, box.size.y + 0.3f, post};
        }
        c.y = box.minY() + sz.y * 0.5f;
        m.box("trim", c, sz, wood);
    }
    Vector3 top = box.center;
    top.y = box.maxY() + 0.2f;
    m.box("trim", top, alongX ? Vector3{width + post * 1.6f, 0.55f, thick + 0.4f} : Vector3{thick + 0.4f, 0.55f, width + post * 1.6f}, wood);
    // Schwelle vor der Tür
    Vector3 sill{box.center.x, box.minY() + 0.02f, box.center.z};
    m.box("trim", sill, alongX ? Vector3{width + 0.4f, 0.06f, thick + 2.2f} : Vector3{thick + 2.2f, 0.06f, width + 0.4f}, wood);

    // Ein Zeichen über der Tür verrät ihre Art
    Color gem{240, 200, 80, 255};
    switch (d.kind) {
        case DoorKind::Key: gem = d.keyType == "boss_key" ? Color{190, 90, 230, 255} : Color{240, 200, 80, 255}; break;
        case DoorKind::Switch: gem = Color{110, 180, 255, 255}; break;
        case DoorKind::Arena: gem = Color{240, 90, 80, 255}; break;
        case DoorKind::BossDefeated: gem = Color{190, 90, 230, 255}; break;
        case DoorKind::Sealed: gem = Color{130, 130, 140, 255}; break;
        case DoorKind::Open: gem = Color{130, 220, 150, 255}; break;
    }
    float pulse = 0.5f + 0.5f * std::sin(time * 3.0f + box.center.x);
    Vector3 gemPos{top.x, top.y + 0.5f, top.z};
    r.sphere(gemPos, 0.24f, alpha(gem, 245));
    r.sphere(gemPos, 0.38f + 0.06f * pulse, alpha(gem, 60));
}

void drawPickup(const LitRenderer& r, const RoomPickup& p, float time) {
    Vector3 pos = p.def.position;
    float bob = std::sin(time * 2.6f + pos.x * 1.7f) * 0.12f;
    pos.y += 0.35f + bob;
    const std::string& t = p.def.type;
    float spin = time * 2.2f;

    auto beam = [&](Color c, float height) {
        r.cylinder({pos.x, pos.y - 0.6f, pos.z}, 0.2f, height, alpha(c, 34));
    };

    if (t == "small_key" || t == "boss_key") {
        bool big = t == "boss_key";
        Color c = big ? Color{200, 100, 240, 255} : Color{242, 204, 80, 255};
        float s = big ? 1.5f : 1.0f;
        r.ellipsoid(pos, {0.2f * s, 0.2f * s, 0.07f * s}, c, spin);
        r.box({pos.x, pos.y - 0.28f * s, pos.z}, {0.07f * s, 0.34f * s, 0.07f * s}, c, spin);
        r.box({pos.x, pos.y - 0.38f * s, pos.z}, {0.2f * s, 0.06f * s, 0.07f * s}, c, spin);
        r.sphere(pos, 0.42f * s, alpha(c, 55));
        beam(c, 4.0f);
    } else if (t == "shard") {
        Color c{110, 210, 255, 255};
        r.ellipsoid(pos, {0.14f, 0.26f, 0.14f}, alpha(c, 245), spin);
        r.sphere(pos, 0.32f, alpha(c, 50));
    } else if (t == "heart") {
        Color c{236, 70, 90, 255};
        r.sphere({pos.x - 0.11f * std::cos(spin), pos.y + 0.06f, pos.z + 0.11f * std::sin(spin)}, 0.16f, c);
        r.sphere({pos.x + 0.11f * std::cos(spin), pos.y + 0.06f, pos.z - 0.11f * std::sin(spin)}, 0.16f, c);
        r.ellipsoid({pos.x, pos.y - 0.12f, pos.z}, {0.2f, 0.2f, 0.16f}, c, spin);
        r.sphere(pos, 0.45f, alpha(c, 50));
        beam(c, 3.0f);
    } else if (t == "flask" || t == "potion") {
        Color c{110, 224, 140, 255};
        r.cylinder({pos.x, pos.y - 0.2f, pos.z}, 0.11f, 0.22f, alpha(Color{200, 230, 240, 255}, 220));
        r.sphere({pos.x, pos.y - 0.2f, pos.z}, 0.2f, c);
        r.sphere(pos, 0.4f, alpha(c, 50));
    } else if (t == "item_dash") {
        Color c{120, 200, 255, 255};
        r.sphere(pos, 0.3f, c);
        r.sphere(pos, 0.5f + 0.05f * std::sin(time * 4.0f), alpha(c, 60));
        for (int i = 0; i < 3; i++) {
            float a = spin * 1.5f + (float)i * 2.094f;
            r.sphere({pos.x + std::cos(a) * 0.55f, pos.y + std::sin(a * 2.0f) * 0.1f, pos.z + std::sin(a) * 0.55f}, 0.09f, Color{255, 255, 255, 230});
        }
        beam(c, 6.0f);
    } else if (t == "item_double_jump") {
        Color c{190, 240, 255, 255};
        // Zwei übereinander schwebende Federn
        for (int i = 0; i < 2; i++) {
            float y = pos.y - 0.16f + (float)i * 0.34f;
            r.ellipsoid({pos.x, y, pos.z}, {0.34f - (float)i * 0.08f, 0.09f, 0.2f}, c, spin);
        }
        r.sphere(pos, 0.55f + 0.05f * std::sin(time * 4.0f), alpha(c, 55));
        beam(c, 6.0f);
    } else if (t == "health_drop") {
        Color c{236, 70, 90, 255};
        r.sphere(pos, 0.16f, c);
        r.sphere(pos, 0.3f, alpha(c, 60));
    } else {
        r.sphere(pos, 0.2f, Color{255, 255, 255, 255});
    }
}

void drawCrystal(const LitRenderer& r, const LevelSwitch& s, const LevelBox& box, bool active, float time) {
    Roles m{r};
    Color base{92, 150, 190, 255};
    Color glow = active ? Color{130, 240, 255, 255} : Color{90, 130, 190, 255};
    m.cylinder("trim", {s.position.x, s.position.y, s.position.z}, 0.55f, 0.4f, Color{130, 128, 140, 255});
    m.cylinder("accent", {s.position.x, s.position.y + 0.4f, s.position.z}, 0.36f, 0.14f, Color{150, 148, 160, 255});
    float bob = std::sin(time * 2.0f) * 0.06f;
    Vector3 c{s.position.x, s.position.y + 1.15f + bob, s.position.z};
    r.ellipsoid(c, {0.34f, 0.72f, 0.34f}, alpha(active ? glow : base, 244), time * (active ? 2.0f : 0.6f));
    r.ellipsoid(c, {0.5f, 0.95f, 0.5f}, alpha(glow, active ? 90 : 40), time);
    (void)box;
}

void drawShrine(const LitRenderer& r, Vector3 pos, bool active, float time) {
    Roles m{r};
    m.cylinder("trim", pos, 1.05f, 0.22f, Color{150, 146, 158, 255});
    m.cylinder("accent", {pos.x, pos.y + 0.22f, pos.z}, 0.75f, 0.3f, Color{160, 156, 170, 255});
    m.cylinder("pillar", {pos.x, pos.y + 0.5f, pos.z}, 0.5f, 0.55f, Color{170, 166, 180, 255});
    Color glow = active ? Color{255, 200, 90, 255} : Color{120, 190, 230, 255};
    float bob = std::sin(time * 2.4f) * 0.08f;
    r.ellipsoid({pos.x, pos.y + 1.7f + bob, pos.z}, {0.28f, 0.5f, 0.28f}, alpha(glow, 244), time * 1.4f);
    r.sphere({pos.x, pos.y + 1.7f + bob, pos.z}, 0.75f, alpha(glow, active ? 70 : 40));
    r.cylinder({pos.x, pos.y + 0.05f, pos.z}, 1.7f, 0.02f, alpha(glow, active ? 120 : 60));
}

void drawSpikes(const LitRenderer& r, const LevelHazard& h, const LevelBox& box) {
    Roles m{r};
    m.box("accent", box.center, {box.size.x, std::max(0.1f, box.size.y * 0.5f), box.size.z}, Color{120, 122, 132, 255});
    int nx = std::max(1, (int)std::floor(box.size.x / 0.7f)), nz = std::max(1, (int)std::floor(box.size.z / 0.7f));
    nx = std::min(nx, 10);
    nz = std::min(nz, 10);
    for (int ix = 0; ix < nx; ix++) {
        for (int iz = 0; iz < nz; iz++) {
            float x = box.minX() + (ix + 0.5f) * box.size.x / (float)nx;
            float z = box.minZ() + (iz + 0.5f) * box.size.z / (float)nz;
            r.cylinder({x, box.maxY(), z}, 0.09f, 0.55f, Color{204, 206, 216, 255});
            r.cylinder({x, box.maxY() + 0.4f, z}, 0.045f, 0.22f, Color{230, 90, 80, 255});
        }
    }
    (void)h;
}

// Wandschmuck: Sockelleiste, Strebepfeiler in regelmäßigen Abständen und eine Krone. Bricht die glatte Wandfläche auf.
void drawWallDetail(const LitRenderer& r, const LevelBox& b, const Level& level) {
    Roles m{r};
    bool alongX = b.size.x > b.size.z;
    float len = alongX ? b.size.x : b.size.z;
    float thick = alongX ? b.size.z : b.size.x;
    // nach innen (zur Raummitte) zeigende Richtung
    float inward = alongX ? (b.center.z > 0 ? -1.0f : 1.0f) : (b.center.x > 0 ? -1.0f : 1.0f);
    float base = std::max(b.minY(), level.groundY);
    float top = b.maxY();
    Color trim = lerpColor(b.color, level.wallCap, 0.35f);
    trim = lerpColor(trim, Color{170, 168, 176, 255}, 0.45f);

    auto place = [&](float along, float out, float y, float sAlong, float sOut, float sY, const char* role) {
        Vector3 c, sz;
        if (alongX) {
            c = {b.center.x + along, y, b.center.z + inward * (thick * 0.5f + out)};
            sz = {sAlong, sY, sOut * 2.0f};
        } else {
            c = {b.center.x + inward * (thick * 0.5f + out), y, b.center.z + along};
            sz = {sOut * 2.0f, sY, sAlong};
        }
        m.box(role, c, sz, trim);
    };

    // Sockelleiste
    place(0.0f, 0.06f, base + 0.28f, len, 0.13f, 0.56f, "trim");
    // Strebepfeiler
    int n = (int)std::floor(len / 6.5f);
    for (int i = 0; i < n; i++) {
        float along = ((float)i + 0.5f) / (float)n * len - len * 0.5f;
        float h = top - base - 0.35f;
        place(along, 0.2f, base + h * 0.5f, 0.95f, 0.22f, h, "trim");
        place(along, 0.3f, top - 0.55f, 1.25f, 0.34f, 0.3f, "accent");   // Kapitell
        place(along, 0.3f, base + 0.3f, 1.25f, 0.34f, 0.6f, "accent");   // Fuß
    }
}

}  // namespace

void drawDecor(const LitRenderer& r, const LevelDecor& d, float time) {
    Roles m{r};
    const Vector3 p = d.position;
    const float s = d.scale;
    float shade = hash01(p.x, p.z);

    if (d.kind == "tree") {
        Color trunk{150, 120, 96, 255};
        Color leaf = lerpColor(Color{92, 140, 76, 255}, Color{130, 170, 84, 255}, shade);
        m.cylinder("bark", p, 0.34f * s, 3.4f * s, trunk);
        m.cylinder("bark", p, 0.5f * s, 0.6f * s, trunk);
        m.ellipsoid("foliage", {p.x, p.y + 3.7f * s, p.z}, {1.8f * s, 1.5f * s, 1.8f * s}, leaf, d.yaw);
        m.ellipsoid("foliage", {p.x + 0.6f * s, p.y + 4.7f * s, p.z - 0.2f * s}, {1.3f * s, 1.1f * s, 1.3f * s}, lerpColor(leaf, Color{170, 210, 110, 255}, 0.25f), d.yaw);
        m.ellipsoid("foliage", {p.x - 0.7f * s, p.y + 3.2f * s, p.z + 0.5f * s}, {1.2f * s, 1.0f * s, 1.2f * s}, lerpColor(leaf, Color{60, 100, 60, 255}, 0.3f), d.yaw);
    } else if (d.kind == "pine") {
        Color trunk{140, 112, 92, 255};
        Color leaf = lerpColor(Color{70, 120, 90, 255}, Color{100, 150, 110, 255}, shade);
        m.cylinder("bark", p, 0.3f * s, 2.2f * s, trunk);
        for (int i = 0; i < 4; i++) {
            float k = (float)i / 3.0f;
            m.ellipsoid("foliage", {p.x, p.y + (2.2f + 1.4f * (float)i) * s, p.z}, {(1.8f - 1.0f * k) * s, 0.95f * s, (1.8f - 1.0f * k) * s}, leaf, d.yaw);
        }
    } else if (d.kind == "rock") {
        Color c = lerpColor(Color{150, 148, 156, 255}, Color{190, 186, 190, 255}, shade);
        m.ellipsoid("rock", {p.x, p.y + 0.45f * s, p.z}, {1.0f * s, 0.62f * s, 0.85f * s}, c, d.yaw);
        m.ellipsoid("rock", {p.x + 0.7f * s, p.y + 0.25f * s, p.z + 0.3f * s}, {0.55f * s, 0.38f * s, 0.5f * s}, lerpColor(c, WHITE, 0.08f), d.yaw + 0.7f);
    } else if (d.kind == "bush") {
        Color c = lerpColor(Color{100, 150, 90, 255}, Color{140, 180, 96, 255}, shade);
        m.ellipsoid("foliage", {p.x, p.y + 0.4f * s, p.z}, {0.9f * s, 0.55f * s, 0.9f * s}, c, d.yaw);
        m.ellipsoid("foliage", {p.x + 0.5f * s, p.y + 0.3f * s, p.z + 0.2f * s}, {0.6f * s, 0.4f * s, 0.6f * s}, lerpColor(c, Color{170, 200, 110, 255}, 0.3f), d.yaw);
    } else if (d.kind == "mushroom") {
        r.cylinder(p, 0.09f * s, 0.5f * s, Color{232, 226, 210, 255});
        r.ellipsoid({p.x, p.y + 0.55f * s, p.z}, {0.4f * s, 0.22f * s, 0.4f * s}, Color{206, 66, 60, 255}, d.yaw);
        r.sphere({p.x + 0.15f * s, p.y + 0.72f * s, p.z}, 0.06f * s, Color{250, 240, 230, 255});
    } else if (d.kind == "pillar") {
        Color c{190, 186, 196, 255};
        m.cylinder("pillar", p, 0.6f * s, 4.0f * s, c);
        m.cylinder("trim", {p.x, p.y + 4.0f * s, p.z}, 0.8f * s, 0.3f * s, c);
        m.cylinder("trim", p, 0.8f * s, 0.3f * s, c);
    } else if (d.kind == "torch") {
        m.cylinder("wood", p, 0.08f * s, 1.5f * s, Color{140, 110, 90, 255});
        m.cylinder("trim", {p.x, p.y + 1.35f * s, p.z}, 0.16f * s, 0.16f * s, Color{120, 116, 124, 255});
        float flicker = 0.85f + 0.15f * std::sin(time * 13.0f + p.x * 3.0f) + 0.05f * std::sin(time * 31.0f);
        r.ellipsoid({p.x, p.y + 1.68f * s, p.z}, {0.15f * s * flicker, 0.28f * s * flicker, 0.15f * s * flicker}, Color{255, 170, 60, 244});
        r.sphere({p.x, p.y + 1.68f * s, p.z}, 0.36f * s * flicker, Color{255, 140, 40, 55});
    } else if (d.kind == "crystal_cluster") {
        Color c{120, 200, 230, 255};
        for (int i = 0; i < 4; i++) {
            float a = d.yaw + (float)i * 1.7f;
            r.ellipsoid({p.x + std::cos(a) * 0.35f * s, p.y + 0.5f * s, p.z + std::sin(a) * 0.35f * s}, {0.16f * s, (0.55f + 0.2f * (float)(i % 3)) * s, 0.16f * s}, alpha(c, 244), a);
        }
        r.sphere({p.x, p.y + 0.7f * s, p.z}, 0.7f * s, alpha(c, 30));
    } else if (d.kind == "obsidian") {
        Color c{70, 60, 80, 255};
        for (int i = 0; i < 3; i++) {
            float a = d.yaw + (float)i * 2.1f;
            float tall = (1.1f + 0.5f * (float)i) * s;
            m.ellipsoid("obsidian", {p.x + std::cos(a) * 0.4f * s, p.y + tall * 0.9f, p.z + std::sin(a) * 0.4f * s}, {0.26f * s, tall, 0.26f * s}, c, a);
        }
        r.sphere({p.x, p.y + 0.2f * s, p.z}, 0.75f * s, Color{255, 110, 40, 36});
    } else if (d.kind == "brazier") {
        m.cylinder("trim", p, 0.16f * s, 0.9f * s, Color{100, 96, 106, 255});
        m.cylinder("trim", {p.x, p.y + 0.9f * s, p.z}, 0.5f * s, 0.22f * s, Color{120, 114, 124, 255});
        float flicker = 0.85f + 0.15f * std::sin(time * 11.0f + p.x * 2.0f) + 0.06f * std::sin(time * 27.0f);
        r.ellipsoid({p.x, p.y + 1.38f * s, p.z}, {0.3f * s * flicker, 0.52f * s * flicker, 0.3f * s * flicker}, Color{255, 150, 40, 244});
        r.sphere({p.x, p.y + 1.38f * s, p.z}, 0.75f * s * flicker, Color{255, 120, 30, 50});
    } else if (d.kind == "cloud") {
        // Schwebende Wolke: mehrere weiche Kugeln, die langsam auf und ab treiben
        float y = p.y + 1.0f + 6.0f * shade + 0.35f * std::sin(time * 0.5f + p.x * 0.3f + p.z * 0.2f);
        Color c = lerpColor(Color{226, 234, 248, 255}, Color{255, 255, 255, 255}, shade);
        r.ellipsoid({p.x, y, p.z}, {2.6f * s, 0.9f * s, 1.7f * s}, c, d.yaw);
        r.ellipsoid({p.x + 1.4f * s, y + 0.3f * s, p.z + 0.3f * s}, {1.7f * s, 0.8f * s, 1.3f * s}, lerpColor(c, WHITE, 0.4f), d.yaw);
        r.ellipsoid({p.x - 1.5f * s, y + 0.1f * s, p.z - 0.2f * s}, {1.5f * s, 0.7f * s, 1.2f * s}, lerpColor(c, Color{200, 214, 236, 255}, 0.3f), d.yaw);
    } else if (d.kind == "spire") {
        // Hoher heller Steinzacken mit leuchtender Spitze
        Color c = lerpColor(Color{200, 204, 216, 255}, Color{230, 232, 240, 255}, shade);
        m.ellipsoid("pillar", {p.x, p.y + 2.6f * s, p.z}, {0.55f * s, 2.8f * s, 0.55f * s}, c, d.yaw);
        m.ellipsoid("pillar", {p.x + 0.5f * s, p.y + 1.4f * s, p.z + 0.2f * s}, {0.4f * s, 1.5f * s, 0.4f * s}, lerpColor(c, Color{140, 150, 180, 255}, 0.3f), d.yaw);
        float pulse = 0.6f + 0.4f * std::sin(time * 2.0f + p.x);
        r.sphere({p.x, p.y + 5.5f * s, p.z}, 0.2f * s, alpha(Color{170, 235, 255, 255}, 244));
        r.sphere({p.x, p.y + 5.5f * s, p.z}, 0.5f * s * pulse, Color{150, 220, 255, 45});
    } else if (d.kind == "lamp") {
        m.cylinder("trim", p, 0.07f * s, 1.6f * s, Color{170, 176, 190, 255});
        float pulse = 0.85f + 0.15f * std::sin(time * 4.0f + p.x * 2.0f);
        r.sphere({p.x, p.y + 1.75f * s, p.z}, 0.2f * s * pulse, alpha(Color{170, 240, 255, 255}, 244));
        r.sphere({p.x, p.y + 1.75f * s, p.z}, 0.5f * s * pulse, Color{140, 215, 255, 50});
    } else if (d.kind == "log") {
        m.box("bark", {p.x, p.y + 0.3f * s, p.z}, {2.2f * s, 0.6f * s, 0.6f * s}, Color{150, 120, 96, 255}, d.yaw);
    }
}

std::vector<PointLight> collectLights(const Level& level, Vector3 focus, float time, size_t maxCount) {
    struct Ranked {
        float d2;
        const LevelLight* l;
    };
    std::vector<Ranked> ranked;
    ranked.reserve(level.lights.size());
    for (const LevelLight& l : level.lights) {
        float dx = l.position.x - focus.x, dy = l.position.y - focus.y, dz = l.position.z - focus.z;
        float d2 = dx * dx + dy * dy + dz * dz;
        float reach = l.radius + 24.0f;  // Lichter weit außerhalb des Bildes lassen wir weg
        if (d2 > reach * reach) continue;
        ranked.push_back({d2, &l});
    }
    std::sort(ranked.begin(), ranked.end(), [](const Ranked& a, const Ranked& b) { return a.d2 < b.d2; });
    std::vector<PointLight> out;
    for (size_t i = 0; i < ranked.size() && i < maxCount; i++) {
        const LevelLight& l = *ranked[i].l;
        float f = 1.0f;
        if (l.flicker > 0.0f) {
            float ph = l.position.x * 3.1f + l.position.z * 1.7f;
            f = 1.0f + l.flicker * 0.16f * (std::sin(time * 13.0f + ph) * 0.6f + std::sin(time * 29.0f + ph * 2.0f) * 0.4f);
        }
        PointLight p;
        p.position = l.position;
        p.color = Vector3Scale(l.color, f);
        p.radius = l.radius;
        out.push_back(p);
    }
    return out;
}

void drawRoom(const LitRenderer& r, const RoomDrawInfo& info) {
    const Level& level = *info.level;
    const float time = info.time;
    Roles m{r};
    const bool sky = level.theme == "himmel";

    // Boden. Außerhalb der Wände liegt eine dunklere Fläche, damit hinter dem Raum kein Loch klafft (nicht im Himmelsturm: dort schwebt der Raum).
    if (level.hasGround) {
        const char* floorRole = (level.type == "rest" || level.type == "treasure" || level.type == "hub") ? "floor2" : "floor";
        m.box(floorRole, {0, level.groundY - 0.5f, 0}, {level.groundSize.x, 1.0f, level.groundSize.y}, level.groundColor);
        if (!sky) m.box("outside", {0, level.groundY - 0.75f, 0}, {600.0f, 1.0f, 600.0f}, lerpColor(level.groundColor, Color{60, 70, 60, 255}, 0.4f));

        // Bodenzeichen je nach Raumart: Ring in Arenen, leuchtender Runenkreis im Bossraum
        if (level.type == "arena") {
            m.cylinder("trim", {0, level.groundY + 0.004f, -1.0f}, 10.5f, 0.02f, Color{170, 166, 176, 255});
            m.cylinder("floor2", {0, level.groundY + 0.008f, -1.0f}, 9.7f, 0.02f, Color{150, 146, 156, 255});
        } else if (level.type == "boss") {
            Color rune = level.theme == "schmiede" ? Color{255, 130, 50, 255} : (level.theme == "himmel" ? Color{130, 220, 255, 255} : Color{255, 210, 110, 255});
            m.cylinder("trim", {0, level.groundY + 0.004f, -6.0f}, 13.5f, 0.02f, Color{170, 166, 176, 255});
            m.cylinder("floor2", {0, level.groundY + 0.008f, -6.0f}, 12.6f, 0.02f, Color{150, 146, 156, 255});
            // Leuchtende Runenperlen auf zwei Ringen
            auto ring = [&](float radius, int count, float phase) {
                for (int i = 0; i < count; i++) {
                    float a = 6.2831853f * (float)i / (float)count + phase;
                    float pulse = 0.75f + 0.25f * std::sin(time * 2.0f + (float)i * 0.7f);
                    r.sphere({std::cos(a) * radius, level.groundY + 0.06f, -6.0f + std::sin(a) * radius}, 0.10f * pulse, alpha(rune, 244));
                }
            };
            ring(12.0f, 56, 0.0f);
            ring(8.3f, 40, 0.08f);
        }
    }

    for (size_t bi = 0; bi < level.boxes.size(); bi++) {
        const LevelBox& b = level.boxes[bi];
        if (!b.visible || b.tag == "hazard") continue;
        // Plattformen bewegen sich: die Textur haftet an der Plattform statt an der Welt, sonst "schwimmt" sie darüber
        const LevelPlatform* moving = nullptr;
        if (b.tag.rfind("platform:", 0) == 0) {
            for (const LevelPlatform& pl : level.platforms) {
                if (pl.boxIndex == (int)bi) moving = &pl;
            }
        }
        if (moving && !moving->path.empty()) {
            Vector3 home = moving->path[0];
            r.setAnchor({b.center.x - home.x, b.center.y - home.y, b.center.z - home.z}, 0.0f);
        }
        m.box(roleForBox(b), b.center, b.size, b.color);
        if (moving) r.clearAnchor();
        if (b.tag == "wall" && b.size.y > 2.0f) {
            // Krone der Wand
            Color trim = lerpColor(lerpColor(b.color, level.wallCap, 0.5f), Color{190, 188, 196, 255}, 0.35f);
            m.box("trim", {b.center.x, b.maxY() + 0.14f, b.center.z}, {b.size.x + 0.28f, 0.3f, b.size.z + 0.28f}, trim);
            drawWallDetail(r, b, level);
        }
    }
    for (const LevelRamp& ramp : level.ramps) {
        r.useMaterial(r.role("accent"));
        r.ramp(ramp);
        r.clearMaterial();
    }

    for (const LevelDoor& d : level.doors) drawDoorFrame(r, d, level.boxes[(size_t)d.boxIndex], time);

    for (size_t i = 0; i < level.switches.size(); i++) {
        const LevelSwitch& s = level.switches[i];
        if (s.kind == SwitchKind::Crystal) {
            bool active = info.room ? info.room->switchActive(i) : false;
            drawCrystal(r, s, level.boxes[(size_t)s.boxIndex], active, time);
        }
    }

    for (const LevelHazard& h : level.hazards) {
        const LevelBox& box = level.boxes[(size_t)h.boxIndex];
        if (h.kind == HazardKind::Spikes) {
            drawSpikes(r, h, box);
        } else {
            // Lava: leuchtende, fließende Textur; eine schwarze Kruste am Rand fasst sie ein
            r.useMaterial(r.material("lava"));
            r.box(box.center, box.size, Color{255, 255, 255, 255});
            r.clearMaterial();
        }
    }

    if (info.room) {
        for (const RoomPickup& p : info.room->pickups()) {
            if (!p.collected) drawPickup(r, p, time);
        }
    }
    if (level.checkpoint) drawShrine(r, *level.checkpoint, info.checkpointActive, time);

    for (const LevelDecor& d : level.decor) drawDecor(r, d, time);
}

}  // namespace aldoria
