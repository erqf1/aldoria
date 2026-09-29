#include "core/file_util.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace aldoria {

std::optional<std::string> readTextFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return std::nullopt;
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool writeTextFileAtomic(const std::string& path, const std::string& text) {
    std::error_code ec;
    std::filesystem::path p(path);
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
    std::string temp = path + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << text;
        out.flush();
        if (!out) return false;
    }
    std::filesystem::rename(temp, path, ec);
    return !ec;
}

std::optional<json> loadJsonFile(const std::string& path, std::string& error) {
    auto text = readTextFile(path);
    if (!text) {
        error = "Datei nicht gefunden: " + path;
        return std::nullopt;
    }
    json j = json::parse(*text, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded()) {
        error = "Ungültiges JSON: " + path;
        return std::nullopt;
    }
    return j;
}

}  // namespace aldoria
