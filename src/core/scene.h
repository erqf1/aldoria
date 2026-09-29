#pragma once

#include <memory>
#include <string>
#include <vector>

namespace aldoria {

// Ein Spielzustand (Titel, Spiel, ...). `dt` ist Spielzeit, während Hit-Stop also 0.
class Scene {
public:
    virtual ~Scene() = default;
    virtual void update(float dt) = 0;
    virtual void draw() = 0;

    // Entwickler-/Testbefehle aus Skripten (z. B. "teleport 1 0 2"). Gibt true zurück, wenn der Befehl bekannt war.
    virtual bool debugCommand(const std::string& name, const std::vector<std::string>& args) {
        (void)name;
        (void)args;
        return false;
    }

    // Will die Szene wechseln? Dann kommt hier die nächste Szene zurück (einmalig).
    virtual std::unique_ptr<Scene> takeNext() { return nullptr; }

    // Will das Spiel beendet werden?
    virtual bool wantsQuit() const { return false; }
};

}  // namespace aldoria
