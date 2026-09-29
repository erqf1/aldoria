#include "core/input.h"

#include <cmath>
#include <format>

namespace aldoria {
namespace {

constexpr float kStickDeadzone = 0.2f;

Binding key(int k) { return {Binding::Device::Key, k}; }
Binding mouse(int b) { return {Binding::Device::Mouse, b}; }
Binding pad(int b) { return {Binding::Device::GamepadButton, b}; }

Vector2 readStick(int axisX, int axisY) {
    if (!IsGamepadAvailable(0)) return {0, 0};
    Vector2 v{GetGamepadAxisMovement(0, axisX), GetGamepadAxisMovement(0, axisY)};
    float len = std::sqrt(v.x * v.x + v.y * v.y);
    if (len < kStickDeadzone) return {0, 0};
    if (len > 1.0f) return {v.x / len, v.y / len};
    return v;
}

}  // namespace

bool actionFromName(const std::string& name, Action& out) {
    struct Entry {
        const char* name;
        Action action;
    };
    static const Entry table[] = {
        {"forward", Action::MoveForward}, {"back", Action::MoveBack},     {"left", Action::MoveLeft},
        {"right", Action::MoveRight},     {"jump", Action::Jump},         {"sprint", Action::Sprint},
        {"attack", Action::Attack},       {"secondary", Action::Secondary}, {"ability", Action::Ability},
        {"interact", Action::Interact},   {"dodge", Action::Dodge},       {"dash", Action::Dash},
        {"heal", Action::Heal},           {"pause", Action::Pause},       {"map", Action::Map},
        {"inventory", Action::Inventory}, {"lockon", Action::LockOn},     {"up", Action::UiUp},
        {"down", Action::UiDown},         {"ui_left", Action::UiLeft},    {"ui_right", Action::UiRight},
        {"confirm", Action::UiConfirm},   {"cancel", Action::UiBack},
    };
    for (const Entry& e : table) {
        if (name == e.name) {
            out = e.action;
            return true;
        }
    }
    return false;
}


const char* actionName(Action a) {
    switch (a) {
        case Action::MoveForward: return "forward";
        case Action::MoveBack: return "back";
        case Action::MoveLeft: return "left";
        case Action::MoveRight: return "right";
        case Action::Jump: return "jump";
        case Action::Sprint: return "sprint";
        case Action::Attack: return "attack";
        case Action::Secondary: return "secondary";
        case Action::Ability: return "ability";
        case Action::Interact: return "interact";
        case Action::Dodge: return "dodge";
        case Action::Dash: return "dash";
        case Action::Heal: return "heal";
        case Action::Pause: return "pause";
        case Action::Map: return "map";
        case Action::Inventory: return "inventory";
        case Action::LockOn: return "lockon";
        case Action::UiUp: return "up";
        case Action::UiDown: return "down";
        case Action::UiLeft: return "ui_left";
        case Action::UiRight: return "ui_right";
        case Action::UiConfirm: return "confirm";
        case Action::UiBack: return "cancel";
        case Action::Count: break;
    }
    return "";
}

const std::vector<RebindableAction>& rebindableActions() {
    static const std::vector<RebindableAction> list = {
        {Action::MoveForward, "Vorwärts"}, {Action::MoveBack, "Rückwärts"}, {Action::MoveLeft, "Links"}, {Action::MoveRight, "Rechts"},
        {Action::Jump, "Springen"},        {Action::Sprint, "Sprinten"},    {Action::Attack, "Schwert"}, {Action::Secondary, "Funken"},
        {Action::Dodge, "Ausweichen"},     {Action::Dash, "Dash"},          {Action::Heal, "Heiltrank"}, {Action::Interact, "Benutzen"},
        {Action::Ability, "Segen kaufen"}, {Action::Map, "Karte"},
    };
    return list;
}

namespace {

std::string keyName(int k) {
    if (k >= KEY_A && k <= KEY_Z) return std::string(1, (char)('A' + (k - KEY_A)));
    if (k >= KEY_ZERO && k <= KEY_NINE) return std::string(1, (char)('0' + (k - KEY_ZERO)));
    if (k >= KEY_F1 && k <= KEY_F12) return std::format("F{}", k - KEY_F1 + 1);
    switch (k) {
        case KEY_SPACE: return "Leertaste";
        case KEY_ENTER: return "Enter";
        case KEY_TAB: return "Tab";
        case KEY_BACKSPACE: return "Rücktaste";
        case KEY_ESCAPE: return "Esc";
        case KEY_UP: return "Pfeil hoch";
        case KEY_DOWN: return "Pfeil runter";
        case KEY_LEFT: return "Pfeil links";
        case KEY_RIGHT: return "Pfeil rechts";
        case KEY_LEFT_SHIFT: return "Shift links";
        case KEY_RIGHT_SHIFT: return "Shift rechts";
        case KEY_LEFT_CONTROL: return "Strg links";
        case KEY_RIGHT_CONTROL: return "Strg rechts";
        case KEY_LEFT_ALT: return "Alt links";
        case KEY_RIGHT_ALT: return "Alt rechts";
        case KEY_COMMA: return ",";
        case KEY_PERIOD: return ".";
        case KEY_MINUS: return "-";
        case KEY_KP_0: return "Num 0";
        case KEY_INSERT: return "Einfg";
        case KEY_DELETE: return "Entf";
        case KEY_HOME: return "Pos1";
        case KEY_END: return "Ende";
        case KEY_PAGE_UP: return "Bild hoch";
        case KEY_PAGE_DOWN: return "Bild runter";
        default: break;
    }
    return std::format("Taste {}", k);
}

std::string bindingText(const Binding& b) {
    switch (b.device) {
        case Binding::Device::Key: return keyName(b.code);
        case Binding::Device::Mouse:
            switch (b.code) {
                case MOUSE_BUTTON_LEFT: return "Maus links";
                case MOUSE_BUTTON_RIGHT: return "Maus rechts";
                case MOUSE_BUTTON_MIDDLE: return "Maus Mitte";
                default: return std::format("Maus {}", b.code + 1);
            }
        case Binding::Device::GamepadButton: return std::format("Pad {}", b.code);
    }
    return "";
}

std::string bindingCode(const Binding& b) {
    return std::format("{}:{}", b.device == Binding::Device::Key ? "key" : (b.device == Binding::Device::Mouse ? "mouse" : "pad"), b.code);
}

bool parseBinding(const std::string& text, Binding& out) {
    size_t colon = text.find(':');
    if (colon == std::string::npos) return false;
    std::string kind = text.substr(0, colon);
    int code = 0;
    try {
        code = std::stoi(text.substr(colon + 1));
    } catch (...) {
        return false;
    }
    if (kind == "key") out = {Binding::Device::Key, code};
    else if (kind == "mouse") out = {Binding::Device::Mouse, code};
    else if (kind == "pad") out = {Binding::Device::GamepadButton, code};
    else return false;
    return true;
}

}  // namespace

namespace {

bool sameBinding(const Binding& a, const Binding& b) { return a.device == b.device && a.code == b.code; }

bool isRebindable(Action a) {
    for (const RebindableAction& r : rebindableActions())
        if (r.action == a) return true;
    return false;
}

}  // namespace

std::string Input::bindingLabel(const Binding& b) { return bindingText(b); }

std::string Input::bindingName(Action a) const {
    const auto& list = bindings_[index(a)];
    for (const Binding& b : list) {
        if (b.device != Binding::Device::GamepadButton) return bindingText(b);
    }
    return list.empty() ? std::string("-") : bindingText(list.front());
}

std::vector<Binding> Input::userBindings(Action a) const {
    std::vector<Binding> out;
    for (const Binding& b : bindings_[index(a)]) {
        if (b.device != Binding::Device::GamepadButton) out.push_back(b);
    }
    return out;
}

std::string Input::bindingNames(Action a) const {
    std::string text;
    for (const Binding& b : userBindings(a)) {
        if (!text.empty()) text += ", ";
        text += bindingText(b);
    }
    return text.empty() ? std::string("-") : text;
}

void Input::storeOverride(Action a) {
    const std::string name = actionName(a);
    std::vector<Binding> now = userBindings(a), def;
    for (const Binding& b : defaults_[index(a)]) {
        if (b.device != Binding::Device::GamepadButton) def.push_back(b);
    }
    bool same = now.size() == def.size();
    for (size_t i = 0; same && i < now.size(); i++) same = sameBinding(now[i], def[i]);
    if (same) {
        overrides_.erase(name);
        return;
    }
    std::string text;
    for (const Binding& b : now) text += (text.empty() ? "" : ",") + bindingCode(b);
    overrides_[name] = text.empty() ? std::string("none") : text;
}

void Input::setUserBindings(Action a, std::vector<Binding> list) {
    if (!isRebindable(a)) return;
    std::vector<Binding> next;
    for (const Binding& b : list) {
        if (b.device == Binding::Device::GamepadButton) continue;
        bool dup = false;
        for (const Binding& x : next) dup = dup || sameBinding(x, b);
        if (dup || (int)next.size() >= kMaxUserBindings) continue;
        next.push_back(b);
    }
    for (const Binding& x : bindings_[index(a)]) {
        if (x.device == Binding::Device::GamepadButton) next.push_back(x);
    }
    bindings_[index(a)] = std::move(next);
    storeOverride(a);
    revision_++;
    if (onRebound) onRebound();
}

void Input::removeUserBinding(Action a, int slot) {
    std::vector<Binding> list = userBindings(a);
    if (slot < 0 || slot >= (int)list.size()) return;
    list.erase(list.begin() + slot);
    setUserBindings(a, std::move(list));
}

void Input::resetAction(Action a) {
    if (!isRebindable(a)) return;
    bindings_[index(a)] = defaults_[index(a)];
    overrides_.erase(actionName(a));
    revision_++;
    if (onRebound) onRebound();
}

void Input::resetAllBindings() {
    setDefaultBindings();
    revision_++;
    if (onRebound) onRebound();
}

bool Input::isDefault(Action a) const { return overrides_.find(actionName(a)) == overrides_.end(); }

std::vector<std::string> Input::sharedWith(Action a) const {
    std::vector<std::string> out;
    std::vector<Binding> mine = userBindings(a);
    for (const RebindableAction& r : rebindableActions()) {
        if (r.action == a) continue;
        bool shared = false;
        for (const Binding& b : userBindings(r.action))
            for (const Binding& m : mine) shared = shared || sameBinding(b, m);
        if (shared) out.push_back(r.label);
    }
    return out;
}

void Input::startRebind(Action a, int slot) {
    if (injection_) return;
    rebinding_ = true;
    rebindAction_ = a;
    rebindSlot_ = slot;
    rebindGrace_ = 2;   // die Taste, mit der man den Menüpunkt ausgewählt hat, zählt nicht
    while (GetKeyPressed() != 0) {}
    while (GetCharPressed() != 0) {}
}

void Input::cancelRebind(const Binding* blockedBy) {
    rebinding_ = false;
    if (blockedBy) {
        hasBlocker_ = true;
        blocker_ = *blockedBy;
    }
}

void Input::finishRebind(Binding b) {
    std::vector<Binding> list = userBindings(rebindAction_);
    if (rebindSlot_ >= 0 && rebindSlot_ < (int)list.size()) list[(size_t)rebindSlot_] = b;
    else list.push_back(b);
    cancelRebind(&b);
    setUserBindings(rebindAction_, std::move(list));
}

std::map<std::string, std::string> Input::exportBindings() const { return overrides_; }

void Input::importBindings(const std::map<std::string, std::string>& saved) {
    for (const auto& [name, code] : saved) {
        Action a;
        if (!actionFromName(name, a) || !isRebindable(a)) continue;
        std::vector<Binding> list;
        bool ok = true;
        if (code != "none") {
            size_t pos = 0;
            while (pos <= code.size()) {
                size_t comma = code.find(',', pos);
                std::string part = code.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
                Binding b;
                if (!parseBinding(part, b)) {
                    ok = false;
                    break;
                }
                list.push_back(b);
                if (comma == std::string::npos) break;
                pos = comma + 1;
            }
        }
        if (!ok) continue;
        std::vector<Binding> next;
        for (const Binding& b : list) {
            bool dup = false;
            for (const Binding& x : next) dup = dup || sameBinding(x, b);
            if (!dup && b.device != Binding::Device::GamepadButton && (int)next.size() < kMaxUserBindings) next.push_back(b);
        }
        for (const Binding& x : bindings_[index(a)]) {
            if (x.device == Binding::Device::GamepadButton) next.push_back(x);
        }
        bindings_[index(a)] = std::move(next);
        storeOverride(a);
    }
    revision_++;
}

Input::Input() { setDefaultBindings(); }

void Input::setDefaultBindings() {
    overrides_.clear();
    rebind(Action::MoveForward, {key(KEY_W), key(KEY_UP)});
    rebind(Action::MoveBack, {key(KEY_S), key(KEY_DOWN)});
    rebind(Action::MoveLeft, {key(KEY_A), key(KEY_LEFT)});
    rebind(Action::MoveRight, {key(KEY_D), key(KEY_RIGHT)});
    rebind(Action::Jump, {key(KEY_SPACE), pad(GAMEPAD_BUTTON_RIGHT_FACE_DOWN)});
    rebind(Action::Sprint, {key(KEY_LEFT_SHIFT), pad(GAMEPAD_BUTTON_LEFT_THUMB)});
    rebind(Action::Attack, {mouse(MOUSE_BUTTON_LEFT), pad(GAMEPAD_BUTTON_RIGHT_FACE_LEFT)});
    rebind(Action::Secondary, {mouse(MOUSE_BUTTON_RIGHT), pad(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)});
    rebind(Action::Ability, {key(KEY_Q), pad(GAMEPAD_BUTTON_LEFT_TRIGGER_1)});
    rebind(Action::Interact, {key(KEY_E), pad(GAMEPAD_BUTTON_RIGHT_FACE_UP)});
    rebind(Action::Dodge, {key(KEY_LEFT_CONTROL), pad(GAMEPAD_BUTTON_RIGHT_TRIGGER_1)});
    rebind(Action::Dash, {key(KEY_LEFT_ALT), key(KEY_V), pad(GAMEPAD_BUTTON_RIGHT_TRIGGER_2)});
    rebind(Action::Heal, {key(KEY_R), pad(GAMEPAD_BUTTON_LEFT_FACE_UP)});
    rebind(Action::Pause, {key(KEY_ESCAPE), pad(GAMEPAD_BUTTON_MIDDLE_RIGHT)});
    rebind(Action::Map, {key(KEY_M), pad(GAMEPAD_BUTTON_MIDDLE_LEFT)});
    rebind(Action::Inventory, {key(KEY_I)});
    rebind(Action::LockOn, {mouse(MOUSE_BUTTON_MIDDLE), pad(GAMEPAD_BUTTON_RIGHT_THUMB)});

    rebind(Action::UiUp, {key(KEY_W), key(KEY_UP), pad(GAMEPAD_BUTTON_LEFT_FACE_UP)});
    rebind(Action::UiDown, {key(KEY_S), key(KEY_DOWN), pad(GAMEPAD_BUTTON_LEFT_FACE_DOWN)});
    rebind(Action::UiLeft, {key(KEY_A), key(KEY_LEFT), pad(GAMEPAD_BUTTON_LEFT_FACE_LEFT)});
    rebind(Action::UiRight, {key(KEY_D), key(KEY_RIGHT), pad(GAMEPAD_BUTTON_LEFT_FACE_RIGHT)});
    rebind(Action::UiConfirm, {key(KEY_ENTER), key(KEY_SPACE), key(KEY_E), pad(GAMEPAD_BUTTON_RIGHT_FACE_DOWN)});
    rebind(Action::UiBack, {key(KEY_ESCAPE), key(KEY_BACKSPACE), pad(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)});
    defaults_ = bindings_;
}

bool Input::bindingDown(const Binding& b) {
    switch (b.device) {
        case Binding::Device::Key: return IsKeyDown(b.code);
        case Binding::Device::Mouse: return IsMouseButtonDown(b.code);
        case Binding::Device::GamepadButton: return IsGamepadAvailable(0) && IsGamepadButtonDown(0, b.code);
    }
    return false;
}

void Input::update() {
    prev_ = cur_;
    if (rebinding_ && !injection_) {
        cur_.fill(false);   // solange eine Taste aufgenommen wird, reagiert nichts anderes
        if (rebindGrace_ > 0) {
            rebindGrace_--;
            while (GetKeyPressed() != 0) {}
        } else {
            int k = GetKeyPressed();
            if (k == KEY_ESCAPE) {
                Binding esc{Binding::Device::Key, KEY_ESCAPE};
                cancelRebind(&esc);
            } else if (k == KEY_DELETE && rebindSlot_ >= 0) {
                Binding del{Binding::Device::Key, KEY_DELETE};
                int slot = rebindSlot_;
                Action a = rebindAction_;
                cancelRebind(&del);
                removeUserBinding(a, slot);
            } else if (k != 0) {
                finishRebind({Binding::Device::Key, k});
            } else {
                for (int m : {MOUSE_BUTTON_LEFT, MOUSE_BUTTON_RIGHT, MOUSE_BUTTON_MIDDLE, MOUSE_BUTTON_SIDE, MOUSE_BUTTON_EXTRA}) {
                    if (IsMouseButtonPressed(m)) {
                        finishRebind({Binding::Device::Mouse, m});
                        break;
                    }
                }
            }
        }
        move_ = {0, 0};
        mouseDelta_ = {0, 0};
        lookStick_ = {0, 0};
        return;
    }
    if (hasBlocker_ && !injection_) {
        if (bindingDown(blocker_)) {
            cur_.fill(false);
            move_ = {0, 0};
            mouseDelta_ = GetMouseDelta();
            lookStick_ = {0, 0};
            return;
        }
        hasBlocker_ = false;
    }
    for (size_t i = 0; i < kCount; i++) {
        bool isDown = false;
        if (injection_) {
            isDown = injected_[i];
        } else {
            for (const Binding& b : bindings_[i]) {
                if (bindingDown(b)) {
                    isDown = true;
                    break;
                }
            }
        }
        cur_[i] = isDown;
    }

    Vector2 keys{(down(Action::MoveRight) ? 1.0f : 0.0f) - (down(Action::MoveLeft) ? 1.0f : 0.0f),
                 (down(Action::MoveForward) ? 1.0f : 0.0f) - (down(Action::MoveBack) ? 1.0f : 0.0f)};
    float keyLen = std::sqrt(keys.x * keys.x + keys.y * keys.y);
    if (keyLen > 1.0f) keys = {keys.x / keyLen, keys.y / keyLen};

    if (injection_) {
        move_ = keys;
        mouseDelta_ = {0, 0};
        lookStick_ = {0, 0};
        latchedInjectedLook_ = injectedLook_;
        injectedLook_ = {0, 0};
        return;
    }

    Vector2 stick = readStick(GAMEPAD_AXIS_LEFT_X, GAMEPAD_AXIS_LEFT_Y);
    stick.y = -stick.y;  // Stick nach oben ist negativ
    move_ = (stick.x != 0.0f || stick.y != 0.0f) ? stick : keys;

    mouseDelta_ = GetMouseDelta();
    lookStick_ = readStick(GAMEPAD_AXIS_RIGHT_X, GAMEPAD_AXIS_RIGHT_Y);
}

Vector2 Input::lookDelta(float dt, bool mouseActive) const {
    if (injection_) return latchedInjectedLook_;
    Vector2 look{0, 0};
    if (mouseActive) {
        look.x += mouseDelta_.x * mouseSensitivity;
        look.y += mouseDelta_.y * mouseSensitivity;
    }
    look.x += lookStick_.x * stickLookRate * dt;
    look.y += lookStick_.y * stickLookRate * dt;
    if (invertY) look.y = -look.y;
    return look;
}

}  // namespace aldoria
