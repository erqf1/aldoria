#pragma once

#include <optional>
#include <string>

#include "core/json.h"

namespace aldoria {

std::optional<std::string> readTextFile(const std::string& path);

// Schreibt atomar (Temp-Datei, dann umbenennen). Legt fehlende Ordner an.
bool writeTextFileAtomic(const std::string& path, const std::string& text);

// Liest und parst eine JSON-Datei. Bei Fehlern kommt ein leeres optional und eine Meldung in `error`.
std::optional<json> loadJsonFile(const std::string& path, std::string& error);

}  // namespace aldoria
