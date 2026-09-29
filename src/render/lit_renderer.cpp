#include "render/lit_renderer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/log.h"
#include "raymath.h"
#include "rlgl.h"

namespace aldoria {
namespace {

// Einheitskeil: x und z von -0.5 bis 0.5, Höhe steigt entlang +z von 0 auf 1.
// Jede Fläche hat eigene Eckpunkte (flache Normalen); Dreiecke laufen gegen den Uhrzeigersinn von außen gesehen.
Mesh buildWedgeMesh() {
    struct V { float x, y, z; };
    const V A{-0.5f, 0, -0.5f}, B{0.5f, 0, -0.5f}, C{0.5f, 1, 0.5f}, D{-0.5f, 1, 0.5f};
    const V P{-0.5f, 0, 0.5f}, Q{0.5f, 0, 0.5f};
    struct Tri { V a, b, c; V n; };
    const float s = 0.70710678f;
    const Tri tris[] = {
        {A, D, C, {0, s, -s}}, {A, C, B, {0, s, -s}},   // Schräge
        {P, Q, C, {0, 0, 1}},  {P, C, D, {0, 0, 1}},    // hohe Kante
        {B, C, Q, {1, 0, 0}},                            // rechte Seite
        {A, P, D, {-1, 0, 0}},                           // linke Seite
        {A, B, Q, {0, -1, 0}}, {A, Q, P, {0, -1, 0}},   // Unterseite
    };
    constexpr int kTriCount = 8;

    Mesh mesh{};
    mesh.triangleCount = kTriCount;
    mesh.vertexCount = kTriCount * 3;
    mesh.vertices = (float*)MemAlloc(mesh.vertexCount * 3 * sizeof(float));
    mesh.normals = (float*)MemAlloc(mesh.vertexCount * 3 * sizeof(float));
    mesh.texcoords = (float*)MemAlloc(mesh.vertexCount * 2 * sizeof(float));
    std::memset(mesh.texcoords, 0, mesh.vertexCount * 2 * sizeof(float));

    int i = 0;
    for (const Tri& t : tris) {
        for (const V& v : {t.a, t.b, t.c}) {
            mesh.vertices[i * 3 + 0] = v.x;
            mesh.vertices[i * 3 + 1] = v.y;
            mesh.vertices[i * 3 + 2] = v.z;
            mesh.normals[i * 3 + 0] = t.n.x;
            mesh.normals[i * 3 + 1] = t.n.y;
            mesh.normals[i * 3 + 2] = t.n.z;
            i++;
        }
    }
    UploadMesh(&mesh, false);
    return mesh;
}

constexpr int kMaxLights = 8;

// Kegelstumpf entlang +y: unten Radius 1 bei y=0, oben Radius `ratio` bei y=1, glatte Normalen, ohne Deckel
// (die Enden deckt der Aufrufer mit Kugeln ab)
Mesh buildTaperMesh(float ratio) {
    constexpr int kSlices = 24;
    Mesh mesh{};
    mesh.vertexCount = (kSlices + 1) * 2;
    mesh.triangleCount = kSlices * 2;
    mesh.vertices = (float*)MemAlloc((unsigned)(mesh.vertexCount * 3 * (int)sizeof(float)));
    mesh.normals = (float*)MemAlloc((unsigned)(mesh.vertexCount * 3 * (int)sizeof(float)));
    mesh.texcoords = (float*)MemAlloc((unsigned)(mesh.vertexCount * 2 * (int)sizeof(float)));
    mesh.indices = (unsigned short*)MemAlloc((unsigned)(mesh.triangleCount * 3 * (int)sizeof(unsigned short)));
    std::memset(mesh.texcoords, 0, (size_t)mesh.vertexCount * 2 * sizeof(float));
    for (int i = 0; i <= kSlices; i++) {
        float a = 6.2831853f * (float)i / (float)kSlices;
        float c = std::cos(a), s = std::sin(a);
        float nl = std::sqrt(c * c + (1.0f - ratio) * (1.0f - ratio) + s * s);
        float nx = c / nl, ny = (1.0f - ratio) / nl, nz = s / nl;
        int b = i * 2, t = i * 2 + 1;
        mesh.vertices[b * 3 + 0] = c; mesh.vertices[b * 3 + 1] = 0.0f; mesh.vertices[b * 3 + 2] = s;
        mesh.vertices[t * 3 + 0] = c * ratio; mesh.vertices[t * 3 + 1] = 1.0f; mesh.vertices[t * 3 + 2] = s * ratio;
        for (int v : {b, t}) {
            mesh.normals[v * 3 + 0] = nx; mesh.normals[v * 3 + 1] = ny; mesh.normals[v * 3 + 2] = nz;
        }
    }
    int k = 0;
    for (int i = 0; i < kSlices; i++) {
        unsigned short a = (unsigned short)(i * 2), b = (unsigned short)(i * 2 + 1);
        unsigned short c = (unsigned short)((i + 1) * 2), d = (unsigned short)((i + 1) * 2 + 1);
        mesh.indices[k++] = a; mesh.indices[k++] = b; mesh.indices[k++] = c;
        mesh.indices[k++] = b; mesh.indices[k++] = d; mesh.indices[k++] = c;
    }
    UploadMesh(&mesh, false);
    return mesh;
}

// Netz für den Stoff: vorn und hinten getrennte Eckpunkte (eigene Normalen), Dreiecke einmal angelegt, Eckpunkte jedes Bild neu
Mesh buildClothMesh() {
    const int n = kClothCols * kClothRows;
    Mesh mesh{};
    mesh.vertexCount = n * 2;
    mesh.triangleCount = (kClothCols - 1) * (kClothRows - 1) * 2 * 2;
    mesh.vertices = (float*)MemAlloc((unsigned)(mesh.vertexCount * 3 * (int)sizeof(float)));
    mesh.normals = (float*)MemAlloc((unsigned)(mesh.vertexCount * 3 * (int)sizeof(float)));
    mesh.texcoords = (float*)MemAlloc((unsigned)(mesh.vertexCount * 2 * (int)sizeof(float)));
    mesh.indices = (unsigned short*)MemAlloc((unsigned)(mesh.triangleCount * 3 * (int)sizeof(unsigned short)));
    std::memset(mesh.vertices, 0, (size_t)mesh.vertexCount * 3 * sizeof(float));
    std::memset(mesh.normals, 0, (size_t)mesh.vertexCount * 3 * sizeof(float));
    for (int r = 0; r < kClothRows; r++) {
        for (int c = 0; c < kClothCols; c++) {
            for (int side = 0; side < 2; side++) {
                int v = side * n + r * kClothCols + c;
                mesh.texcoords[v * 2 + 0] = (float)c / (float)(kClothCols - 1);
                mesh.texcoords[v * 2 + 1] = (float)r / (float)(kClothRows - 1);
            }
        }
    }
    int t = 0;
    for (int r = 0; r < kClothRows - 1; r++) {
        for (int c = 0; c < kClothCols - 1; c++) {
            unsigned short a = (unsigned short)(r * kClothCols + c), b = (unsigned short)(a + 1);
            unsigned short d = (unsigned short)(a + kClothCols), e = (unsigned short)(d + 1);
            // Vorderseite (Normale nach außen, vom Träger weg)
            mesh.indices[t++] = a; mesh.indices[t++] = d; mesh.indices[t++] = b;
            mesh.indices[t++] = b; mesh.indices[t++] = d; mesh.indices[t++] = e;
            // Rückseite mit umgekehrter Windung
            unsigned short off = (unsigned short)n;
            mesh.indices[t++] = (unsigned short)(a + off); mesh.indices[t++] = (unsigned short)(b + off); mesh.indices[t++] = (unsigned short)(d + off);
            mesh.indices[t++] = (unsigned short)(b + off); mesh.indices[t++] = (unsigned short)(e + off); mesh.indices[t++] = (unsigned short)(d + off);
        }
    }
    UploadMesh(&mesh, true);
    return mesh;
}

}  // namespace

bool LitRenderer::init(const std::string& dataDir) {
    // Größerer Nahbereich als der raylib-Standard: weniger Flimmern auf entfernten Flächen
    rlSetClipPlanes(0.1, 500.0);

    cube_ = GenMeshCube(1.0f, 1.0f, 1.0f);
    sphere_ = GenMeshSphere(1.0f, 24, 48);
    cylinder_ = GenMeshCylinder(1.0f, 1.0f, 40);
    wedge_ = buildWedgeMesh();
    for (int i = 0; i < kTaperMeshes; i++) taper_[i] = buildTaperMesh((float)i / (float)(kTaperMeshes - 1));
    cloth_ = buildClothMesh();
    material_ = LoadMaterialDefault();
    whiteTex_ = material_.maps[MATERIAL_MAP_ALBEDO].texture;
    depthMaterial_ = LoadMaterialDefault();

    const std::string shaderDir = dataDir + "/shaders";
    std::string vs = shaderDir + "/lit.vs", fs = shaderDir + "/lit.fs", dfs = shaderDir + "/depth.fs", sfs = shaderDir + "/sky.fs";
    shader_ = LoadShader(vs.c_str(), fs.c_str());
    lit_ = shader_.id > 0 && shader_.id != rlGetShaderIdDefault();
    if (lit_) {
        shader_.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader_, "matModel");
        shader_.locs[SHADER_LOC_MATRIX_NORMAL] = GetShaderLocation(shader_, "matNormal");
        shader_.locs[SHADER_LOC_MAP_NORMAL] = GetShaderLocation(shader_, "texNormal");
        shader_.locs[SHADER_LOC_MAP_HEIGHT] = GetShaderLocation(shader_, "shadowMap");
        locViewPos_ = GetShaderLocation(shader_, "viewPos");
        locSunDir_ = GetShaderLocation(shader_, "sunDir");
        locSunColor_ = GetShaderLocation(shader_, "sunColor");
        locSkyAmbient_ = GetShaderLocation(shader_, "skyAmbient");
        locGroundAmbient_ = GetShaderLocation(shader_, "groundAmbient");
        locFogColor_ = GetShaderLocation(shader_, "fogColor");
        locFogStart_ = GetShaderLocation(shader_, "fogStart");
        locFogEnd_ = GetShaderLocation(shader_, "fogEnd");
        locUseTex_ = GetShaderLocation(shader_, "useTex");
        locUseAnchor_ = GetShaderLocation(shader_, "useAnchor");
        locTexAnchor_ = GetShaderLocation(shader_, "texAnchor");
        locMatA_ = GetShaderLocation(shader_, "matA");
        locMatB_ = GetShaderLocation(shader_, "matB");
        locMatC_ = GetShaderLocation(shader_, "matC");
        locHasNormal_ = GetShaderLocation(shader_, "hasNormalMap");
        locTime_ = GetShaderLocation(shader_, "time");
        locGroundY_ = GetShaderLocation(shader_, "groundY");
        locAmbientBoost_ = GetShaderLocation(shader_, "ambientBoost");
        locSunBoost_ = GetShaderLocation(shader_, "sunBoost");
        locLightVP_ = GetShaderLocation(shader_, "lightVP");
        locShadowTexel_ = GetShaderLocation(shader_, "shadowTexel");
        locShadowOn_ = GetShaderLocation(shader_, "shadowOn");
        locLightCount_ = GetShaderLocation(shader_, "lightCount");
        locLightPos_ = GetShaderLocation(shader_, "lightPos[0]");
        locLightCol_ = GetShaderLocation(shader_, "lightCol[0]");
        locDirectOut_ = GetShaderLocation(shader_, "directOut");
        locExposure_ = GetShaderLocation(shader_, "exposure");
        material_.shader = shader_;

        depthShader_ = LoadShader(vs.c_str(), dfs.c_str());
        if (depthShader_.id > 0 && depthShader_.id != rlGetShaderIdDefault()) {
            depthShader_.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(depthShader_, "matModel");
            depthMaterial_.shader = depthShader_;
        } else {
            Log::warn(LogCategory::Loading, "Schatten-Shader fehlt, es gibt keine Schatten");
            depthShader_ = Shader{};
        }
        skyShader_ = LoadShader(nullptr, sfs.c_str());
        if (skyShader_.id > 0 && skyShader_.id != rlGetShaderIdDefault()) {
            skyRes_ = GetShaderLocation(skyShader_, "resolution");
            skyF_ = GetShaderLocation(skyShader_, "camF");
            skyR_ = GetShaderLocation(skyShader_, "camR");
            skyU_ = GetShaderLocation(skyShader_, "camU");
            skyTan_ = GetShaderLocation(skyShader_, "tanHalf");
            skyAspect_ = GetShaderLocation(skyShader_, "aspect");
            skyTop_ = GetShaderLocation(skyShader_, "skyTop");
            skyHorizon_ = GetShaderLocation(skyShader_, "horizon");
            skySunDir_ = GetShaderLocation(skyShader_, "sunDir");
            skySunColor_ = GetShaderLocation(skyShader_, "sunColor");
            skyTime_ = GetShaderLocation(skyShader_, "time");
            skyCloud_ = GetShaderLocation(skyShader_, "cloudAmount");
            skyGlow_ = GetShaderLocation(skyShader_, "sunGlow");
            skyDirect_ = GetShaderLocation(skyShader_, "directOut");
            skyExposure_ = GetShaderLocation(skyShader_, "exposure");
        } else {
            Log::warn(LogCategory::Loading, "Himmel-Shader fehlt, der Himmel bleibt einfarbig");
            skyShader_ = Shader{};
        }
        post_.init(shaderDir);
        lib_.load(dataDir);
        Log::info(LogCategory::Loading, "Licht-Shader geladen aus {}", shaderDir);
    } else {
        Log::error(LogCategory::Loading, "Licht-Shader konnte nicht geladen werden ({}), zeichne flach", shaderDir);
    }
    ready_ = true;
    return lit_;
}

