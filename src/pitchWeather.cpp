// pitch-weather: rain, drizzle and snow drawn over a 2D football game with SDL2.
// Copyright (c) 2026 C4ppy77. MIT licence, see LICENSE.
//
// Every drop is a thin slanted quad fading towards its tail, every snowflake a soft disc in one of three depth layers
// blown by a gusting wind, and they all go to the GPU in one SDL_RenderGeometry call. A drop's position comes from a
// fixed pseudo-random start plus the clock, so nothing is stored or updated per drop.
#include "pitchWeather.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace PitchWeather {

// how rain looks; lengths and widths in points on a 390 point tall screen, scaled to the real one
struct RainStyle
{
    float drops;            // on screen at once at full intensity
    float length;
    float width;
    float alpha;
    float speed;            // screen heights a second
};

static constexpr float kRainSlant = 0.22f;
static constexpr float kReferenceScreenHeight = 390;

Weather forPitchType(int pitchType, float minute, int seed)
{
    Weather weather;
    float low, high;
    switch (pitchType) {
    case 0: weather.kind = Kind::Snow; low = 0.55f; high = 1; break;
    case 1: weather.kind = Kind::HeavyRain; low = 0.7f; high = 1; break;
    case 2: weather.kind = Kind::Rain; low = 0.5f; high = 0.9f; break;
    case 3: weather.kind = Kind::Drizzle; low = 0.4f; high = 0.8f; break;
    default: return weather;
    }

    constexpr double kPi = 3.14159265358979;
    double t = std::max(0.0f, minute) / 90.0 * 2 * kPi;
    double phase = seed * kPi / 180;
    double wave = (std::sin(1.3 * t + phase) + 0.5 * std::sin(3.1 * t + 2 * phase)) / 1.5;
    double intensity = low + (high - low) * (wave + 1) / 2;
    weather.intensity = static_cast<float>(std::round(intensity * 10) / 10);
    return weather;
}

static RainStyle rainStyle(Kind kind)
{
    switch (kind) {
    case Kind::Drizzle: return { 70, 9, 0.8f, 0.22f, 1.1f };
    case Kind::Rain: return { 130, 14, 1, 0.28f, 1.5f };
    default: return { 210, 20, 1.2f, 0.32f, 1.9f };
    }
}

// a fixed pseudo-random 0..1 for drop i; salt picks a different number for each property
static float random(int i, int salt)
{
    double value = std::sin(i * 12.9898 + salt * 78.233) * 43758.5453;
    return static_cast<float>(value - std::floor(value));
}

// value wrapped into 0..range, also for negative values
static double wrap(double value, double range)
{
    double wrapped = std::fmod(value, range);
    return wrapped < 0 ? wrapped + range : wrapped;
}

// Snow falls in three depths: lots of faint specks far away, a main layer, and a few big soft flakes close to the
// camera that fall faster and get blown around more. Sizes and flutter in points, speeds in screen heights a second.
struct SnowLayer
{
    float share;            // of all flakes
    float radius, radiusSpread;
    float speed, speedSpread;
    float alpha, alphaSpread;
    float core;             // solid part of the flake, the rest fades out
    float flutter;          // how far a flake sways
    float wind;             // how much the gusts move it
};

static constexpr float kSnowFlakes = 280;          // on screen at once at full intensity
static constexpr SnowLayer kSnowLayers[] = {
    { 0.55f, 0.9f, 0.5f, 0.045f, 0.02f, 0.35f, 0.2f, 0.5f, 4, 0.45f },   // far
    { 0.35f, 1.8f, 0.8f, 0.08f, 0.03f, 0.6f, 0.25f, 0.45f, 7, 0.8f },    // middle
    { 0.10f, 3.6f, 1.6f, 0.15f, 0.06f, 0.45f, 0.2f, 0.25f, 12, 1.4f },   // near, big and soft
};

static const SnowLayer& snowLayerFor(float pick)
{
    for (const auto& layer : kSnowLayers) {
        if (pick < layer.share)
            return layer;
        pick -= layer.share;
    }
    return kSnowLayers[0];
}

static SDL_Color color(Uint8 r, Uint8 g, Uint8 b, float alpha)
{
    return { r, g, b, static_cast<Uint8>(std::clamp(alpha, 0.0f, 1.0f) * 255) };
}

static void drawTint(SDL_Renderer *renderer, const SDL_Rect& area, const Weather& weather)
{
    bool snow = weather.kind == Kind::Snow;
    float strength = weather.intensity * (weather.kind == Kind::HeavyRain ? 1 : 0.7f);
    auto top = snow ? color(255, 255, 255, 0.16f * strength) : color(0x1a, 0x22, 0x33, 0.28f * strength);
    auto bottom = snow ? color(255, 255, 255, 0.04f * strength) : color(0x1a, 0x22, 0x33, 0.08f * strength);

    float left = static_cast<float>(area.x), right = static_cast<float>(area.x + area.w);
    float upper = static_cast<float>(area.y), lower = static_cast<float>(area.y + area.h);
    SDL_Vertex vertices[4] = {
        { { left, upper }, top, {} }, { { right, upper }, top, {} },
        { { right, lower }, bottom, {} }, { { left, lower }, bottom, {} },
    };
    static const int kIndices[6] = { 0, 1, 2, 0, 2, 3 };
    SDL_RenderGeometry(renderer, nullptr, vertices, 4, kIndices, 6);
}

