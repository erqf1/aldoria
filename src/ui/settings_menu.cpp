#include "ui/settings_menu.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace aldoria {
namespace {

std::string percent(float v) { return std::format("{}%", (int)std::lround(v * 100.0f)); }

MenuItem slider(const std::string& label, float& value, float step, float lo, float hi, std::function<void()> onChange,
                bool asFactor = false) {
    MenuItem item;
    item.label = label;
    item.value = [&value, asFactor]() {
        if (asFactor) return std::format("x{:.2f}", value);
        return percent(value);
    };
    item.onAdjust = [&value, step, lo, hi, onChange](int dir) {
        value = std::clamp(std::round((value + (float)dir * step) / step) * step, lo, hi);
        if (onChange) onChange();
    };
    return item;
}

MenuItem toggle(const std::string& label, bool& value, std::function<void()> onChange) {
    MenuItem item;
    item.label = label;
    item.value = [&value]() { return std::string(value ? "An" : "Aus"); };
    item.onAdjust = [&value, onChange](int) {
        value = !value;
        if (onChange) onChange();
    };
    item.onSelect = [&value, onChange]() {
        value = !value;
        if (onChange) onChange();
    };
    return item;
}

MenuItem choice(const std::string& label, int& value, std::vector<std::string> names, std::function<void()> onChange) {
    MenuItem item;
    item.label = label;
    item.value = [&value, names]() { return names[(size_t)std::clamp(value, 0, (int)names.size() - 1)]; };
    item.onAdjust = [&value, names, onChange](int dir) {
        int n = (int)names.size();
        value = ((value + dir) % n + n) % n;
        if (onChange) onChange();
    };
    item.onSelect = [&value, names, onChange]() {
        value = (value + 1) % (int)names.size();
        if (onChange) onChange();
    };
    return item;
}

}  // namespace

SettingsScreen::SettingsScreen(Settings& settings, Input& input, std::function<void()> onClose, std::function<void()> onChange)
    : settings_(settings), input_(input), onClose_(std::move(onClose)), onChange_(std::move(onChange)) {
    buildMain();
    buildKeys();
    seenRevision_ = input_.revision();
}

void SettingsScreen::setSound(std::function<void(int)> sound) {
    main_.sound = sound;
    keys_.sound = sound;
    action_.sound = sound;
}

void SettingsScreen::open() {
    page_ = Page::Main;
    main_.select(0);
}

void SettingsScreen::goBack() {
    switch (page_) {
        case Page::Action:
            page_ = Page::Keys;
            break;
        case Page::Keys:
            page_ = Page::Main;
            break;
        case Page::Main:
            if (onClose_) onClose_();
            break;
    }
}

void SettingsScreen::buildMain() {
    Settings& s = settings_;
    auto onChange = onChange_;
    std::vector<MenuItem> items;
    items.push_back(choice("Grafik", s.graphicsQuality, {"Niedrig", "Mittel", "Hoch"}, onChange));
    items.push_back(slider("Gesamtlautstärke", s.masterVolume, 0.1f, 0.0f, 1.0f, onChange));
    items.push_back(slider("Musik", s.musicVolume, 0.1f, 0.0f, 1.0f, onChange));
    items.push_back(slider("Effekte", s.sfxVolume, 0.1f, 0.0f, 1.0f, onChange));
    items.push_back(slider("Wackeln und Trefferblitz", s.cameraShake, 0.25f, 0.0f, 1.0f, onChange));
    items.push_back(slider("Mausempfindlichkeit", s.mouseSensitivity, 0.25f, 0.25f, 3.0f, onChange, /*asFactor=*/true));
    items.push_back(toggle("Blick nach oben/unten umkehren", s.invertY, onChange));
    items.push_back(toggle("Steuerungshilfe anzeigen", s.showControlsHelp, onChange));
    MenuItem keys;
    keys.label = "Tasten belegen";
    keys.value = []() { return std::string(">"); };
    keys.onSelect = [this]() {
        buildKeys();
        page_ = Page::Keys;
    };
    items.push_back(std::move(keys));
    MenuItem back;
    back.label = "Zurück";
    back.onSelect = [this]() { goBack(); };
    items.push_back(std::move(back));
    main_.setItems(std::move(items));
}

