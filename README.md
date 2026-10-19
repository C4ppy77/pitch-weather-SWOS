# pitch-weather-SWOS

Rain, drizzle and snow for a 2D football game, drawn with SDL2. Made for my iPhone port of Sensible World of Soccer,
where it follows the pitch type: wet is rain, muddy heavy rain, soft drizzle and frozen snow.

It's one small C++ file with no dependencies beyond SDL2 (2.0.18 or later), so it works anywhere SDL2 does: Windows,
macOS, Linux, iOS, Android.

> Not official and not affiliated with Sensible Software or the SWOS rights holders. This repo contains no SWOS code
> or game data, only the weather effect.

## How it works

- **What falls:** each pitch type maps to a kind of weather (drizzle, rain, heavy rain, snow).
- **How much:** the intensity drifts over the 90 minutes in two slow sine waves, starting at a random point each
  match, so no two matches look the same.
- **Drawing:** every drop is a thin slanted streak fading towards its tail, every snowflake a tiny swaying diamond,
  plus a faint dark (rain) or white (snow) tint. Everything goes to the GPU in a single `SDL_RenderGeometry` call,
  drawn straight into the game's own frame. (Drawing it as a separate full-screen layer on top cost an iPhone 13
  about a third of its frames.)
- **No per-drop state:** each drop's position comes from a fixed pseudo-random start plus the clock, so nothing is
  stored or updated between frames.

## Using it

Add `src/pitchWeather.cpp` and `src/pitchWeather.h` to your project, then once a frame, after drawing the game and
before `SDL_RenderPresent`:

```cpp
#include "pitchWeather.h"

// at kick-off
int seed = SDL_GetPerformanceCounter() % 360;

// every frame
auto weather = PitchWeather::forPitchType(pitchType, matchMinute, seed);
PitchWeather::draw(renderer, { 0, 0, screenWidth, screenHeight }, weather, SDL_GetTicks64() / 1000.0);
```

Pitch types: 0 frozen, 1 muddy, 2 wet, 3 soft; anything else draws nothing. Or skip `forPitchType` and pass your own
`PitchWeather::Weather{ PitchWeather::Kind::Rain, 0.8f }`. To let players turn it off, just don't call `draw`.

## Demo

```sh
cmake -S . -B build
cmake --build build --config Release
./build/pitch_weather_demo        # Windows: build\Release\pitch_weather_demo.exe
```

Keys: `1` drizzle, `2` rain, `3` heavy rain, `4` snow, `0` none, `Up`/`Down` intensity, `M` match mode (a 90 minute
match in 90 seconds), `Esc` quit. Start in another weather with `pitch_weather_demo snow` (or `drizzle`, `rain`, `heavy`).

Needs SDL2: `brew install sdl2` on macOS, `apt install libsdl2-dev` on Debian/Ubuntu, or on Windows the SDL2
development package from [libsdl.org](https://github.com/libsdl-org/SDL/releases) (point CMake at it with
`-DSDL2_DIR=<path to its cmake folder>`) or `vcpkg install sdl2`.

## Credit

MIT licence: free to use, just keep the copyright notice. If you use it in your game, a mention in your credits or a
link back here would be appreciated. — C4ppy77
