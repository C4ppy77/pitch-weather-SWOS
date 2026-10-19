// pitch-weather: rain, drizzle and snow drawn over a 2D football game with SDL2.
// Copyright (c) 2026 C4ppy77. MIT licence, see LICENSE.
#pragma once

#include <SDL.h>

namespace PitchWeather {

enum class Kind { None, Drizzle, Rain, HeavyRain, Snow };

struct Weather
{
    Kind kind = Kind::None;
    float intensity = 0;    // 0..1, how much is falling right now
};

// The weather for a SWOS-style pitch type: 0 frozen (snow), 1 muddy (heavy rain), 2 wet (rain), 3 soft (drizzle),
// anything else none. Its intensity drifts over the match in two slow waves, in steps of 0.1; seed (0..359) gives each
// match its own pattern, minute is the match minute (0..90).
Weather forPitchType(int pitchType, float minute, int seed);

// Draws the weather over area (usually the whole game screen), with a slight tint for the gloom or the cold. Call it
// once a frame after drawing the game and before presenting; seconds is any steadily rising clock (e.g.
// SDL_GetTicks64() / 1000.0). Needs SDL 2.0.18 or later (SDL_RenderGeometry).
void draw(SDL_Renderer *renderer, const SDL_Rect& area, const Weather& weather, double seconds);

}
