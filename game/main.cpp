#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <string>
#include <vector>
#include "timer.h"
#include <cmath>

struct SDLState
{
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    int width = 1600, height = 900, logW = 640, logH = 320;
};

struct Bullet
{
    float x, y;   // world position (top-left)
    float dirX;   // -1 = left, +1 = right
    float speed;
};

// ---------- tuning ----------
const float SPRITE_SIZE      = 32.0f;   // player sprite size (px)
const float GRAVITY          = 900.0f;  // px/s^2
const float JUMP_SPEED       = 300.0f;  // px/s
const float BULLET_SPEED     = 700.0f;  // px/s
const float BULLET_DRAW_SIZE = 12.0f;   // longest side of the bullet on screen (px)
const float GUN_HEIGHT       = 16.0f;   // how far down from the player's top the bullet spawns
const float MOVE_SPEED       = 150.0f;  // px/s
const float SPEED_MULTIPLIER = 2.0f;    // multiplier for move speed when shift is held
const float FIRE_COOLDOWN    = 0.35f;   // seconds between shots (lower = faster fire)

bool initialize(SDLState& state);
void cleanup(SDLState& state);
SDL_Texture* loadTexture(SDLState& state, const char* file);

int main(int argc, char* argv[])
{
    SDLState state;
    if (!initialize(state))
    {
        return 1;
    }

    // ---------- load assets ----------
    SDL_Texture* idleTex = loadTexture(state, "idle.png");
    SDL_Texture* bulletTex = loadTexture(state, "bullet.png");
    if (!idleTex || !bulletTex)
    {
        if (idleTex) SDL_DestroyTexture(idleTex);
        if (bulletTex) SDL_DestroyTexture(bulletTex);
        cleanup(state);
        return 1;
    }

    // Fit the bullet image into BULLET_DRAW_SIZE, whatever size the PNG is,
    // while keeping its shape (aspect ratio).
    float texW = 0, texH = 0;
    SDL_GetTextureSize(bulletTex, &texW, &texH);
    SDL_Log("bullet.png is %.0f x %.0f", texW, texH);
    const float bulletScale = BULLET_DRAW_SIZE / std::max(texW, texH);
    const float bulletW = texW * bulletScale;
    const float bulletH = texH * bulletScale;

    // ---------- game state ----------
    const bool* keys = SDL_GetKeyboardState(nullptr);
    const float floor = (float)state.logH;

    float playerX = 150;
    float playerY = 0;       // offset from the floor (negative = up)
    float velY = 0;          // vertical velocity in px/s
    bool grounded = true;
    bool leftFace = false;
    float camX = 0;          // camera offset in world space

    std::vector<Bullet> bullets;
    bool showDebug = false;  // press F1 to draw red boxes around bullets

    Timer timer;

    // ---------- game loop ----------
    bool running = true;
    float moveAmount = 0;
    float shootTimer = 0;

    while (running)
    {
        timer.tick();
        float deltaTime = timer.getDeltaTime();

        // ----- input events -----
        SDL_Event event{ 0 };
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
                case SDL_EVENT_QUIT:
                    running = false;
                    break;

                case SDL_EVENT_WINDOW_RESIZED:
                    state.width = event.window.data1;
                    state.height = event.window.data2;
                    break;

                case SDL_EVENT_KEY_DOWN:
                {
                    if (event.key.repeat) break;
                    SDL_Scancode key = event.key.scancode;

                    // jump
                    if ((key == SDL_SCANCODE_W || key == SDL_SCANCODE_UP) && grounded)
                    {
                        velY = -JUMP_SPEED;
                        grounded = false;
                    }

                    // debug toggle
                    if (key == SDL_SCANCODE_F1)
                    {
                        showDebug = !showDebug;
                    }
                    break;
                    
                    
                }

                case SDL_EVENT_KEY_UP:
                {
                    // release early for a shorter jump
                    SDL_Scancode key = event.key.scancode;
                    if ((key == SDL_SCANCODE_W || key == SDL_SCANCODE_UP) && velY < 0)
                    {
                        velY *= 0.4f;
                    }
                    break;
                }
            }
        }

        // ----- horizontal movement -----
        moveAmount = 0;
        float speed = MOVE_SPEED;  // current move speed, can be modified by shift key
        if (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT])
        {
            speed *= SPEED_MULTIPLIER;   // sprint while Shift is held
        }
        if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT])
        {
            moveAmount -= speed;
            leftFace = true;
        }
        if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT])
        {
            moveAmount += speed;
            leftFace = false;
        }
        if (shootTimer > 0)
        {
            shootTimer -= deltaTime;
        }

        if (keys[SDL_SCANCODE_SPACE] && shootTimer <= 0)
        {
            float playerTop = floor - SPRITE_SIZE + playerY;
            Bullet b;
            b.dirX = leftFace ? -1.0f : 1.0f;
            b.x = leftFace ? playerX - bulletW / 2
                        : playerX + SPRITE_SIZE - bulletW / 2;
            b.y = playerTop + GUN_HEIGHT - bulletH / 2;
            b.speed = BULLET_SPEED;
            bullets.push_back(b);

            shootTimer = FIRE_COOLDOWN;
        }

            playerX += moveAmount * deltaTime;

        // ----- vertical movement: gravity + landing -----
        velY += GRAVITY * deltaTime;
        playerY += velY * deltaTime;
        if (playerY >= 0)
        {
            playerY = 0;
            velY = 0;
            grounded = true;
        }

        // ----- camera follows the player -----
        camX = playerX + SPRITE_SIZE / 2 - state.logW / 2.0f;

        // ----- move bullets, then delete the ones that left the screen -----
    

        for (Bullet& b : bullets)
        {
            b.x += b.dirX * b.speed * deltaTime;

        }
        bullets.erase(
            std::remove_if(bullets.begin(), bullets.end(), [&](const Bullet& b) {
                float screenX = b.x - camX;
                return screenX < -50 || screenX > state.logW + 50;
            }),
            bullets.end());

        // ---------- drawing ----------
        SDL_SetRenderDrawColor(state.renderer, 20, 10, 30, 255);
        SDL_RenderClear(state.renderer);

        // reference pillars so you can see the camera moving
        SDL_SetRenderDrawColor(state.renderer, 80, 60, 110, 255);
        const int spacing = 100;
        int startX = ((int)camX / spacing - 1) * spacing;
        int endX = startX + state.logW + 2 * spacing;
        for (int worldX = startX; worldX <= endX; worldX += spacing)
        {
            SDL_FRect pillar{ worldX - camX, floor - 60, 8, 60 };
            SDL_RenderFillRect(state.renderer, &pillar);
        }

        // player
        SDL_FRect playerSrc{ 0, 0, SPRITE_SIZE, SPRITE_SIZE };
        SDL_FRect playerDst{ playerX - camX, floor - SPRITE_SIZE + playerY, SPRITE_SIZE, SPRITE_SIZE };
        SDL_FlipMode flip = leftFace ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
        SDL_RenderTextureRotated(state.renderer, idleTex, &playerSrc, &playerDst, 0, nullptr, flip);

        // bullets (nullptr source = draw the whole image, squeezed into bulletW x bulletH)
        for (const Bullet& b : bullets)
        {
            SDL_FRect bulletDst{ b.x - camX, b.y, bulletW, bulletH };
            SDL_FlipMode bulletFlip = (b.dirX < 0) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
            SDL_RenderTextureRotated(state.renderer, bulletTex, nullptr, &bulletDst, 0, nullptr, bulletFlip);

            if (showDebug)
            {
                SDL_SetRenderDrawColor(state.renderer, 255, 0, 0, 255);
                SDL_RenderRect(state.renderer, &bulletDst);
            }
        }

        SDL_RenderPresent(state.renderer);
    }

    SDL_DestroyTexture(bulletTex);
    SDL_DestroyTexture(idleTex);
    cleanup(state);
    return 0;
}

