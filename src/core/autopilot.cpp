#include "core/autopilot.h"

#include <format>
#include <sstream>

#include "core/file_util.h"

namespace aldoria {
namespace {

std::vector<std::string> splitWords(const std::string& line) {
    std::vector<std::string> words;
    std::istringstream in(line);
    std::string w;
    while (in >> w) words.push_back(w);
    return words;
}

bool parseFloat(const std::string& s, float& out) {
    try {
        size_t used = 0;
        out = std::stof(s, &used);
        return used == s.size();
    } catch (...) {
        return false;
    }
}

}  // namespace

bool Autopilot::loadFromFile(const std::string& path, std::string& error) {
    auto text = readTextFile(path);
    if (!text) {
        error = "Skript nicht gefunden: " + path;
        return false;
    }
    return loadFromString(*text, error);
}

bool Autopilot::loadFromString(const std::string& text, std::string& error) {
    steps_.clear();
    index_ = 0;
    elapsed_ = 0.0f;

    std::istringstream in(text);
    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        lineNo++;
        if (auto hash = line.find('#'); hash != std::string::npos) line.erase(hash);
        std::vector<std::string> w = splitWords(line);
        if (w.empty()) continue;

        Step step;
        const std::string& cmd = w[0];
        if (cmd == "wait") {
            step.type = StepType::Wait;
            if (w.size() != 2 || !parseFloat(w[1], step.duration)) {
                error = std::format("Zeile {}: 'wait <Sekunden>' erwartet", lineNo);
                return false;
            }
        } else if (cmd == "hold" || cmd == "press") {
            step.type = cmd == "hold" ? StepType::Hold : StepType::Press;
            size_t expected = cmd == "hold" ? 3 : 2;
            if (w.size() != expected) {
                error = std::format("Zeile {}: '{}' hat die falsche Anzahl Argumente", lineNo, cmd);
                return false;
            }
            std::istringstream names(w[1]);
            std::string name;
            while (std::getline(names, name, '+')) {
                Action a;
                if (!actionFromName(name, a)) {
                    error = std::format("Zeile {}: unbekannte Aktion '{}'", lineNo, name);
                    return false;
                }
                step.actions.push_back(a);
            }
            if (step.type == StepType::Hold && !parseFloat(w[2], step.duration)) {
                error = std::format("Zeile {}: Dauer ist keine Zahl", lineNo);
                return false;
            }
        } else if (cmd == "look") {
            step.type = StepType::Look;
            if (w.size() != 3 || !parseFloat(w[1], step.look.x) || !parseFloat(w[2], step.look.y)) {
                error = std::format("Zeile {}: 'look <yaw> <pitch>' erwartet", lineNo);
                return false;
            }
        } else {
            step.type = StepType::Command;
            step.command.name = cmd;
            step.command.args.assign(w.begin() + 1, w.end());
        }
        steps_.push_back(std::move(step));
    }
    return true;
}

void Autopilot::update(float dt, Input& input, std::vector<AutopilotCommand>& commands) {
    input.clearInjected();
    if (finished()) return;

    Step& s = steps_[index_];
    switch (s.type) {
        case StepType::Wait:
            elapsed_ += dt;
            if (elapsed_ >= s.duration) {
                index_++;
                elapsed_ = 0.0f;
            }
            break;
        case StepType::Hold:
            for (Action a : s.actions) input.injectAction(a, true);
            elapsed_ += dt;
            if (elapsed_ >= s.duration) {
                index_++;
                elapsed_ = 0.0f;
            }
            break;
        case StepType::Press:
            for (Action a : s.actions) input.injectAction(a, true);
            index_++;
            break;
        case StepType::Look:
            input.injectLook(s.look);
            index_++;
            break;
        case StepType::Command:
            commands.push_back(s.command);
            index_++;
            break;
    }
}

}  // namespace aldoria
