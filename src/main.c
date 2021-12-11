#include <stdio.h>
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include "mylib.h"

int main(int argc, char *argv[])
{
    int scale = 1;
    char *romFilename;
    int clockSpeed = 500;
    int c;
    while ((c = getopt(argc, argv, "s:r:c:")) != -1)
    {
        switch (c)
        {
        case 's':
            scale = atoi(optarg);
            break;
        case 'r':
            romFilename = optarg;
            break;
        case 'c':
            clockSpeed = atoi(optarg);
            break;
        case '?':
            fprintf(stderr, "Scale (-s) requires an integer > 0, clock speend (-c) too, and ROM (-r) a path to the ROM");
            return 1;
        default:
            abort();
        }
    }

    SDL_Init(SDL_INIT_EVERYTHING);

    SDL_Window *window = SDL_CreateWindow("CHIP8 Display",
                                          SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH * scale, SCREEN_HEIGHT * scale, SDL_WINDOW_RESIZABLE);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, SCREEN_WIDTH, SCREEN_HEIGHT);
    SDL_RenderSetScale(renderer, SCALE, scale);

    // VM init
    uint8_t memory[MEM_SIZE];
    // important so we don't have any random pixels turned on when they shouldn't
    // we can't use memset because it works at a byte-level and each memory "cell" is 2-bytes wide
    for (int i = 0; i < MEM_SIZE; i++)
    {
        memory[i] = 0;
    }
    // load up the sprites
    copySpritesToMemory(memory);
    State state = {.draw = false, .pc = ROM_OFFSET};
    // this is where our *actual* pixels will be stored
    uint32_t pixels[SCREEN_WIDTH * SCREEN_HEIGHT];
    SDL_Log("ROM filename: %s", romFilename);
    loadROM(romFilename, memory);

    const uint8_t *keyStates = SDL_GetKeyboardState(NULL);
    // 60Hz, in milliseconds
    float timerDelta = 1 / 60.0 * 1000;
    float accumulator = 0.0;
    uint32_t currTick = SDL_GetTicks();
    uint32_t newTick, elapsedTicks, numCycles;
    uint32_t totalCycles = 0;
    float timePerCycle;
    while (!state.quit)
    {
        newTick = SDL_GetTicks();
        elapsedTicks = newTick - currTick;
        numCycles = elapsedTicks / 1000 * clockSpeed;
        totalCycles += numCycles;
        if (numCycles > 0)
        {
            currTick = newTick;
            timePerCycle = elapsedTicks / numCycles;
            while (numCycles > 1)
            {

                SDL_PumpEvents(); // this is needed to populate the keyboard state array
                processInput(&state, keyStates);
                processOp(&state, memory);
                accumulator += timePerCycle;
                if (keyStates[SDL_SCANCODE_SPACE])
                {
                    SDL_Log("Backspace pressed, will exit");
                    state.quit = true;
                    break;
                }
                if (state.draw)
                {
                    updateScreen2(renderer, texture, state.pixels, pixels);
                    state.draw = false;
                }
                while (accumulator > timerDelta)
                {
                    if (state.delay_timer > 0)
                        state.delay_timer--;
                    if (state.sound_timer > 0)
                        //TODO: play audio!
                        state.sound_timer--;
                    accumulator -= timerDelta;
                }
                numCycles--;
                SDL_Delay(timePerCycle);
            }
        }
    }
    // bit of a delay so we get the see the screen before it closes
    SDL_Delay(2000);

    SDL_Quit();

    return 0;
}