void LitRenderer::shutdown() {
    if (!ready_) return;
    destroyShadowTarget();
    post_.shutdown();
    lib_.unloadAll();
    UnloadMesh(cube_);
    UnloadMesh(sphere_);
    UnloadMesh(cylinder_);
    UnloadMesh(wedge_);
    for (Mesh& m : taper_) UnloadMesh(m);
    UnloadMesh(cloth_);
    if (lit_) {
        UnloadShader(shader_);
        if (depthShader_.id > 0) UnloadShader(depthShader_);
        if (skyShader_.id > 0) UnloadShader(skyShader_);
    }
    // Die Materialien teilen sich Standard-Textur und -Shader mit raylib und werden bewusst nicht einzeln freigegeben
    ready_ = false;
}

void LitRenderer::setTheme(const std::string& theme) {
    theme_ = theme;
    // Texturen des Themas vorladen (beim Raumwechsel liegt das hinter der Überblendung)
    for (MaterialId id : lib_.themeMaterials(theme)) lib_.acquire(id);
}

void LitRenderer::setQuality(int quality) { quality_ = std::clamp(quality, 0, 2); }

bool LitRenderer::ensureShadowTarget(int size) {
    if (shadowRT_.id != 0 && shadowSize_ == size) return true;
    destroyShadowTarget();
    RenderTexture2D target{};
    target.id = rlLoadFramebuffer();
    if (target.id == 0) return false;
    target.texture.width = size;
    target.texture.height = size;
    rlEnableFramebuffer(target.id);
    target.depth.id = rlLoadTextureDepth(size, size, false);
    target.depth.width = size;
    target.depth.height = size;
    target.depth.format = 19;
    target.depth.mipmaps = 1;
    rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
    bool ok = rlFramebufferComplete(target.id);
    rlDisableFramebuffer();
    if (!ok) {
        Log::error(LogCategory::Loading, "Schattenkarte {}x{} unvollständig, Schatten aus", size, size);
        UnloadRenderTexture(target);
        return false;
    }
    shadowRT_ = target;
    shadowSize_ = size;
    Log::info(LogCategory::Loading, "Schattenkarte {}x{} bereit", size, size);
    return true;
}

