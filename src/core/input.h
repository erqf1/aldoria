#pragma once

#include <array>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "raylib.h"

namespace aldoria {

// Logische Aktionen. Im Spielcode werden nie physische Tasten abgefragt.
enum class Action {
    MoveForward,
    MoveBack,
    MoveLeft,
    MoveRight,
    Jump,
    Sprint,
    Attack,
    Secondary,
    Ability,
    Interact,
    Dodge,
    Dash,
    Heal,
    Pause,
    Map,
    Inventory,
    LockOn,
    // Menüs
    UiUp,
    UiDown,
    UiLeft,
    UiRight,
    UiConfirm,
    UiBack,
    Count
};

// Aktionsname aus Skripten ("forward", "attack", ...). Gibt false zurück, wenn der Name unbekannt ist.
bool actionFromName(const std::string& name, Action& out);
// Umgekehrt: Skriptname einer Aktion ("jump", "attack", ...)
const char* actionName(Action a);
// Aktionen, deren Tasten der Spieler in den Einstellungen ändern kann, samt deutscher Bezeichnung
struct RebindableAction {
    Action action;
    const char* label;
};
const std::vector<RebindableAction>& rebindableActions();

struct Binding {
    enum class Device { Key, Mouse, GamepadButton };
    Device device;
    int code;
};

class Input {
public:
    Input();

    // Einmal pro Frame aufrufen, vor der Spiellogik
    void update();

    bool down(Action a) const { return cur_[index(a)]; }
    bool pressed(Action a) const { return cur_[index(a)] && !prev_[index(a)]; }
    bool released(Action a) const { return !cur_[index(a)] && prev_[index(a)]; }

    // x = rechts, y = vorwärts, Länge höchstens 1 (Tastatur oder linker Stick)
    Vector2 moveAxis() const { return move_; }

    // Blickänderung in Radiant. Die Maus zählt nur, wenn sie eingefangen ist.
    Vector2 lookDelta(float dt, bool mouseActive) const;

    void rebind(Action a, std::vector<Binding> bindings) { bindings_[index(a)] = std::move(bindings); }
    void setDefaultBindings();

    // ---- Tasten belegen (Einstellungen): die nächste gedrückte Taste oder Maustaste gehört der Aktion. Esc bricht ab,
    // Entf löscht die Taste an diesem Platz. Jede Aktion kann bis zu kMaxUserBindings Tasten haben, dieselbe Taste darf
    // mehreren Aktionen gehören.
    static constexpr int kMaxUserBindings = 4;
    // slot < 0: eine weitere Taste anhängen, sonst die Taste an diesem Platz ersetzen
    void startRebind(Action a, int slot = -1);
    bool rebinding() const { return rebinding_; }
    Action rebindAction() const { return rebindAction_; }
    int rebindSlot() const { return rebindSlot_; }
    // Zählt jede Änderung der Belegung hoch (Menüs bauen sich danach neu auf)
    int revision() const { return revision_; }
    // Name der Taste, mit der die Aktion ausgelöst wird (Tastatur oder Maus zuerst), z. B. "E", "Leertaste", "Maus links"
    std::string bindingName(Action a) const;
    // Alle Tasten und Maustasten der Aktion, mit ", " getrennt ("-" wenn keine)
    std::string bindingNames(Action a) const;
    // Tasten und Maustasten der Aktion in der Reihenfolge der Belegung (ohne Controller)
    std::vector<Binding> userBindings(Action a) const;
    static std::string bindingLabel(const Binding& b);
    // Ersetzt alle Tasten und Maustasten einer Aktion (Doppelte fallen weg, höchstens kMaxUserBindings), Controller bleibt
    void setUserBindings(Action a, std::vector<Binding> list);
    void removeUserBinding(Action a, int slot);
    // Eine Aktion oder alle wieder auf die Standardtasten
    void resetAction(Action a);
    void resetAllBindings();
    bool isDefault(Action a) const;
    // Bezeichnungen anderer Aktionen, die eine der Tasten dieser Aktion ebenfalls nutzen
    std::vector<std::string> sharedWith(Action a) const;
    // Nur die vom Spieler geänderte Belegung speichern/laden ("key:87,mouse:1" oder "none"): Aktionsname -> Belegung
    std::map<std::string, std::string> exportBindings() const;
    void importBindings(const std::map<std::string, std::string>& saved);
    // Wird nach jeder geänderten Belegung aufgerufen (zum Speichern)
    std::function<void()> onRebound;

    // Skript-Modus: echte Geräte werden ignoriert, nur eingespeiste Aktionen zählen (für automatische Tests)
    void setInjectionMode(bool enabled) { injection_ = enabled; }
    bool injectionMode() const { return injection_; }
    void clearInjected() { injected_.fill(false); }
    void injectAction(Action a, bool isDown) { injected_[index(a)] = isDown; }
    void injectLook(Vector2 radians) { injectedLook_ = radians; }

    float mouseSensitivity = 0.0028f;  // Radiant pro Pixel
    float stickLookRate = 2.6f;        // Radiant pro Sekunde bei vollem Ausschlag
    bool invertY = false;

private:
    static constexpr size_t kCount = static_cast<size_t>(Action::Count);
    static size_t index(Action a) { return static_cast<size_t>(a); }
    static bool bindingDown(const Binding& b);

    std::array<std::vector<Binding>, kCount> bindings_;
    std::array<bool, kCount> cur_{};
    std::array<bool, kCount> prev_{};
    std::array<bool, kCount> injected_{};
    Vector2 move_{0, 0};
    Vector2 mouseDelta_{0, 0};
    Vector2 lookStick_{0, 0};
    Vector2 injectedLook_{0, 0};
    Vector2 latchedInjectedLook_{0, 0};
    bool injection_ = false;

    bool rebinding_ = false;
    Action rebindAction_ = Action::Jump;
    int rebindSlot_ = -1;
    int rebindGrace_ = 0;
    int revision_ = 0;
    std::array<std::vector<Binding>, kCount> defaults_;
    std::map<std::string, std::string> overrides_;   // vom Spieler geänderte Belegungen (Aktionsname -> "key:87,mouse:1")
    // Nach dem Belegen wirkt die Taste, die das beendet hat, erst nach dem Loslassen (sonst löst z. B. Esc gleich das Menü aus)
    bool hasBlocker_ = false;
    Binding blocker_{Binding::Device::Key, 0};
    void finishRebind(Binding b);
    void cancelRebind(const Binding* blockedBy);
    void storeOverride(Action a);
};

}  // namespace aldoria
