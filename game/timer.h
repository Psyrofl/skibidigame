#pragma once
#include <SDL3/SDL.h>

class Timer
{
public:
    Timer() : prevTicks(SDL_GetTicks()), deltaTime(0.0f) {}

    // Call once per frame at the top of the loop
    void tick()
    {
        uint64_t now = SDL_GetTicks();
        deltaTime = (now - prevTicks) / 1000.0f; // seconds
        prevTicks = now;
    }

    float getDeltaTime() const { return deltaTime; }

private:
    uint64_t prevTicks;
    float deltaTime;
};