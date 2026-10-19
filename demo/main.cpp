// pitch-weather demo: a plain striped pitch with the weather over it.
// Copyright (c) 2026 C4ppy77. MIT licence, see LICENSE.
//
// Keys: 1 drizzle, 2 rain, 3 heavy rain, 4 snow, 0 none, Up/Down intensity, M match mode (the intensity drifts as
// over a 90 minute match, played in 90 seconds), Esc quit. Start with another kind: pitch_weather_demo drizzle|rain|heavy|snow
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <cstdio>
#include <initializer_list>
#include "pitchWeather.h"

using namespace PitchWeather;

static void drawPitch(SDL_Renderer *renderer, int width, int height)
{
    constexpr int kStripes = 12;
    for (int i = 0; i < kStripes; i++) {
        if (i % 2)
            SDL_SetRenderDrawColor(renderer, 0x3c, 0x8c, 0x3c, 255);
        else
            SDL_SetRenderDrawColor(renderer, 0x44, 0x9a, 0x44, 255);
        SDL_Rect stripe{ i * width / kStripes, 0, (i + 1) * width / kStripes - i * width / kStripes, height };
        SDL_RenderFillRect(renderer, &stripe);
    }

    SDL_SetRenderDrawColor(renderer, 230, 240, 230, 255);
    SDL_Rect lines{ width / 16, height / 10, width * 7 / 8, height * 4 / 5 };
    SDL_RenderDrawRect(renderer, &lines);
    SDL_RenderDrawLine(renderer, width / 2, lines.y, width / 2, lines.y + lines.h);
}

static const char *kindName(Kind kind)
{
    switch (kind) {
    case Kind::Drizzle: return "drizzle";
    case Kind::Rain: return "rain";
    case Kind::HeavyRain: return "heavy rain";
    case Kind::Snow: return "snow";
    default: return "none";
    }
}

int main(int argc, char **argv)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    auto window = SDL_CreateWindow("pitch-weather", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    auto renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC) : nullptr;
    if (!renderer) {
        std::fprintf(stderr, "Couldn't create the window: %s\n", SDL_GetError());
        return 1;
    }

    Weather weather{ Kind::Rain, 0.8f };
    if (argc > 1) {
        for (auto kind : { Kind::Drizzle, Kind::Rain, Kind::HeavyRain, Kind::Snow })
            if (SDL_strncasecmp(argv[1], kindName(kind), SDL_strlen(argv[1])) == 0)
                weather.kind = kind;
    }
    bool matchMode = false;
    Uint64 matchStart = 0;
    int seed = static_cast<int>(SDL_GetPerformanceCounter() % 360);

    for (bool running = true; running; ) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE: running = false; break;
                case SDLK_0: weather.kind = Kind::None; break;
                case SDLK_1: weather.kind = Kind::Drizzle; break;
                case SDLK_2: weather.kind = Kind::Rain; break;
                case SDLK_3: weather.kind = Kind::HeavyRain; break;
                case SDLK_4: weather.kind = Kind::Snow; break;
                case SDLK_UP: weather.intensity = SDL_min(1.0f, weather.intensity + 0.1f); break;
                case SDLK_DOWN: weather.intensity = SDL_max(0.0f, weather.intensity - 0.1f); break;
                case SDLK_m: matchMode = !matchMode; matchStart = SDL_GetTicks64(); break;
                }
            }
        }

        int width, height;
        SDL_GetRendererOutputSize(renderer, &width, &height);
        double seconds = SDL_GetTicks64() / 1000.0;

        auto shown = weather;
        if (matchMode) {
            // pitch types as forPitchType takes them
            int pitchType = weather.kind == Kind::Snow ? 0 : weather.kind == Kind::HeavyRain ? 1 :
                weather.kind == Kind::Rain ? 2 : weather.kind == Kind::Drizzle ? 3 : -1;
            float minute = static_cast<float>((SDL_GetTicks64() - matchStart) % 90000) / 1000;
            shown = forPitchType(pitchType, minute, seed);
        }

        char title[128];
        std::snprintf(title, sizeof(title), "pitch-weather - %s, intensity %.1f%s", kindName(shown.kind),
            shown.intensity, matchMode ? " (match mode)" : "");
        SDL_SetWindowTitle(window, title);

        drawPitch(renderer, width, height);
        draw(renderer, { 0, 0, width, height }, shown, seconds);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