void LitRenderer::destroyShadowTarget() {
    if (shadowRT_.id != 0) UnloadRenderTexture(shadowRT_);
    shadowRT_ = RenderTexture2D{};
    shadowSize_ = 0;
    shadowValid_ = false;
}

void LitRenderer::uploadFrameUniforms(const Camera3D& camera, float time, bool directOut) const {
    if (!lit_) return;
    Vector3 sun = Vector3Normalize(env_.sunDirection);
    SetShaderValue(shader_, locSunDir_, &sun, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader_, locSunColor_, &env_.sunColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader_, locSkyAmbient_, &env_.skyAmbient, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader_, locGroundAmbient_, &env_.groundAmbient, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader_, locFogColor_, &env_.fogColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader_, locFogStart_, &env_.fogStart, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader_, locFogEnd_, &env_.fogEnd, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader_, locViewPos_, &camera.position, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader_, locTime_, &time, SHADER_UNIFORM_FLOAT);
    float groundY = 0.0f;
    SetShaderValue(shader_, locGroundY_, &groundY, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader_, locAmbientBoost_, &env_.ambientBoost, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader_, locSunBoost_, &env_.sunBoost, SHADER_UNIFORM_FLOAT);
    int direct = directOut ? 1 : 0;
    SetShaderValue(shader_, locDirectOut_, &direct, SHADER_UNIFORM_INT);
    SetShaderValue(shader_, locExposure_, &env_.exposure, SHADER_UNIFORM_FLOAT);

    int shadowOn = shadowValid_ ? 1 : 0;
    SetShaderValue(shader_, locShadowOn_, &shadowOn, SHADER_UNIFORM_INT);
    if (shadowValid_) {
        SetShaderValueMatrix(shader_, locLightVP_, lightVP_);
        Vector2 texel{1.0f / (float)shadowSize_, 1.0f / (float)shadowSize_};
        SetShaderValue(shader_, locShadowTexel_, &texel, SHADER_UNIFORM_VEC2);
    }

    float pos[kMaxLights * 4] = {}, col[kMaxLights * 4] = {};
    int n = std::min((int)lights_.size(), kMaxLights);
    for (int i = 0; i < n; i++) {
        pos[i * 4 + 0] = lights_[(size_t)i].position.x;
        pos[i * 4 + 1] = lights_[(size_t)i].position.y;
        pos[i * 4 + 2] = lights_[(size_t)i].position.z;
        pos[i * 4 + 3] = lights_[(size_t)i].radius;
        col[i * 4 + 0] = lights_[(size_t)i].color.x;
        col[i * 4 + 1] = lights_[(size_t)i].color.y;
        col[i * 4 + 2] = lights_[(size_t)i].color.z;
        col[i * 4 + 3] = 1.0f;
    }
    SetShaderValue(shader_, locLightCount_, &n, SHADER_UNIFORM_INT);
    if (n > 0) {
        SetShaderValueV(shader_, locLightPos_, pos, SHADER_UNIFORM_VEC4, n);
        SetShaderValueV(shader_, locLightCol_, col, SHADER_UNIFORM_VEC4, n);
    }

    // Das Material des nächsten Körpers neu einstellen
    lastMat_ = -2;
    lastAnchored_ = -1;
    material_.maps[MATERIAL_MAP_HEIGHT].texture = shadowValid_ ? shadowRT_.depth : Texture2D{};
}

