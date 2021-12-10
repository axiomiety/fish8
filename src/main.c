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

    // SDL_Event event;

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
    // uint8_t test_rom[] = {
    //     0xa2, 0x04, // set i to x204
    //     0x12, 0x0c, // jump to the start of the program
    //     0x42, 0x24, // alien sprite start
    //     0x7e, 0xdb,
    //     0xff, 0x7e,
    //     0x24, 0x3c, // alien sprite end
    //     0x61, 0x0,  // r1=0
    //     0x62, 0x0,  // r2=0
    //     0x0, 0xe0,  // DISP: clear display
    //     0xd1, 0x28, // display sprite
    //     0x71, 0x01, // r1 += 1
    //     0x72, 0x01, // r2 += 1
    //     0x65, 0x3c, // r5 = 60
    //     0xf5, 0x15, // set the timer to r5
    //     0xf5, 0x07, // CHECK_TIMER: r5 = delay timer value
    //     0x35, 0x00, // if r5 (the timer value) is 0, skip the next instructions
    //     0x12, 0x1c, // jump back to CHECK_TIMER
    //     0x12, 0x10, // jump back to DISP
    // };
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
            numCycles = elapsedTicks / 1000 * clockSpeed;
            totalCycles += numCycles;
            if (numCycles > 0)
            {
                currTick = newTick;
                timePerCycle = elapsedTicks / numCycles;
                SDL_PumpEvents(); // this is needed to populate the keyboard state array
                while (numCycles > 1)
                {
                    processOp(&state, memory);
                    processInput(&state, keyStates);
                    accumulator += timePerCycle;
                    if (keyStates[SDL_SCANCODE_BACKSPACE])
                    {
                        SDL_Log("Backspace pressed, will exit");
                        state.quit = true;
                        break;
                    }
                    while (accumulator > timerDelta)
                    {
                        if (state.draw)
                        {
                            updateScreen(renderer, texture, memory, pixels);
                            state.draw = false;
                        }
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
                // float numSeconds = totalCycles / clockSpeed;
                // SDL_Log("Processed %f seconds' worth", numSeconds);
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
        if (keyStates[SDL_SCANCODE_BACKSPACE])
        {
            SDL_Log("Backspace pressed, will exit");
            state.quit = true;
        }
    }
    SDL_Delay(2000);

    SDL_Quit();

    return 0;
}