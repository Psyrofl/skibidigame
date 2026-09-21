#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <iostream>
#include "timer.h"

using namespace std;

struct SDLState
{
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    int width = 1600, height = 900, logW = 640, logH = 320;
};

void cleanup(SDLState& state);
bool initialize(SDLState& state);

int main(int argc, char* argv[])
{
    SDLState state;

    if (!initialize(state))
    {
        return 1;
    }

    // load game assets
    SDL_Texture* idleTex = IMG_LoadTexture(state.renderer, "data/idle.png");
    if (!idleTex)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error loading data/idle.png", state.window);
        cleanup(state);
        return 1;
    }
    SDL_SetTextureScaleMode(idleTex, SDL_SCALEMODE_NEAREST);

    // setup game data
    const bool* keys = SDL_GetKeyboardState(nullptr);
    const float floor = state.logH;
    const float spriteSize = 32;

    float playerX = 150;
    float playerY = 0;              // offset from the floor (negative = up)
    float velY = 0;                 // vertical velocity in px/s
    bool grounded = true;
    bool leftFace = false;

    const float moveSpeed = 300.0f;  // px/s
    const float gravity = 900.0f;   // px/s^2
    const float jumpSpeed = 300.0f; // initial upward speed in px/s

    float camX = 0;                 // camera offset in world space

    Timer timer;

    // start the game loop
    bool running = true;
    while (running)
    {
        timer.tick();
        float deltaTime = timer.getDeltaTime();

        SDL_Event event{ 0 };
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
                case SDL_EVENT_QUIT:
                {
                    running = false;
                    break;
                }
                case SDL_EVENT_WINDOW_RESIZED:
                {
                    state.width = event.window.data1;
                    state.height = event.window.data2;
                    break;
                }
                case SDL_EVENT_KEY_DOWN:
                {
                    if (event.key.scancode == SDL_SCANCODE_W && !event.key.repeat && grounded)
                    {
                        velY = -jumpSpeed;
                        grounded = false;
                    }
                    break;
                }
                case SDL_EVENT_KEY_UP:
                {
                    // release early for a shorter jump
                    if (event.key.scancode == SDL_SCANCODE_W && velY < 0)
                    {
                        velY *= 0.4f;
                    }
                    break;
                }
            }
        }

        // handle horizontal movement
        float moveAmount = 0;
        if (keys[SDL_SCANCODE_A])
        {
            moveAmount -= moveSpeed;
            leftFace = true;
        }
        if (keys[SDL_SCANCODE_D])
        {
            moveAmount += moveSpeed;
            leftFace = false;
        }
        playerX += moveAmount * deltaTime;

        // vertical movement: gravity + landing
        velY += gravity * deltaTime;
        playerY += velY * deltaTime;
        if (playerY >= 0)
        {
            playerY = 0;
            velY = 0;
            grounded = true;
        }

        // camera follows the player (centered horizontally)
        camX = playerX + spriteSize / 2 - state.logW / 2.0f;

        // perform drawing commands
        SDL_SetRenderDrawColor(state.renderer, 20, 10, 30, 255);
        SDL_RenderClear(state.renderer);

        // reference pillars so you can see the camera moving
        SDL_SetRenderDrawColor(state.renderer, 80, 60, 110, 255);
        const int spacing = 100;
        int startX = ((int)camX / spacing - 1) * spacing;
        int endX   = startX + state.logW + 2 * spacing;

        for (int worldX = startX; worldX <= endX; worldX += spacing)
        {
            SDL_FRect pillar{
                .x = worldX - camX,
                .y = floor - 60,
                .w = 8,
                .h = 60,
            };
            SDL_RenderFillRect(state.renderer, &pillar);
        }

        SDL_FRect src{
            .x = 0,
            .y = 0,
            .w = spriteSize,
            .h = spriteSize,
        };

        SDL_FRect dst{
            .x = playerX - camX,
            .y = floor - spriteSize + playerY,
            .w = spriteSize,
            .h = spriteSize,
        };

        SDL_FlipMode flip = leftFace ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
        SDL_RenderTextureRotated(state.renderer, idleTex, &src, &dst, 0, nullptr, flip);

        // swap buffers and present
        SDL_RenderPresent(state.renderer);
    }

    SDL_DestroyTexture(idleTex);
    cleanup(state);
    return 0;
}

bool initialize(SDLState& state)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error initializing SDL3", nullptr);
        return false;
    }

    // create the window
    state.window = SDL_CreateWindow("SDL3 Demo", state.width, state.height, SDL_WINDOW_RESIZABLE);
    if (!state.window)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error creating window", nullptr);
        cleanup(state);
        return false;
    }

    // create the renderer
    state.renderer = SDL_CreateRenderer(state.window, nullptr);
    if (!state.renderer)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error creating renderer", state.window);
        cleanup(state);
        return false;
    }

    // configure presentation
    SDL_SetRenderLogicalPresentation(state.renderer, state.logW, state.logH, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    return true;
}

void cleanup(SDLState& state)
{
    if (state.renderer) SDL_DestroyRenderer(state.renderer);
    if (state.window) SDL_DestroyWindow(state.window);
    state.renderer = nullptr;
    state.window = nullptr;
    SDL_Quit();
}