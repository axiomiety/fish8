#include <stdint.h>
#include <stdio.h>
#include "mylib.h"

void fillScreen(uint32_t pixels[], uint32_t pixel)
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
    for (int i = 0; i < numOpcodesToPrint; i++)
    {
        SDL_Log("Opcode at %0x: %0x%0x", i * 2, memory[ROM_OFFSET + i * 2], memory[ROM_OFFSET + i * 2 + 1]);
    }
}
void clearDisplay(State *state, uint8_t memory[])
{
    memset(memory+MEM_DISPLAY_START, 0, 256*sizeof(uint8_t));
    state->draw = true;
    state->pc += 2;
}
void jumpToAddress(State *state, uint8_t opCodeB, uint8_t opCodeRight) {
    // we need to combine opCodeB and opCodeRight to form a 12-bit address
    uint16_t address = (opCodeB << 8) | opCodeRight;
    state->pc = address;
}
void callSubroutine(State *state, uint8_t opCodeB, uint8_t opCodeRight) {
    // we need to combine opCodeB and opCodeRight to form a 12-bit address
    uint16_t address = (opCodeB << 8) | opCodeRight;
    // push the return address onto the stack
    state->stack[state->sp] = state->pc + 2;
    state->sp += 1;
    state->pc = address;
}
void returnFromSubroutine(State *state) {
    state->sp -= 1;
    state->pc = state->stack[state->sp];
}

void processOp(State *state, uint8_t memory[])
{
    // memory is byte-addressable, but opcodes are 2-bytes long
    // for simplicity, we break this as such:
    // 0x0123
    // opCodeLeft = 0x01
    // opcodeRight = 0x23
    // opCodeA,B,C,D = 0x0, 0x1, 0x2, 0x3
    uint8_t opCodeLeft, opCodeRight, opCodeA, opCodeB, opCodeC, opCodeD;
    opCodeLeft = memory[state->pc];
    opCodeRight = memory[state->pc + 1];
    opCodeA = opCodeLeft >> 4;
    opCodeB = opCodeLeft & 0x0f;
    opCodeC = opCodeRight >> 4;
    opCodeD = opCodeRight & 0x0f;
    bool error = false;
    SDL_Log("Decoding %02x%02x (A:%x, B:%x, C:%x, D:%x)", opCodeLeft, opCodeRight, opCodeA, opCodeB, opCodeC, opCodeD);
    switch (opCodeA)
    {
    case (0x0):
    {
        switch (opCodeB)
        {
        case (0x0):
        {
            switch (opCodeRight)
            {
            case (0xe0):
                clearDisplay(state, memory);
                break;
            case (0xee):
                returnFromSubroutine(state);
                break;
            default:
                error = true;
                break;
            }
        }
        break;
        default:
            error = true;
            break;
        }
    }
    break;
    case (0x1):
        jumpToAddress(state, opCodeB, opCodeRight);
        break;
    case (0x2):
        callSubroutine(state, opCodeB, opCodeRight);
        break;
    default:
        error = true;
        break;
    }
    if (error)
    {
        // we could do a bit more like dumping the state/memory
        SDL_Log("Unknown/unimplemented opcode %02x%02x", opCodeLeft, opCodeRight);
        exit(1);
    }
}

void updateScreen(SDL_Renderer *renderer, SDL_Texture *texture, uint8_t memory[], uint32_t pixels[])
{
    // assume we're filling by row
    uint8_t values;
    int pixelIndex = 0;
    for (int row = 0; row < SCREEN_HEIGHT; row++)
    {
        // our memory unit is a byte - so each block of 8 bits represents 8 pixels
        for (int colGroup = 0; colGroup < SCREEN_WIDTH / 8; colGroup++)
        {
            values = memory[MEM_DISPLAY_START + row * (SCREEN_WIDTH / 8) + colGroup];
            // we now bit-shift to get the state of each pixel
            for (int shift = 7; shift >= 0; shift--)
            {
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
        case SDLK_ESCAPE:
            state->quit = true;
            SDL_Log("Escape pressed");
            break;
        case SDLK_1:
            state->input[0x1] = true;
            break;
        case SDLK_2:
            state->input[0x2] = true;
            break;
        case SDLK_3:
            state->input[0x3] = true;
            break;
        case SDLK_4:
            state->input[0xc] = true;
            break;
        case SDLK_q:
            state->input[0x4] = true;
            break;
        case SDLK_w:
            state->input[0x5] = true;
            break;
        case SDLK_e:
            state->input[0x6] = true;
            break;
        case SDLK_r:
            state->input[0xd] = true;
            break;
        case SDLK_a:
            state->input[0x7] = true;
            break;
        case SDLK_s:
            state->input[0x8] = true;
            break;
        case SDLK_d:
            state->input[0x9] = true;
            break;
        case SDLK_f:
            state->input[0xe] = true;
            break;
        case SDLK_z:
            state->input[0xa] = true;
            break;
        case SDLK_x:
            state->input[0x0] = true;
            break;
        case SDLK_c:
            state->input[0xb] = true;
            break;
        case SDLK_v:
            state->input[0xf] = true;
            break;
        default:
            break;
        }
    case SDL_KEYUP:
        switch (event->key.keysym.sym)
        {
        case SDLK_1:
            state->input[0x1] = false;
            break;
        case SDLK_2:
            state->input[0x2] = false;
            break;
        case SDLK_3:
            state->input[0x3] = false;
            break;
        case SDLK_4:
            state->input[0xc] = false;
            break;
        case SDLK_q:
            state->input[0x4] = false;
            break;
        case SDLK_w:
            state->input[0x5] = false;
            break;
        case SDLK_e:
            state->input[0x6] = false;
            break;
        case SDLK_r:
            state->input[0xd] = false;
            break;
        case SDLK_a:
            state->input[0x7] = false;
            break;
        case SDLK_s:
            state->input[0x8] = false;
            break;
        case SDLK_d:
            state->input[0x9] = false;
            break;
        case SDLK_f:
            state->input[0xe] = false;
            break;
        case SDLK_z:
            state->input[0xa] = false;
            break;
        case SDLK_x:
            state->input[0x0] = false;
            break;
        case SDLK_c:
            state->input[0xb] = false;
            break;
        case SDLK_v:
            state->input[0xf] = false;
            break;
        default:
            break;
        }
    default:
        break;
    }
}