void LitRenderer::begin(const Camera3D& camera) const { uploadFrameUniforms(camera, (float)GetTime(), false); }

void LitRenderer::drawSky(const Camera3D& camera, float time, int width, int height, bool directOut) const {
    if (skyShader_.id == 0) {
        DrawRectangleGradientV(0, 0, width, (int)(height * 0.62f),
                               Color{(unsigned char)(env_.skyTopColor.x * 255), (unsigned char)(env_.skyTopColor.y * 255), (unsigned char)(env_.skyTopColor.z * 255), 255},
                               Color{(unsigned char)(env_.fogColor.x * 255), (unsigned char)(env_.fogColor.y * 255), (unsigned char)(env_.fogColor.z * 255), 255});
        return;
    }
    Vector3 f = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 r = Vector3Normalize(Vector3CrossProduct(f, camera.up));
    Vector3 u = Vector3CrossProduct(r, f);
    float tanHalf = std::tan(camera.fovy * 0.5f * DEG2RAD);
    Vector2 res{(float)width, (float)height};
    float aspect = (float)width / (float)std::max(1, height);
    Vector3 sun = Vector3Normalize(env_.sunDirection);
    int direct = directOut ? 1 : 0;
    Shader& sh = const_cast<Shader&>(skyShader_);
    SetShaderValue(sh, skyRes_, &res, SHADER_UNIFORM_VEC2);
    SetShaderValue(sh, skyF_, &f, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, skyR_, &r, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, skyU_, &u, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, skyTan_, &tanHalf, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, skyAspect_, &aspect, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, skyTop_, &env_.skyTopColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, skyHorizon_, &env_.fogColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, skySunDir_, &sun, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, skySunColor_, &env_.sunColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, skyTime_, &time, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, skyCloud_, &env_.cloudAmount, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, skyGlow_, &env_.sunGlow, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, skyDirect_, &direct, SHADER_UNIFORM_INT);
    SetShaderValue(sh, skyExposure_, &env_.exposure, SHADER_UNIFORM_FLOAT);
    BeginShaderMode(sh);
    DrawRectangle(0, 0, width, height, WHITE);
    EndShaderMode();
}

