#pragma once

#include "audio/audio_manager.h"
#include "core/event_bus.h"
#include "core/input.h"
#include "core/settings.h"
#include "core/time_control.h"
#include "render/lit_renderer.h"

namespace aldoria {

// Gemeinsame Dienste, die jede Szene braucht. Die App besitzt sie, Szenen bekommen eine Referenz.
struct GameContext {
    Input input;
    EventBus events;
    TimeControl time;
    Settings settings;
    LitRenderer renderer;
    AudioManager audio;
    bool mute = false;  // Testmodus: keine Töne
};

}  // namespace aldoria
