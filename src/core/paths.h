#pragma once

#include <string>

namespace aldoria {

// Ordner mit Spieldaten (JSON, Shader). Reihenfolge: Umgebungsvariable ALDORIA_DATA,
// data-Ordner im Quellbaum (Dev-Build), data-Ordner neben der .exe.
const std::string& dataDir();
std::string dataPath(const std::string& relative);

// Ordner für Spielstände und Einstellungen (neben der .exe, Umgebungsvariable ALDORIA_SAVES überschreibt ihn)
std::string savesDir();

}  // namespace aldoria