void LitRenderer::renderShadowMap(Vector3 focus, const std::function<void()>& drawScene) {
    shadowValid_ = false;
    if (quality_ < 1 || depthMaterial_.shader.id == 0 || depthShader_.id == 0) return;
    if (!ensureShadowTarget(quality_ >= 2 ? 2048 : 1024)) return;

    const float extent = 30.0f;  // halbe Kantenlänge des Schattenbereichs um den Spieler
    Vector3 sun = Vector3Normalize(env_.sunDirection);
    Vector3 up = std::fabs(sun.y) > 0.95f ? Vector3{0, 0, 1} : Vector3{0, 1, 0};
    Vector3 zAxis = Vector3Negate(sun);
    Vector3 xAxis = Vector3Normalize(Vector3CrossProduct(up, zAxis));
    Vector3 yAxis = Vector3CrossProduct(zAxis, xAxis);
    // Auf das Texelraster einrasten, damit Schattenkanten beim Laufen nicht flimmern
    float texelSize = 2.0f * extent / (float)shadowSize_;
    float fx = std::floor(Vector3DotProduct(focus, xAxis) / texelSize) * texelSize;
    float fy = std::floor(Vector3DotProduct(focus, yAxis) / texelSize) * texelSize;
    float fz = Vector3DotProduct(focus, zAxis);
    Vector3 center = Vector3Add(Vector3Add(Vector3Scale(xAxis, fx), Vector3Scale(yAxis, fy)), Vector3Scale(zAxis, fz));

    Camera3D lc{};
    lc.target = center;
    lc.position = Vector3Subtract(center, Vector3Scale(sun, 70.0f));
    lc.up = up;
    lc.fovy = extent * 2.0f;
    lc.projection = CAMERA_ORTHOGRAPHIC;

    rlSetClipPlanes(1.0, 150.0);
    BeginTextureMode(shadowRT_);
    ClearBackground(BLACK);
    BeginMode3D(lc);
    lightVP_ = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
    shadowPass_ = true;
    drawScene();
    shadowPass_ = false;
    EndMode3D();
    EndTextureMode();
    rlSetClipPlanes(0.1, 500.0);
    shadowValid_ = true;
}

