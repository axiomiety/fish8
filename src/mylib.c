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

void loadROM(char *fileName, uint8_t memory[])
{
    FILE *fp;
    fp = fopen(fileName, "rb");
    int bytesRead = fread(memory + ROM_OFFSET, sizeof(uint8_t), MAX_ROM_SIZE, fp);
    SDL_Log("Read %d bytes from %s", bytesRead, fileName);
    int numOpcodesToPrint = 8;
    SDL_Log("The first %d opcodes are:", numOpcodesToPrint);
    int i;
    for (i = 0; i < numOpcodesToPrint; i++) {
        SDL_Log("Opcode at %x: %x%x", i*2, memory[ROM_OFFSET+i*2], memory[ROM_OFFSET+i*2+1]);
    }
}

void processOp(State *state, uint8_t memory[])
{
}

void updateScreen(SDL_Renderer *renderer, SDL_Texture *texture, uint8_t memory[], uint32_t pixels[])
{
    // assume we're filling by row
    int row, colGroup;
    uint8_t values;
    int pixelIndex = 0;
    for (row=0; row<SCREEN_HEIGHT; row++) {
        // our memory unit is a byte - so each block of 8 bits represents 8 pixels
        for (colGroup=0; colGroup<SCREEN_WIDTH/8; colGroup++) {
            values = memory[MEM_DISPLAY_START+row*(SCREEN_WIDTH/8)+colGroup];
            // we now bit-shift to get the state of each pixel
            int shift;
            for (shift=7; shift>=0; shift--) {
                pixels[pixelIndex] = ((values >> shift) & 0x1) ? PIXEL_ON : PIXEL_OFF;
                pixelIndex++; 
            }
        }
    }
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
        case SDLK_1:
            state->input[0x1] = true;
        default:
            break;
        }
    case SDL_KEYUP:
        switch (event->key.keysym.sym)
        {
        case SDLK_1:
            state->input[0x1] = false;
        default:
            break;
        }
    default:
        break;
    }
}