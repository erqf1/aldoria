#include "render/post_process.h"

#include <algorithm>

#include "core/log.h"
#include "rlgl.h"

namespace aldoria {

RenderTexture2D PostProcess::makeTarget(int width, int height, bool withDepth) {
    RenderTexture2D rt{};
    rt.id = rlLoadFramebuffer();
    if (rt.id == 0) return rt;
    rt.texture.id = rlLoadTexture(nullptr, width, height, RL_PIXELFORMAT_UNCOMPRESSED_R16G16B16A16, 1);
    rt.texture.width = width;
    rt.texture.height = height;
    rt.texture.mipmaps = 1;
    rt.texture.format = RL_PIXELFORMAT_UNCOMPRESSED_R16G16B16A16;
    rlFramebufferAttach(rt.id, rt.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
    if (withDepth) {
        rt.depth.id = rlLoadTextureDepth(width, height, true);
        rt.depth.width = width;
        rt.depth.height = height;
        rt.depth.mipmaps = 1;
        rt.depth.format = 19;
        rlFramebufferAttach(rt.id, rt.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0);
    }
    if (!rlFramebufferComplete(rt.id)) {
        Log::error(LogCategory::Loading, "HDR-Zielbild {}x{} unvollständig", width, height);
        UnloadRenderTexture(rt);
        return RenderTexture2D{};
    }
    SetTextureFilter(rt.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(rt.texture, TEXTURE_WRAP_CLAMP);
    return rt;
}

bool PostProcess::init(const std::string& shaderDir) {
    down_ = LoadShader(nullptr, (shaderDir + "/bloom_down.fs").c_str());
    up_ = LoadShader(nullptr, (shaderDir + "/bloom_up.fs").c_str());
    final_ = LoadShader(nullptr, (shaderDir + "/post_final.fs").c_str());
    bool ok = down_.id > 0 && up_.id > 0 && final_.id > 0 && down_.id != rlGetShaderIdDefault() && up_.id != rlGetShaderIdDefault() &&
              final_.id != rlGetShaderIdDefault();
    if (!ok) {
        Log::error(LogCategory::Loading, "Nachbearbeitungs-Shader konnten nicht geladen werden ({}), zeichne ohne HDR", shaderDir);
        return false;
    }
    locDownTexel_ = GetShaderLocation(down_, "texel");
    locDownFirst_ = GetShaderLocation(down_, "firstPass");
    locDownThreshold_ = GetShaderLocation(down_, "threshold");
    locUpTexel_ = GetShaderLocation(up_, "texel");
    locUpRadius_ = GetShaderLocation(up_, "radius");
    locFinalBloom_ = GetShaderLocation(final_, "texBloom");
    locFinalTexel_ = GetShaderLocation(final_, "texel");
    locExposure_ = GetShaderLocation(final_, "exposure");
    locBloomStrength_ = GetShaderLocation(final_, "bloomStrength");
    locVignette_ = GetShaderLocation(final_, "vignette");
    locSaturation_ = GetShaderLocation(final_, "saturation");
    locGrade_ = GetShaderLocation(final_, "grade");
    locFxaa_ = GetShaderLocation(final_, "fxaaOn");
    locTime_ = GetShaderLocation(final_, "time");
    ready_ = true;
    return true;
}

void PostProcess::destroyTargets() {
    if (scene_.id != 0) UnloadRenderTexture(scene_);
    scene_ = RenderTexture2D{};
    for (RenderTexture2D& b : bloom_) {
        if (b.id != 0) UnloadRenderTexture(b);
        b = RenderTexture2D{};
    }
}

void PostProcess::shutdown() {
    destroyTargets();
    if (down_.id > 0 && down_.id != rlGetShaderIdDefault()) UnloadShader(down_);
    if (up_.id > 0 && up_.id != rlGetShaderIdDefault()) UnloadShader(up_);
    if (final_.id > 0 && final_.id != rlGetShaderIdDefault()) UnloadShader(final_);
    down_ = up_ = final_ = Shader{};
    ready_ = false;
}

bool PostProcess::resize(int width, int height) {
    if (!ready_) return false;
    width = std::max(width, 16);
    height = std::max(height, 16);
    if (width == width_ && height == height_ && scene_.id != 0) return true;
    destroyTargets();
    width_ = width;
    height_ = height;
    scene_ = makeTarget(width, height, true);
    if (scene_.id == 0) {
        ready_ = false;
        return false;
    }
    int w = width, h = height;
    for (RenderTexture2D& b : bloom_) {
        w = std::max(w / 2, 4);
        h = std::max(h / 2, 4);
        b = makeTarget(w, h, false);
        if (b.id == 0) {
            ready_ = false;
            return false;
        }
    }
    Log::info(LogCategory::Loading, "HDR-Zielbild {}x{} bereit", width, height);
    return true;
}

void PostProcess::runBloom(const Environment& env) {
    auto pass = [&](Shader& sh, const RenderTexture2D& src, const RenderTexture2D& dst, bool additive) {
        BeginTextureMode(dst);
        if (additive) BeginBlendMode(BLEND_ADDITIVE);
        BeginShaderMode(sh);
        DrawTexturePro(src.texture, Rectangle{0, 0, (float)src.texture.width, -(float)src.texture.height},
                       Rectangle{0, 0, (float)dst.texture.width, (float)dst.texture.height}, Vector2{0, 0}, 0.0f, WHITE);
        EndShaderMode();
        if (additive) EndBlendMode();
        EndTextureMode();
    };

    // Abwärts: Szene -> Stufe 0 -> ... -> Stufe 4
    for (int i = 0; i < kBloomLevels; i++) {
        const RenderTexture2D& src = i == 0 ? scene_ : bloom_[i - 1];
        Vector2 texel{1.0f / (float)src.texture.width, 1.0f / (float)src.texture.height};
        int first = i == 0 ? 1 : 0;
        float threshold = 1.6f / std::max(0.2f, env.exposure);
        SetShaderValue(down_, locDownTexel_, &texel, SHADER_UNIFORM_VEC2);
        SetShaderValue(down_, locDownFirst_, &first, SHADER_UNIFORM_INT);
        SetShaderValue(down_, locDownThreshold_, &threshold, SHADER_UNIFORM_FLOAT);
        pass(down_, src, bloom_[i], false);
    }
    // Aufwärts: kleinere Stufen additiv auf die größeren
    for (int i = kBloomLevels - 1; i >= 1; i--) {
        const RenderTexture2D& src = bloom_[i];
        Vector2 texel{1.0f / (float)src.texture.width, 1.0f / (float)src.texture.height};
        float radius = 1.0f;
        SetShaderValue(up_, locUpTexel_, &texel, SHADER_UNIFORM_VEC2);
        SetShaderValue(up_, locUpRadius_, &radius, SHADER_UNIFORM_FLOAT);
        pass(up_, src, bloom_[i - 1], true);
    }
}

void PostProcess::present(const Environment& env, float time, int quality) {
    if (!ready_ || scene_.id == 0) return;
    bool bloomOn = quality >= 2 && env.bloom > 0.001f;
    if (bloomOn) runBloom(env);

    Vector2 texel{1.0f / (float)width_, 1.0f / (float)height_};
    float bloomStrength = bloomOn ? env.bloom : 0.0f;
    int fxaa = quality >= 1 ? 1 : 0;
    SetShaderValue(final_, locFinalTexel_, &texel, SHADER_UNIFORM_VEC2);
    SetShaderValue(final_, locExposure_, &env.exposure, SHADER_UNIFORM_FLOAT);
    SetShaderValue(final_, locBloomStrength_, &bloomStrength, SHADER_UNIFORM_FLOAT);
    SetShaderValue(final_, locVignette_, &env.vignette, SHADER_UNIFORM_FLOAT);
    SetShaderValue(final_, locSaturation_, &env.saturation, SHADER_UNIFORM_FLOAT);
    SetShaderValue(final_, locGrade_, &env.grade, SHADER_UNIFORM_VEC3);
    SetShaderValue(final_, locFxaa_, &fxaa, SHADER_UNIFORM_INT);
    SetShaderValue(final_, locTime_, &time, SHADER_UNIFORM_FLOAT);

    BeginShaderMode(final_);
    SetShaderValueTexture(final_, locFinalBloom_, bloomOn ? bloom_[0].texture : scene_.texture);
    DrawTexturePro(scene_.texture, Rectangle{0, 0, (float)width_, -(float)height_},
                   Rectangle{0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()}, Vector2{0, 0}, 0.0f, WHITE);
    EndShaderMode();
}

}  // namespace aldoria