void LitRenderer::renderFrame(const Camera3D& camera, Vector3 focus, float time, const std::function<void()>& drawScene) {
    if (!lit_) {
        ClearBackground(Color{(unsigned char)(env_.fogColor.x * 255), (unsigned char)(env_.fogColor.y * 255), (unsigned char)(env_.fogColor.z * 255), 255});
        drawSky(camera, time, GetScreenWidth(), GetScreenHeight(), true);
        BeginMode3D(camera);
        drawScene();
        EndMode3D();
        return;
    }
    if (quality_ >= 1 && post_.ready() && post_.resize(GetScreenWidth(), GetScreenHeight())) {
        renderShadowMap(focus, drawScene);
        BeginTextureMode(post_.scene());
        ClearBackground(BLACK);
        drawSky(camera, time, post_.scene().texture.width, post_.scene().texture.height, false);
        BeginMode3D(camera);
        uploadFrameUniforms(camera, time, false);
        drawScene();
        EndMode3D();
        EndTextureMode();
        post_.present(env_, time, quality_);
        return;
    }
    // Ohne HDR-Zielbild (niedrige Qualität oder Fehler): direkt ins Fenster, Tonemapping im Shader
    shadowValid_ = false;
    ClearBackground(BLACK);
    drawSky(camera, time, GetScreenWidth(), GetScreenHeight(), true);
    BeginMode3D(camera);
    uploadFrameUniforms(camera, time, true);
    drawScene();
    EndMode3D();
}

void LitRenderer::uploadMaterial(MaterialDef* def, MaterialId id) const {
    if (def) {
        material_.maps[MATERIAL_MAP_ALBEDO].texture = def->color;
        material_.maps[MATERIAL_MAP_NORMAL].texture = def->normal;
    } else {
        material_.maps[MATERIAL_MAP_ALBEDO].texture = whiteTex_;
        material_.maps[MATERIAL_MAP_NORMAL].texture = Texture2D{};
    }
    if (id == lastMat_) return;
    lastMat_ = id;
    int useTex = def ? 1 : 0;
    SetShaderValue(shader_, locUseTex_, &useTex, SHADER_UNIFORM_INT);
    if (def) {
        float a[4] = {1.0f / def->tile, def->normalStrength, def->gloss, def->spec};
        float b[4] = {def->emissive, def->scroll, def->tint, 0.0f};
        int hasNormal = def->normal.id != 0 ? 1 : 0;
        SetShaderValue(shader_, locMatA_, a, SHADER_UNIFORM_VEC4);
        SetShaderValue(shader_, locMatB_, b, SHADER_UNIFORM_VEC4);
        float c[4] = {def->saturation, def->brightness, 0.0f, 0.0f};
        SetShaderValue(shader_, locMatC_, c, SHADER_UNIFORM_VEC4);
        SetShaderValue(shader_, locHasNormal_, &hasNormal, SHADER_UNIFORM_INT);
    }
}

