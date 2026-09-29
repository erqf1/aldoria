#include "ui/menu.h"

#include <algorithm>
#include <cmath>

#include "raylib.h"

namespace aldoria {

void Menu::setItems(std::vector<MenuItem> items) {
    items_ = std::move(items);
    selected_ = 0;
    if (!items_.empty() && !isEnabled(0)) move(1);
}

bool Menu::isEnabled(int index) const {
    if (index < 0 || index >= (int)items_.size()) return false;
    return !items_[(size_t)index].enabled || items_[(size_t)index].enabled();
}

void Menu::select(int index) {
    if (isEnabled(index)) selected_ = index;
}

void Menu::move(int dir) {
    if (items_.empty()) return;
    int n = (int)items_.size();
    for (int i = 1; i <= n; i++) {
        int candidate = ((selected_ + dir * i) % n + n) % n;
        if (isEnabled(candidate)) {
            selected_ = candidate;
            if (sound) sound(0);
            return;
        }
    }
}

void Menu::activate() {
    if (isEnabled(selected_) && items_[(size_t)selected_].onSelect) {
        if (sound) sound(1);
        items_[(size_t)selected_].onSelect();
    }
}

void Menu::update(const Input& input) {
    if (items_.empty()) return;
    if (input.pressed(Action::UiUp)) move(-1);
    if (input.pressed(Action::UiDown)) move(1);

    MenuItem& item = items_[(size_t)selected_];
    if (item.onAdjust) {
        if (input.pressed(Action::UiLeft)) {
            item.onAdjust(-1);
            if (sound) sound(0);
        }
        if (input.pressed(Action::UiRight)) {
            item.onAdjust(1);
            if (sound) sound(0);
        }
    }
    if (input.pressed(Action::UiConfirm)) activate();
}

void Menu::draw(const std::string& title, float centerY, float time, float centerX) const {
    const int rowH = 46;
    const int w = 620;
    // Lange Listen (Einstellungen mit den Tasten) scrollen: es passen so viele Zeilen wie das Fenster hergibt
    int maxRows = std::max(4, (GetScreenHeight() - 150) / rowH);
    int total = (int)items_.size();
    int visible = std::min(total, maxRows);
    int first = std::clamp(selected_ - visible / 2, 0, std::max(0, total - visible));
    int h = visible * rowH + 90;
    int x = centerX < 0.0f ? (GetScreenWidth() - w) / 2 : std::clamp((int)centerX - w / 2, 8, std::max(8, GetScreenWidth() - w - 8));
    int y = std::max(6, (int)centerY - h / 2);
    if (total > visible) y = std::max(6, (GetScreenHeight() - h) / 2);

    DrawRectangle(x, y, w, h, Color{16, 20, 32, 230});
    DrawRectangleLinesEx(Rectangle{(float)x, (float)y, (float)w, (float)h}, 3, Color{214, 196, 140, 255});
    DrawRectangleLinesEx(Rectangle{(float)x + 5, (float)y + 5, (float)w - 10, (float)h - 10}, 1, Color{90, 90, 130, 255});

    int tw = MeasureText(title.c_str(), 30);
    DrawText(title.c_str(), x + (w - tw) / 2 + 2, y + 22, 30, Color{0, 0, 0, 200});
    DrawText(title.c_str(), x + (w - tw) / 2, y + 20, 30, Color{250, 226, 150, 255});

    if (first > 0) DrawText("^", x + w - 34, y + 22, 24, Color{200, 200, 230, 255});
    if (first + visible < total) DrawText("v", x + w - 34, y + h - 32, 24, Color{200, 200, 230, 255});
    for (size_t i = (size_t)first; i < (size_t)(first + visible); i++) {
        const MenuItem& item = items_[i];
        bool enabled = isEnabled((int)i);
        bool sel = (int)i == selected_;
        int ry = y + 76 + (int)(i - (size_t)first) * rowH;
        if (sel) {
            float pulse = 0.5f + 0.5f * std::sin(time * 5.0f);
            DrawRectangle(x + 16, ry - 6, w - 32, rowH - 6, Color{60, 78, 150, (unsigned char)(170 + 40 * pulse)});
        }
        Color c = !enabled ? Color{110, 112, 130, 255} : (sel ? WHITE : Color{206, 208, 224, 255});
        DrawText(item.label.c_str(), x + 40, ry, 20, c);
        if (item.value) {
            std::string v = item.value();
            if (sel && item.onAdjust) v = "<  " + v + "  >";
            int vw = MeasureText(v.c_str(), 20);
            DrawText(v.c_str(), x + w - 40 - vw, ry, 20, c);
        }
    }
    if (!hint.empty()) {
        int hw = MeasureText(hint.c_str(), 16);
        DrawText(hint.c_str(), x + (w - hw) / 2 + 1, y + h + 13, 16, Color{0, 0, 0, 200});
        DrawText(hint.c_str(), x + (w - hw) / 2, y + h + 12, 16, Color{226, 230, 244, 255});
    }
}

}  // namespace aldoria
