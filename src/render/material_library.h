#pragma once

#include <map>
#include <string>
#include <vector>

#include "raylib.h"

namespace aldoria {

using MaterialId = int;
constexpr MaterialId kNoMaterial = -1;

// Ein Oberflächenmaterial: Farb- und Normaltextur plus Kennwerte für den Licht-Shader.
struct MaterialDef {
    std::string name;
    std::string colorFile, normalFile;   // relativ zu data/textures
    float tile = 2.5f;                   // Kantenlänge einer Kachel in Metern
    float normalStrength = 1.0f;
    float gloss = 0.2f;                  // 0 = matt, 1 = poliert
    float spec = 0.3f;                   // Stärke der Glanzlichter
    float emissive = 0.0f;               // > 0: leuchtet selbst (Lava)
    float scroll = 0.0f;                 // Fließgeschwindigkeit der Textur
    float tint = 0.2f;                   // wie stark die Raumfarbe die Textur einfärbt
    float saturation = 1.0f;             // Farbsättigung der Textur
    float brightness = 1.0f;             // Helligkeitsfaktor der Textur
    int maxSize = 0;                     // > 0: Texturen werden beim Laden auf diese Kantenlänge verkleinert (spart Grafikspeicher)
    Texture2D color{}, normal{};
    bool loaded = false, failed = false;
    unsigned long long lastUse = 0;
};

// Lädt Materialbeschreibungen aus data/materials.json und die Texturen erst bei Bedarf (mit Begrenzung des Grafikspeichers).
class MaterialLibrary {
public:
    bool load(const std::string& dataDir);
    void unloadAll();

    MaterialId find(const std::string& name) const;
    // Material einer Rolle ("wall", "floor", ...) im Thema (z. B. "wurzel"); kNoMaterial, wenn es keins gibt
    MaterialId themeRole(const std::string& theme, const std::string& role) const;
    std::vector<MaterialId> themeMaterials(const std::string& theme) const;

    // Liefert das Material mit geladenen Texturen (oder nullptr, wenn sie fehlen)
    MaterialDef* acquire(MaterialId id);
    size_t count() const { return materials_.size(); }
    size_t loadedCount() const;

private:
    void loadTextures(MaterialDef& m);
    void unloadMaterial(MaterialDef& m);
    void evictIfNeeded(MaterialId keep);
    // Texturdateien werden geteilt: Materialien mit derselben Datei (und Größe) belegen den Speicher nur einmal
    Texture2D acquireTexture(const std::string& file, int maxSize);
    void releaseTexture(const std::string& file, int maxSize);
    size_t textureBytes() const { return textureBytes_; }

    std::string textureDir_;
    std::vector<MaterialDef> materials_;
    std::map<std::string, MaterialId> byName_;
    std::map<std::string, std::map<std::string, std::string>> themes_;
    struct TexEntry {
        Texture2D tex{};
        int refs = 0;
        size_t bytes = 0;
    };
    std::map<std::string, TexEntry> textures_;
    size_t textureBytes_ = 0;
    unsigned long long clock_ = 0;
};

}  // namespace aldoria
