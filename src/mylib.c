#include <stdint.h>
#include <stdio.h>
#include "mylib.h"

void
fillScreen(uint32_t pixels[], uint32_t pixel)
{
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++)
    {
        pixels[i] = pixel;
    }
}

void loadRom(char *fileName, uint8_t memory[])
{
    /*
    Load the file into the memory structure passed in.

    Performs a boundary check to ensure our ROM isn't bigger
    */
}

void processOp(State *state, uint8_t memory[])
{
}

void updateScreen(SDL_Renderer *renderer, SDL_Texture *texture, uint32_t pixels[])
{
    SDL_UpdateTexture(texture, NULL, pixels, SCREEN_HEIGHT * sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void processEvent(State *state, SDL_Event *event)
{
    switch (event->type)
    {
    case SDL_QUIT:
        state->quit = true;
        SDL_Log("Quit pressed");
        break;
    case SDL_KEYDOWN:
        switch (event->key.keysym.sym)
        {
        case SDLK_q:
            state->quit = true;
            SDL_Log("Escape pressed");
            break;
        default:
            break;
        }
    default:
        break;
    }
}