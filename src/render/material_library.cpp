#include "render/material_library.h"

#include <algorithm>
#include <set>

#include "core/file_util.h"
#include "core/log.h"

namespace aldoria {
namespace {

constexpr size_t kTextureBudget = 360ull * 1024 * 1024;  // Grafikspeicher für Materialtexturen (RGBA8 mit Mip-Stufen)

Texture2D loadTextureFile(const std::string& path, int maxSize) {
    Texture2D t{};
    Image img = LoadImage(path.c_str());
    if (img.data == nullptr) return t;
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    if (maxSize > 0 && img.width > maxSize) ImageResize(&img, maxSize, maxSize * img.height / img.width);
    t = LoadTextureFromImage(img);
    UnloadImage(img);
    if (t.id == 0) return t;
    GenTextureMipmaps(&t);
    SetTextureWrap(t, TEXTURE_WRAP_REPEAT);
    SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR);
    SetTextureFilter(t, TEXTURE_FILTER_ANISOTROPIC_8X);
    return t;
}

}  // namespace

bool MaterialLibrary::load(const std::string& dataDir) {
    unloadAll();
    materials_.clear();
    byName_.clear();
    themes_.clear();
    textureDir_ = dataDir + "/textures";

    std::string error;
    auto j = loadJsonFile(dataDir + "/materials.json", error);
    if (!j || !j->is_object()) {
        Log::warn(LogCategory::Loading, "materials.json fehlt oder ist ungültig ({}), Oberflächen bleiben einfarbig", error);
        return false;
    }
    if (auto it = j->find("materials"); it != j->end() && it->is_object()) {
        for (auto m = it->begin(); m != it->end(); ++m) {
            if (!m.value().is_object()) continue;
            MaterialDef d;
            d.name = m.key();
            d.colorFile = m.value().value("color", d.name + "_color.jpg");
            d.normalFile = m.value().value("normal_file", d.name + "_normal.jpg");
            d.tile = std::max(0.1f, m.value().value("tile", d.tile));
            d.normalStrength = m.value().value("normal", d.normalStrength);
            d.gloss = m.value().value("gloss", d.gloss);
            d.spec = m.value().value("spec", d.spec);
            d.emissive = m.value().value("emissive", d.emissive);
            d.scroll = m.value().value("scroll", d.scroll);
            d.tint = m.value().value("tint", d.tint);
            d.saturation = m.value().value("sat", d.saturation);
            d.brightness = m.value().value("bright", d.brightness);
            d.maxSize = m.value().value("max_size", d.maxSize);
            byName_[d.name] = (MaterialId)materials_.size();
            materials_.push_back(std::move(d));
        }
    }
    if (auto it = j->find("themes"); it != j->end() && it->is_object()) {
        for (auto t = it->begin(); t != it->end(); ++t) {
            if (!t.value().is_object()) continue;
            for (auto r = t.value().begin(); r != t.value().end(); ++r) {
                if (r.value().is_string()) themes_[t.key()][r.key()] = r.value().get<std::string>();
            }
        }
    }
    Log::info(LogCategory::Loading, "{} Materialien, {} Themen", materials_.size(), themes_.size());
    return !materials_.empty();
}

void MaterialLibrary::unloadAll() {
    for (MaterialDef& m : materials_) {
        if (m.loaded) unloadMaterial(m);
    }
    for (auto& [key, e] : textures_) {
        if (e.tex.id != 0) UnloadTexture(e.tex);
    }
    textures_.clear();
    textureBytes_ = 0;
}

Texture2D MaterialLibrary::acquireTexture(const std::string& file, int maxSize) {
    const std::string key = file + "@" + std::to_string(maxSize);
    auto it = textures_.find(key);
    if (it != textures_.end()) {
        it->second.refs++;
        return it->second.tex;
    }
    Texture2D t = loadTextureFile(textureDir_ + "/" + file, maxSize);
    if (t.id == 0) return t;
    TexEntry e;
    e.tex = t;
    e.refs = 1;
    e.bytes = (size_t)t.width * (size_t)t.height * 4 * 4 / 3;
    textureBytes_ += e.bytes;
    textures_[key] = e;
    return t;
}