// Liste aller Aktionen mit ihren Tasten
void SettingsScreen::buildKeys() {
    int sel = keys_.selected();
    std::vector<MenuItem> items;
    for (const RebindableAction& r : rebindableActions()) {
        MenuItem item;
        item.label = r.label;
        Action action = r.action;
        item.value = [this, action]() {
            std::string text = input_.bindingNames(action);
            return input_.isDefault(action) ? text : text + " *";
        };
        item.onSelect = [this, action]() {
            editing_ = action;
            buildAction();
            page_ = Page::Action;
        };
        items.push_back(std::move(item));
    }
    MenuItem reset;
    reset.label = "Alle Tasten auf Standard";
    reset.onSelect = [this]() {
        input_.resetAllBindings();
        if (onChange_) onChange_();
    };
    items.push_back(std::move(reset));
    MenuItem back;
    back.label = "Zurück";
    back.onSelect = [this]() { goBack(); };
    items.push_back(std::move(back));
    keys_.setItems(std::move(items));
    keys_.select(sel);
    keys_.hint = "Aktion wählen · Enter ändert · * = geändert · Esc zurück";
}

// Die Tasten einer Aktion: jede Zeile ersetzt ihre Taste, "Weitere Taste" hängt eine an
void SettingsScreen::buildAction() {
    int sel = action_.selected();
    const Action a = editing_;
    std::vector<MenuItem> items;
    std::vector<Binding> list = input_.userBindings(a);
    for (size_t i = 0; i < list.size(); i++) {
        MenuItem item;
        item.label = std::format("Taste {}", i + 1);
        int slot = (int)i;
        item.value = [this, a, slot, i]() {
            if (input_.rebinding() && input_.rebindAction() == a && input_.rebindSlot() == slot) return std::string("... Taste drücken ...");
            std::vector<Binding> now = input_.userBindings(a);
            return i < now.size() ? Input::bindingLabel(now[i]) : std::string("-");
        };
        item.onSelect = [this, a, slot]() { input_.startRebind(a, slot); };
        items.push_back(std::move(item));
    }
    MenuItem add;
    add.label = "Weitere Taste hinzufügen";
    add.enabled = [this, a]() { return (int)input_.userBindings(a).size() < Input::kMaxUserBindings; };
    add.value = [this, a]() {
        if (input_.rebinding() && input_.rebindAction() == a && input_.rebindSlot() < 0) return std::string("... Taste drücken ...");
        return std::string();
    };
    add.onSelect = [this, a]() { input_.startRebind(a, -1); };
    items.push_back(std::move(add));
    MenuItem reset;
    reset.label = "Diese Aktion auf Standard";
    reset.enabled = [this, a]() { return !input_.isDefault(a); };
    reset.onSelect = [this, a]() {
        input_.resetAction(a);
        if (onChange_) onChange_();
    };
    items.push_back(std::move(reset));
    MenuItem back;
    back.label = "Zurück";
    back.onSelect = [this]() { goBack(); };
    items.push_back(std::move(back));
    action_.setItems(std::move(items));
    action_.select(sel);
    action_.hint = "Zeile wählen, neue Taste drücken · Entf löscht · Esc bricht ab";
}

void SettingsScreen::update(const Input& in) {
    // Änderungen der Belegung (auch durch das Belegen selbst) bauen die Listen neu auf
    if (seenRevision_ != input_.revision()) {
        seenRevision_ = input_.revision();
        buildKeys();
        if (page_ == Page::Action) buildAction();
    }
    current().update(in);
    if (in.pressed(Action::UiBack)) goBack();
}

void SettingsScreen::draw(float centerY, float time, float centerX) const {
    switch (page_) {
        case Page::Main: main_.draw("Einstellungen", centerY, time, centerX); break;
        case Page::Keys: keys_.draw("Tasten belegen", centerY, time, centerX); break;
        case Page::Action: {
            std::string title = "Belegung: ";
            for (const RebindableAction& r : rebindableActions()) {
                if (r.action == editing_) title += r.label;
            }
            Menu copy = action_;
            std::vector<std::string> shared = input_.sharedWith(editing_);
            if (!shared.empty()) {
                std::string text = "Auch belegt bei: ";
                for (size_t i = 0; i < shared.size(); i++) text += (i ? ", " : "") + shared[i];
                copy.hint = text + " (beides löst aus)";
            }
            copy.draw(title, centerY, time, centerX);
            break;
        }
    }
}

}  // namespace aldoria
