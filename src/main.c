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
    State state = {.draw = true};
    uint32_t pixels[SCREEN_WIDTH * SCREEN_HEIGHT];
    fillScreen(pixels, PIXEL_ON);
    loadRom(romFilename, memory);

    while (!state.quit)
    {
        processOp(&state, memory);
        if (state.draw)
        {
            updateScreen(renderer, texture, pixels);
            state.draw = false;
        }
        if (SDL_PollEvent(&event))
        {
            processEvent(&state, &event);
        }
    }

    SDL_Quit();

    return 0;
}