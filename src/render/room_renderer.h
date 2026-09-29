#pragma once

#include <string>
#include <vector>

#include "raylib.h"
#include "render/lit_renderer.h"
#include "world/level.h"
#include "world/room.h"

namespace aldoria {

// Zeichnet einen Raum: Boden, Wände, Türen, Kristalle, Aufsammelbares, Schrein, Gefahren und Dekoration.
// `room` darf leer sein (z. B. für den Titelhintergrund); dann fehlen dynamische Dinge wie Pickups.
struct RoomDrawInfo {
    const Level* level = nullptr;
    const Room* room = nullptr;
    float time = 0.0f;
    bool checkpointActive = false;
};

void drawRoom(const LitRenderer& renderer, const RoomDrawInfo& info);

// Einzelne Dekorationsobjekte (auch für Tests der Formen nutzbar)
void drawDecor(const LitRenderer& renderer, const LevelDecor& decor, float time);

// Die `maxCount` wichtigsten Punktlichter des Raums um `focus` (nächste zuerst), mit Flackern zur Zeit `time`
std::vector<PointLight> collectLights(const Level& level, Vector3 focus, float time, size_t maxCount);

}  // namespace aldoria
