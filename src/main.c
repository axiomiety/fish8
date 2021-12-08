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

    SDL_Event event;

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
    uint32_t pixels[SCREEN_WIDTH * SCREEN_HEIGHT];
    for (int i = 0; i < SCREEN_HEIGHT * SCREEN_WIDTH; i++)
    {
        pixels[i] = PIXEL_OFF;
    }
    // we don't really need this - ROMs should execute the 0x00e0 instruction
    // that essentially does the same thing
    updateScreen(renderer, texture, memory, pixels);
    SDL_Log("ROM filename: %s", romFilename);
    loadROM(romFilename, memory);
    // uint8_t test_rom[] = {0xff, 0x29, 0xd0, 0x05, 0xf1, 0x0a, 0x0, 0xe0};
    // memcpy(memory + ROM_OFFSET, test_rom, sizeof(test_rom));

    const uint8_t *keyStates = SDL_GetKeyboardState(NULL);
    bool running = true;
    bool step = false;
    // 60Hz, in milliseconds
    float timerDelta = 1 / 60.0 * 1000;
    float accumulator = 0.0;
    uint32_t currTick = SDL_GetTicks();
    uint32_t newTick, elapsedTicks, numCycles;
    uint32_t totalCycles = 0;
    float timePerCycle;
    while (!state.quit)
    {
        if (running || step)
        {
            newTick = SDL_GetTicks();
            elapsedTicks = newTick - currTick;
            numCycles = elapsedTicks/1000 * clockSpeed;
            totalCycles += numCycles;
            if (numCycles > 0)
            {
                currTick = newTick;
                timePerCycle = elapsedTicks / numCycles;
                while (numCycles > 1)
                {
                    processOp(&state, memory);
                    if (state.draw)
                    {
                        updateScreen(renderer, texture, memory, pixels);
                        state.draw = false;
                    }
                    processInput(&state, keyStates);
                    accumulator += timePerCycle;
                    while (accumulator > timerDelta)
                    {
                        if (state.delay_timer > 0)
                            state.delay_timer--;
                        if (state.sound_timer > 0)
                            state.sound_timer--;
                        accumulator -= timerDelta;
                    }
                    numCycles--;
                }
                step = false;
            }
            if (totalCycles > clockSpeed)
            {
                float numSeconds = totalCycles / clockSpeed;
                SDL_Log("Processed %f seconds' worth", numSeconds);
                totalCycles = totalCycles % clockSpeed;
            }
        }
        if (keyStates[SDL_SCANCODE_SPACE])
        {
            SDL_Log("Pause toggled");
            // so we don't untoggle too fast
            SDL_Delay(1000);
            running = running ? false : true;
        }
        if (keyStates[SDL_SCANCODE_RETURN])
        {
            SDL_Log("Stepping through");
            step = true;
        }
        // more for quit than anything else?
        while (SDL_PollEvent(&event))
        {
            processEvent(&state, &event);
        }
    }
    SDL_Delay(2000);

    SDL_Quit();

    return 0;
}