// Tries "data/<file>" relative to where the game was started, then
// "data/<file>" next to the .exe. Shows an error box if both fail.
SDL_Texture* loadTexture(SDLState& state, const char* file)
{
    std::string path = std::string("data/") + file;
    SDL_Texture* tex = IMG_LoadTexture(state.renderer, path.c_str());

    if (!tex)
    {
        const char* base = SDL_GetBasePath();
        if (base)
        {
            path = std::string(base) + "data/" + file;
            tex = IMG_LoadTexture(state.renderer, path.c_str());
        }
    }

    if (!tex)
    {
        std::string msg = std::string("Could not load ") + file + "\n\n" + SDL_GetError();
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", msg.c_str(), state.window);
        return nullptr;
    }

    SDL_Log("Loaded %s", path.c_str());
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
    return tex;
}

bool initialize(SDLState& state)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error initializing SDL3", nullptr);
        return false;
    }

    state.window = SDL_CreateWindow("SDL3 Demo", state.width, state.height, SDL_WINDOW_RESIZABLE);
    if (!state.window)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error creating window", nullptr);
        cleanup(state);
        return false;
    }

    state.renderer = SDL_CreateRenderer(state.window, nullptr);
    if (!state.renderer)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Error creating renderer", state.window);
        cleanup(state);
        return false;
    }

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