#pragma once

#include <cstdint>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace aldoria {

using SubscriptionId = std::uint64_t;

// Typisierter Event-Bus: Systeme melden Ereignisse (z. B. "Spieler ist gelandet"), ohne einander zu kennen.
// Beim emit() wird eine Kopie der Handlerliste durchlaufen, damit sich Handler währenddessen
// an- oder abmelden dürfen. Ein im selben emit() abgemeldeter Handler wird noch einmal aufgerufen.
class EventBus {
public:
    template <class E>
    SubscriptionId subscribe(std::function<void(const E&)> handler) {
        SubscriptionId id = ++nextId_;
        handlers_[std::type_index(typeid(E))].push_back(
            Entry{id, [h = std::move(handler)](const void* event) { h(*static_cast<const E*>(event)); }});
        return id;
    }

    void unsubscribe(SubscriptionId id) {
        for (auto& [type, list] : handlers_) {
            std::erase_if(list, [id](const Entry& e) { return e.id == id; });
        }
    }

    template <class E>
    void emit(const E& event) const {
        auto it = handlers_.find(std::type_index(typeid(E)));
        if (it == handlers_.end()) return;
        auto copy = it->second;
        for (const auto& entry : copy) entry.fn(&event);
    }

private:
    struct Entry {
        SubscriptionId id;
        std::function<void(const void*)> fn;
    };
    std::unordered_map<std::type_index, std::vector<Entry>> handlers_;
    SubscriptionId nextId_ = 0;
};

}  // namespace aldoria
