#include "core/event_bus.h"
#include "core/events.h"
#include "doctest.h"

using namespace aldoria;

TEST_CASE("Event-Bus liefert Ereignisse nur an passende Abonnenten") {
    EventBus bus;
    int landed = 0, jumped = 0;
    float lastImpact = 0;
    bus.subscribe<PlayerLanded>([&](const PlayerLanded& e) {
        landed++;
        lastImpact = e.impactSpeed;
    });
    bus.subscribe<PlayerJumped>([&](const PlayerJumped&) { jumped++; });

    bus.emit(PlayerLanded{7.5f});
    CHECK(landed == 1);
    CHECK(jumped == 0);
    CHECK(lastImpact == doctest::Approx(7.5f));

    bus.emit(PlayerJumped{});
    CHECK(jumped == 1);
}

TEST_CASE("Abmelden stoppt die Zustellung") {
    EventBus bus;
    int count = 0;
    SubscriptionId id = bus.subscribe<PlayerJumped>([&](const PlayerJumped&) { count++; });
    bus.emit(PlayerJumped{});
    bus.unsubscribe(id);
    bus.emit(PlayerJumped{});
    CHECK(count == 1);
}

TEST_CASE("Ein Handler darf sich während emit() abmelden") {
    EventBus bus;
    int count = 0;
    SubscriptionId id = 0;
    id = bus.subscribe<PlayerJumped>([&](const PlayerJumped&) {
        count++;
        bus.unsubscribe(id);
    });
    bus.emit(PlayerJumped{});
    bus.emit(PlayerJumped{});
    CHECK(count == 1);
}

TEST_CASE("emit ohne Abonnenten ist harmlos") {
    EventBus bus;
    bus.emit(LevelLoaded{"x"});
    CHECK(true);
}