void LitRenderer::bindSurface(Color color) const {
    // Durchscheinende Körper werden nie texturiert
    MaterialDef* def = (curMat_ != kNoMaterial && color.a >= 250) ? lib_.acquire(curMat_) : nullptr;
    uploadMaterial(def, def ? curMat_ : kNoMaterial);
    int anch = (def && anchored_) ? 1 : 0;
    if (anch != lastAnchored_) {
        lastAnchored_ = anch;
        SetShaderValue(shader_, locUseAnchor_, &anch, SHADER_UNIFORM_INT);
    }
    if (anch == 1) SetShaderValue(shader_, locTexAnchor_, &anchor_, SHADER_UNIFORM_VEC4);
}

void LitRenderer::setAnchor(Vector3 position, float yaw) const {
    anchored_ = true;
    anchor_ = {position.x, position.y, position.z, yaw};
    lastAnchored_ = -1;
}

void LitRenderer::clearAnchor() const {
    anchored_ = false;
    lastAnchored_ = -1;
}

void LitRenderer::limb(Vector3 a, Vector3 b, float radius, Color color) const {
    Vector3 d = Vector3Subtract(b, a);
    float len = Vector3Length(d);
    if (len < 1e-4f) {
        sphere(a, radius, color);
        return;
    }
    Quaternion q = QuaternionFromVector3ToVector3({0, 1, 0}, Vector3Scale(d, 1.0f / len));
    Matrix m = MatrixMultiply(MatrixMultiply(MatrixScale(radius, len, radius), QuaternionToMatrix(q)), MatrixTranslate(a.x, a.y, a.z));
    drawMatrix(cylinder_, m, color);
    sphere(a, radius, color);
    sphere(b, radius, color);
}

void LitRenderer::taper(Vector3 a, Vector3 b, float rA, float rB, Color color, bool rounded) const {
    Vector3 d = Vector3Subtract(b, a);
    float len = Vector3Length(d);
    if (len < 1e-4f) {
        sphere(a, std::max(rA, rB), color);
        return;
    }
    // Das Netz ist unten breit; ist b das breitere Ende, wird die Achse umgedreht
    const bool flip = rB > rA;
    Vector3 from = flip ? b : a, to = flip ? a : b;
    float big = std::max(rA, rB), small = std::min(rA, rB);
    int idx = (int)std::lround(std::clamp(small / std::max(big, 1e-4f), 0.0f, 1.0f) * (float)(kTaperMeshes - 1));
    Quaternion q = QuaternionFromVector3ToVector3({0, 1, 0}, Vector3Normalize(Vector3Subtract(to, from)));
    Matrix m = MatrixMultiply(MatrixMultiply(MatrixScale(big, len, big), QuaternionToMatrix(q)), MatrixTranslate(from.x, from.y, from.z));
    drawMatrix(taper_[idx], m, color);
    if (rounded) {
        sphere(a, rA, color);
        sphere(b, rB, color);
    }
}

void LitRenderer::bar(Vector3 a, Vector3 b, float width, float thickness, Color color, Vector3 up) const {
    Vector3 d = Vector3Subtract(b, a);
    float len = Vector3Length(d);
    if (len < 1e-4f) return;
    Vector3 z = Vector3Scale(d, 1.0f / len);
    Vector3 x = Vector3CrossProduct(up, z);
    if (Vector3Length(x) < 1e-3f) x = Vector3CrossProduct({1, 0, 0}, z);
    x = Vector3Normalize(x);
    Vector3 y = Vector3CrossProduct(z, x);
    // Basisvektoren als Zeilen (raylib-Matrizen wirken auf Zeilenvektoren)
    Matrix rot = {x.x, y.x, z.x, 0, x.y, y.y, z.y, 0, x.z, y.z, z.z, 0, 0, 0, 0, 1};
    Vector3 mid = Vector3Scale(Vector3Add(a, b), 0.5f);
    Matrix m = MatrixMultiply(MatrixMultiply(MatrixScale(width, thickness, len), rot), MatrixTranslate(mid.x, mid.y, mid.z));
    drawMatrix(cube_, m, color);
}

void LitRenderer::draw(const Mesh& mesh, Vector3 scale, float yaw, Vector3 position, Color color) const {
    Matrix m = MatrixMultiply(MatrixMultiply(MatrixScale(scale.x, scale.y, scale.z), MatrixRotateY(yaw)),
                              MatrixTranslate(position.x, position.y, position.z));
    drawMatrix(mesh, m, color);
}