void draw(SDL_Renderer *renderer, const SDL_Rect& area, const Weather& weather, double seconds)
{
    if (weather.kind == Kind::None || weather.intensity <= 0 || area.w <= 0 || area.h <= 0)
        return;

    float width = static_cast<float>(area.w), height = static_cast<float>(area.h);
    float originX = static_cast<float>(area.x), originY = static_cast<float>(area.y);
    float unit = height / kReferenceScreenHeight;

    SDL_BlendMode oldBlendMode;
    SDL_GetRenderDrawBlendMode(renderer, &oldBlendMode);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    drawTint(renderer, area, weather);

    static std::vector<SDL_Vertex> s_vertices;
    static std::vector<int> s_indices;
    s_vertices.clear();
    s_indices.clear();

    auto addQuad = [originX, originY](const SDL_FPoint (&corners)[4], SDL_Color head, SDL_Color tail) {
        int base = static_cast<int>(s_vertices.size());
        for (int i = 0; i < 4; i++)
            s_vertices.push_back({ { originX + corners[i].x, originY + corners[i].y }, i < 2 ? head : tail, {} });
        for (int index : { 0, 1, 2, 0, 2, 3 })
            s_indices.push_back(base + index);
    };

    // a soft round flake: a solid core out to core * radius, then fading to nothing at the rim
    auto addFlake = [originX, originY](float x, float y, float radius, float core, float alpha) {
        constexpr int kSides = 6;
        static const float kCos[kSides] = { 1, 0.5f, -0.5f, -1, -0.5f, 0.5f };
        static const float kSin[kSides] = { 0, 0.866f, 0.866f, 0, -0.866f, -0.866f };
        int base = static_cast<int>(s_vertices.size());
        auto solid = color(255, 255, 255, alpha), clear = color(255, 255, 255, 0);
        x += originX;
        y += originY;
        s_vertices.push_back({ { x, y }, solid, {} });
        for (int i = 0; i < kSides; i++)
            s_vertices.push_back({ { x + kCos[i] * radius * core, y + kSin[i] * radius * core }, solid, {} });
        for (int i = 0; i < kSides; i++)
            s_vertices.push_back({ { x + kCos[i] * radius, y + kSin[i] * radius }, clear, {} });
        for (int i = 0; i < kSides; i++) {
            int inner = base + 1 + i, nextInner = base + 1 + (i + 1) % kSides;
            int outer = inner + kSides, nextOuter = nextInner + kSides;
            for (int index : { base, inner, nextInner, inner, outer, nextOuter, inner, nextOuter, nextInner })
                s_indices.push_back(index);
        }
    };

    if (weather.kind == Kind::Snow) {
        // A shared wind that gusts: its displacement is a steady drift plus two slow waves, so the whole snowfall
        // speeds up, eases off and sometimes swirls back a little. Nearer layers get pushed further.
        double gust = seconds * 14 + 38 * std::sin(seconds * 0.21) + 17 * std::sin(seconds * 0.57 + 1.3);

        int count = static_cast<int>(kSnowFlakes * weather.intensity);
        for (int i = 0; i < count; i++) {
            const auto& layer = snowLayerFor(random(i, 8));
            float radius = (layer.radius + layer.radiusSpread * random(i, 5)) * unit;
            float margin = radius * 2;

            double speed = height * (layer.speed + layer.speedSpread * random(i, 3));
            float y = static_cast<float>(wrap(random(i, 1) * height + seconds * speed, height + 2 * margin)) - margin;

            // each flake flutters on its own, on top of the wind
            double phase = random(i, 7) * 6.2832;
            double flutter = std::sin(seconds * (0.7 + 0.9 * random(i, 6)) + phase) * layer.flutter
                + std::sin(seconds * (1.9 + 1.4 * random(i, 9)) + 2 * phase) * layer.flutter * 0.3;
            double drift = (gust * layer.wind + flutter) * unit;
            float x = static_cast<float>(wrap(random(i, 2) * width + drift, width + 2 * margin)) - margin;

            // a gentle twinkle as the flake tumbles
            float twinkle = 0.85f + 0.15f * static_cast<float>(std::sin(seconds * (2 + 3 * random(i, 10)) + phase));
            float alpha = (layer.alpha + layer.alphaSpread * random(i, 4)) * twinkle;
            addFlake(x, y, radius, layer.core, alpha);
        }
    } else {
        auto style = rainStyle(weather.kind);
        int count = static_cast<int>(style.drops * weather.intensity);
        float length = style.length * unit, halfWidth = style.width * unit / 2;
        double fall = style.speed * height;
        // perpendicular to a drop's slant, for its width
        float normalX = 1 / std::sqrt(1 + kRainSlant * kRainSlant), normalY = -kRainSlant * normalX;
        for (int i = 0; i < count; i++) {
            double speed = fall * (0.8 + 0.4 * random(i, 3));
            float y = static_cast<float>(std::fmod(random(i, 1) * height + seconds * speed, height + length));
            float x = static_cast<float>(std::fmod(random(i, 2) * width + seconds * speed * kRainSlant, width));
            float tailX = x - length * kRainSlant, tailY = y - length;
            float alpha = style.alpha * (0.6f + 0.4f * random(i, 4));
            SDL_FPoint corners[4] = {
                { x - normalX * halfWidth, y - normalY * halfWidth }, { x + normalX * halfWidth, y + normalY * halfWidth },
                { tailX + normalX * halfWidth, tailY + normalY * halfWidth }, { tailX - normalX * halfWidth, tailY - normalY * halfWidth },
            };
            // fading towards the tail
            addQuad(corners, color(255, 255, 255, alpha), color(255, 255, 255, 0));
        }
    }

    if (!s_vertices.empty())
        SDL_RenderGeometry(renderer, nullptr, s_vertices.data(), static_cast<int>(s_vertices.size()),
            s_indices.data(), static_cast<int>(s_indices.size()));
    SDL_SetRenderDrawBlendMode(renderer, oldBlendMode);
}

}
