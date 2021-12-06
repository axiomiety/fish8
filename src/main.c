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

    int c;
    while ((c = getopt(argc, argv, "s:r:")) != -1)
    {
        switch (c)
        {
        case 's':
            scale = atoi(optarg);
            break;
        case 'r':
            romFilename = optarg;
            break;
        case '?':
            fprintf(stderr, "Scale (-s) requires an integer > 0 and ROM (-r) a path to the ROM");
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
    //uint8_t test_rom[] = {0xff, 0x29, 0xd0, 0x05, 0xf1, 0x0a, 0x0, 0xe0};
    //memcpy(memory + ROM_OFFSET, test_rom, sizeof(test_rom));

    uint8_t count = 0;
    const uint8_t *keyStates = SDL_GetKeyboardState(NULL);
    bool running = true;
    bool step = false;
    while (!state.quit)
    {
        if (running || step)
        {
            processOp(&state, memory);
            if (state.draw)
            {
                updateScreen(renderer, texture, memory, pixels);
                state.draw = false;
            }
            processInput(&state, keyStates);

            count += 1;
            step = false;
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
        SDL_Delay(100);
    }
    SDL_Delay(2000);

    SDL_Quit();

    return 0;
}