void LitRenderer::drawMatrix(const Mesh& mesh, const Matrix& m, Color color) const {
    if (shadowPass_) {
        // Nur feste Körper werfen Schatten; Lichthöfe und Funken (durchscheinend) nicht
        if (color.a < 200) return;
        if (curMat_ != kNoMaterial) {
            MaterialDef* def = lib_.acquire(curMat_);
            if (def && def->emissive > 0.0f) return;  // Lava wirft keinen Schatten
        }
        DrawMesh(mesh, depthMaterial_, m);
        return;
    }
    if (lit_) bindSurface(color);
    material_.maps[MATERIAL_MAP_DIFFUSE].color = color;
    DrawMesh(mesh, material_, m);
}

void LitRenderer::box(Vector3 center, Vector3 size, Color color, float yaw) const {
    draw(cube_, size, yaw, center, color);
}

void LitRenderer::sphere(Vector3 center, float radius, Color color) const {
    draw(sphere_, {radius, radius, radius}, 0.0f, center, color);
}

void LitRenderer::ellipsoid(Vector3 center, Vector3 radii, Color color, float yaw) const {
    draw(sphere_, radii, yaw, center, color);
}

void LitRenderer::cylinder(Vector3 baseCenter, float radius, float height, Color color) const {
    draw(cylinder_, {radius, height, radius}, 0.0f, baseCenter, color);
}

void LitRenderer::cloth(const Vector3* g, Color color) const {
    const int n = kClothCols * kClothRows;
    float verts[kClothCols * kClothRows * 2 * 3];
    float norms[kClothCols * kClothRows * 2 * 3];
    auto at = [&](int c, int r) -> const Vector3& {
        c = std::clamp(c, 0, kClothCols - 1);
        r = std::clamp(r, 0, kClothRows - 1);
        return g[r * kClothCols + c];
    };
    for (int r = 0; r < kClothRows; r++) {
        for (int c = 0; c < kClothCols; c++) {
            Vector3 u = Vector3Subtract(at(c + 1, r), at(c - 1, r));
            Vector3 v = Vector3Subtract(at(c, r + 1), at(c, r - 1));
            Vector3 nrm = Vector3CrossProduct(v, u);
            float len = Vector3Length(nrm);
            nrm = len > 1e-6f ? Vector3Scale(nrm, 1.0f / len) : Vector3{0, 0, -1};
            int i = r * kClothCols + c;
            const Vector3& p = g[i];
            verts[i * 3 + 0] = p.x; verts[i * 3 + 1] = p.y; verts[i * 3 + 2] = p.z;
            verts[(n + i) * 3 + 0] = p.x; verts[(n + i) * 3 + 1] = p.y; verts[(n + i) * 3 + 2] = p.z;
            norms[i * 3 + 0] = nrm.x; norms[i * 3 + 1] = nrm.y; norms[i * 3 + 2] = nrm.z;
            norms[(n + i) * 3 + 0] = -nrm.x; norms[(n + i) * 3 + 1] = -nrm.y; norms[(n + i) * 3 + 2] = -nrm.z;
        }
    }
    UpdateMeshBuffer(cloth_, 0, verts, (int)sizeof(verts), 0);
    UpdateMeshBuffer(cloth_, 2, norms, (int)sizeof(norms), 0);
    Matrix identity = MatrixIdentity();
    if (shadowPass_) {
        if (color.a >= 200) DrawMesh(cloth_, depthMaterial_, identity);
        return;
    }
    if (lit_) bindSurface(color);
    material_.maps[MATERIAL_MAP_DIFFUSE].color = color;
    DrawMesh(cloth_, material_, identity);
}

void LitRenderer::ramp(const LevelRamp& r) const {
    // Der Einheitskeil steigt entlang +z. Für andere Richtungen wird gedreht und die Breite getauscht.
    float yaw = 0.0f;
    Vector3 scale = r.size;
    switch (r.rise) {
        case RampDir::PlusZ: break;
        case RampDir::MinusZ: yaw = PI; break;
        case RampDir::PlusX:
            yaw = PI * 0.5f;
            scale = {r.size.z, r.size.y, r.size.x};
            break;
        case RampDir::MinusX:
            yaw = -PI * 0.5f;
            scale = {r.size.z, r.size.y, r.size.x};
            break;
    }
    draw(wedge_, scale, yaw, {r.center.x, r.baseY(), r.center.z}, r.color);
}

}  // namespace aldoria