void MaterialLibrary::releaseTexture(const std::string& file, int maxSize) {
    const std::string key = file + "@" + std::to_string(maxSize);
    auto it = textures_.find(key);
    if (it == textures_.end()) return;
    if (--it->second.refs > 0) return;
    if (it->second.tex.id != 0) UnloadTexture(it->second.tex);
    textureBytes_ -= std::min(textureBytes_, it->second.bytes);
    textures_.erase(it);
}

void MaterialLibrary::unloadMaterial(MaterialDef& m) {
    if (m.color.id != 0) releaseTexture(m.colorFile, m.maxSize);
    if (m.normal.id != 0) releaseTexture(m.normalFile, m.maxSize);
    m.color = {};
    m.normal = {};
    m.loaded = false;
}

MaterialId MaterialLibrary::find(const std::string& name) const {
    auto it = byName_.find(name);
    return it == byName_.end() ? kNoMaterial : it->second;
}

MaterialId MaterialLibrary::themeRole(const std::string& theme, const std::string& role) const {
    auto t = themes_.find(theme);
    if (t == themes_.end()) return kNoMaterial;
    auto r = t->second.find(role);
    if (r == t->second.end()) return kNoMaterial;
    return find(r->second);
}

std::vector<MaterialId> MaterialLibrary::themeMaterials(const std::string& theme) const {
    std::vector<MaterialId> out;
    auto t = themes_.find(theme);
    if (t == themes_.end()) return out;
    std::set<MaterialId> seen;
    for (const auto& [role, name] : t->second) {
        MaterialId id = find(name);
        if (id != kNoMaterial && seen.insert(id).second) out.push_back(id);
    }
    return out;
}

size_t MaterialLibrary::loadedCount() const {
    size_t n = 0;
    for (const MaterialDef& m : materials_) n += m.loaded ? 1 : 0;
    return n;
}

void MaterialLibrary::loadTextures(MaterialDef& m) {
    m.color = acquireTexture(m.colorFile, m.maxSize);
    m.normal = acquireTexture(m.normalFile, m.maxSize);
    if (m.color.id == 0) {
        Log::warn(LogCategory::Loading, "Textur fehlt: {}/{}", textureDir_, m.colorFile);
        if (m.normal.id != 0) releaseTexture(m.normalFile, m.maxSize);
        m.color = {};
        m.normal = {};
        m.failed = true;
        return;
    }
    m.loaded = true;  // fehlt nur die Normalkarte, bleibt normal.id == 0 (der Shader nutzt dann die Flächennormale)
}

// Am wenigsten benutzte Materialien entladen, bis die Texturen wieder ins Speicherbudget passen
void MaterialLibrary::evictIfNeeded(MaterialId keep) {
    while (textureBytes_ > kTextureBudget) {
        MaterialDef* oldest = nullptr;
        for (size_t i = 0; i < materials_.size(); i++) {
            MaterialDef& m = materials_[i];
            if (!m.loaded || (MaterialId)i == keep) continue;
            if (!oldest || m.lastUse < oldest->lastUse) oldest = &m;
        }
        if (!oldest) return;
        unloadMaterial(*oldest);
    }
}

MaterialDef* MaterialLibrary::acquire(MaterialId id) {
    if (id < 0 || (size_t)id >= materials_.size()) return nullptr;
    MaterialDef& m = materials_[(size_t)id];
    if (m.failed) return nullptr;
    if (!m.loaded) {
        loadTextures(m);
        if (!m.loaded) return nullptr;
        evictIfNeeded(id);
    }
    m.lastUse = ++clock_;
    return &m;
}

}  // namespace aldoria
