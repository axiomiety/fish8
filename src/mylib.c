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
void copySpritesToMemory(uint8_t memory[])
{
    uint8_t sprites[] = {
        0xf0, 0x90, 0x90, 0x90, 0xf0, // 0
        0x20, 0x60, 0x20, 0x20, 0x70, // 1
        0xf0, 0x10, 0xf0, 0x80, 0xf0, // 2
        0xf0, 0x10, 0xf0, 0x10, 0xf0, // 3
        0x90, 0x90, 0xf0, 0x10, 0x10, // 4
        0xf0, 0x80, 0xf0, 0x10, 0xf0, // 5
        0xf0, 0x80, 0xf0, 0x90, 0xf0, // 6
        0xf0, 0x10, 0x20, 0x40, 0x40, // 7
        0xf0, 0x90, 0xf0, 0x90, 0xf0, // 8
        0xf0, 0x90, 0xf0, 0x10, 0xf0, // 9
        0xf0, 0x90, 0xf0, 0x90, 0x90, // a
        0xe0, 0x90, 0xe0, 0x90, 0xe0, // b
        0xf0, 0x80, 0x80, 0x80, 0xf0, // c
        0xe0, 0x90, 0x90, 0x90, 0xe0, // d
        0xf0, 0x80, 0xf0, 0x80, 0xf0, // e
        0xf0, 0x80, 0xf0, 0x80, 0x80, // f
    };

    memcpy(memory + SPRITES_OFFSET, sprites, sizeof(sprites));
}
void clearDisplay(State *state, uint8_t memory[])
{
    memset(memory + MEM_DISPLAY_START, 0, 256 * sizeof(uint8_t));
    state->draw = true;
    for (int i=0;i<SCREEN_WIDTH*SCREEN_HEIGHT;i++)
    {
        state->pixels[i] = false;
    }
    state->pc += 2;
}
void jumpToAddress(State *state, uint8_t opCodeB, uint8_t opCodeRight)
{
    // we need to combine opCodeB and opCodeRight to form a 12-bit address
    uint16_t address = (opCodeB << 8) | opCodeRight;
    state->pc = address;
}
void callSubroutine(State *state, uint8_t opCodeB, uint8_t opCodeRight)
{
    // we need to combine opCodeB and opCodeRight to form a 12-bit address
    uint16_t address = (opCodeB << 8) | opCodeRight;
    // push the return address onto the stack
    state->stack[state->sp] = state->pc + 2;
    state->sp += 1;
    state->pc = address;
}
void returnFromSubroutine(State *state)
{
    state->sp -= 1;
    state->pc = state->stack[state->sp];
}
void setRegister(State *state, uint8_t reg, uint8_t opCodeRight)
{
    // reg is a byte long, but we only care for the last 4 bits
    state->registers[reg] = opCodeRight;
    state->pc += 2;
}
void addToRegister(State *state, uint8_t reg, uint8_t opCodeRight)
{
    // reg is a byte long, but we only care for the last 4 bits
    state->registers[reg] = (state->registers[reg] + opCodeRight) & 0xff;
    state->pc += 2;
}
void jumpIfRegEqualToConst(State *state, uint8_t reg, uint8_t value)
{
    state->pc += (state->registers[reg] == value) ? 4 : 2;
}
void jumpIfRegNotEqualToConst(State *state, uint8_t reg, uint8_t value)
{
    state->pc += (state->registers[reg] != value) ? 4 : 2;
}
void jumpIfRegEqualToReg(State *state, uint8_t reg1, uint8_t reg2)
{
    state->pc += (state->registers[reg1] == state->registers[reg2]) ? 4 : 2;
}
void jumpIfRegNotEqualToReg(State *state, uint8_t reg1, uint8_t reg2)
{
    state->pc += (state->registers[reg1] != state->registers[reg2]) ? 4 : 2;
}
void setRegisterToRegister(State *state, uint8_t reg1, uint8_t reg2)
{
    state->registers[reg1] = state->registers[reg2];
    state->pc += 2;
}
void setRegisterToBitwiseOr(State *state, uint8_t reg1, uint8_t reg2)
{
    state->registers[reg1] |= state->registers[reg2];
    state->pc += 2;
}
void setRegisterToBitwiseAnd(State *state, uint8_t reg1, uint8_t reg2)
{
    state->registers[reg1] &= state->registers[reg2];
    state->pc += 2;
}
void setRegisterToBitwiseXor(State *state, uint8_t reg1, uint8_t reg2)
{
    state->registers[reg1] ^= state->registers[reg2];
    state->pc += 2;
}
void leftShift(State *state, uint8_t reg)
{
    uint16_t val = state->registers[reg] << 1;
    state->registers[reg] = val & 0xff;
    state->registers[0xf] = (val & 0x100) >> 8;
    state->pc += 2;
}
void rightShift(State *state, uint8_t reg)
{
    state->registers[0xf] = state->registers[reg] & 0x1;
    state->registers[reg] >>= 1;
    state->pc += 2;
}
void addRegisters(State *state, uint8_t reg1, uint8_t reg2)
{
    uint16_t val = state->registers[reg1] + state->registers[reg2];
    state->registers[reg1] = val & 0xff;
    state->registers[0xf] = (val & 0x100) >> 8;
    state->pc += 2;
}
void subtractRegisters(State *state, uint8_t reg1, uint8_t reg2)
{
    bool needBorrow = state->registers[reg2] > state->registers[reg1];
    state->registers[reg1] = (state->registers[reg1] - state->registers[reg2]) % 0xff;
    state->registers[0xf] = needBorrow ? 0 : 1;
    state->pc += 2;
}
void subtractRightFromLeft(State *state, uint8_t reg1, uint8_t reg2)
{
    bool needBorrow = state->registers[reg1] > state->registers[reg2];
    state->registers[reg1] = (state->registers[reg2] - state->registers[reg1]) % 0xff;
    // if we need a borrow, this is set  0
    state->registers[0xf] = needBorrow ? 0 : 1;
    state->pc += 2;
}
void jumpIfKeyPressed(State *state, uint8_t reg)
{
    state->pc += state->input[state->registers[reg]] ? 4 : 2;
}
void jumpIfKeyNotPressed(State *state, uint8_t reg)
{
    state->pc += state->input[state->registers[reg]] ? 2 : 4;
}
void waitForKey(State *state, uint8_t reg)
{
    for (int key = 0; key < 16; key++)
    {
        if (state->input[key])
        {
            SDL_Log("noticed key %d pressed", key);
            state->registers[reg] = key;
            state->pc += 2;
            break;
        }
    }
}
void setI(State *state, uint8_t top, uint8_t bottom)
{
    state->i = (top << 8) | bottom;
    state->pc += 2;
}
void addRegToI(State *state, uint8_t reg)
{
    state->i += state->registers[reg];
    state->pc += 2;
}
void setPC(State *state, uint8_t top, uint8_t bottom)
{
    state->pc = state->registers[0] + ((top << 8) | bottom);
}
void saveRegisters(State *state, uint8_t reg, uint8_t memory[])
{
    uint16_t memIdx = state->i;
    for (int regIdx = 0; regIdx <= reg; regIdx++)
    {
        memory[memIdx++] = state->registers[regIdx];
    }
    state->pc += 2;
}
void loadRegisters(State *state, uint8_t reg, uint8_t memory[])
{
    uint16_t memIdx = state->i;
    for (int regIdx = 0; regIdx <= reg; regIdx++)
    {
        state->registers[regIdx] = memory[memIdx++];
    }
    state->pc += 2;
}
void getRandomNumber(State *state, uint8_t reg, uint8_t mask)
{
    state->registers[reg] = rand() % 256 & mask;
    state->pc += 2;
}
void setIToSprite(State *state, uint8_t reg)
{
    // a sprite is 5 bytes long and indexing starts at 0 for this interpreter
    state->i = reg * 5;
    state->pc += 2;
}
void setPixels2(State *state, uint8_t xReg, uint8_t yReg, uint8_t height, uint8_t memory[])
{
    state->registers[0xf] = 0;
    for (int h=0; h<height;h++) {
        uint8_t y = (state->registers[yReg] + h) % SCREEN_HEIGHT;
        uint8_t sprite = memory[state->i+h];
        for (int shift=7;shift>=0;shift--) {
            uint8_t x = (state->registers[xReg] + 7-shift) % SCREEN_WIDTH;
            uint8_t bit = (sprite >> shift) & 0x1;
            state->registers[0xf] |= state->pixels[x+y*SCREEN_WIDTH] & bit;
            state->pixels[x+y*SCREEN_WIDTH] ^= bit; 
        }
    }
    state->draw = true;
    state->pc += 2;
}
void setIToBCD(State *state, uint8_t reg, uint8_t memory[])
{
    uint8_t val = state->registers[reg];
    for (int offset = 2; offset >= 0; offset--)
    {
        memory[state->i + offset] = val % 10;
        val -= memory[state->i + offset];
        val /= 10;
    }
    state->pc += 2;
}
void setRegisterToDelayTimer(State *state, uint8_t reg)
{
    state->registers[reg] = state->delay_timer;
    state->pc += 2;
}
void setDelayTimerFromRegister(State *state, uint8_t reg)
{
    state->delay_timer = state->registers[reg];
    state->pc += 2;
}
void setSoundTimerFromRegister(State *state, uint8_t reg)
{
    state->sound_timer = state->registers[reg];
    state->pc += 2;
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
    //SDL_Log("Decoding %02x%02x (A:%x, B:%x, C:%x, D:%x)", opCodeLeft, opCodeRight, opCodeA, opCodeB, opCodeC, opCodeD);
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
    case (0x3):
        jumpIfRegEqualToConst(state, opCodeB, opCodeRight);
        break;
    case (0x4):
        jumpIfRegNotEqualToConst(state, opCodeB, opCodeRight);
        break;
    case (0x5):
        jumpIfRegEqualToReg(state, opCodeB, opCodeC);
        break;
    case (0x6):
        setRegister(state, opCodeB, opCodeRight);
        break;
    case (0x7):
        addToRegister(state, opCodeB, opCodeRight);
        break;
    case (0x8):
    {
        switch (opCodeD)
        {
        case (0x0):
            setRegisterToRegister(state, opCodeB, opCodeC);
            break;
        case (0x1):
            setRegisterToBitwiseOr(state, opCodeB, opCodeC);
            break;
        case (0x2):
            setRegisterToBitwiseAnd(state, opCodeB, opCodeC);
            break;
        case (0x3):
            setRegisterToBitwiseXor(state, opCodeB, opCodeC);
            break;
        case (0x4):
            addRegisters(state, opCodeB, opCodeC);
            break;
        case (0x5):
            subtractRegisters(state, opCodeB, opCodeC);
            break;
        case (0x6):
            rightShift(state, opCodeB);
            break;
        case (0x7):
            subtractRightFromLeft(state, opCodeB, opCodeC);
            break;
        case (0xe):
            leftShift(state, opCodeB);
            break;
        default:
            error = true;
            break;
        }
    }
    break;
    case (0x9):
        jumpIfRegNotEqualToReg(state, opCodeB, opCodeC);
        break;
    case (0xa):
        setI(state, opCodeB, opCodeRight);
        break;
    case (0xb):
        setPC(state, opCodeB, opCodeRight);
        break;
    case (0xc):
        getRandomNumber(state, opCodeB, opCodeRight);
        break;
    case (0xd):
        setPixels2(state, opCodeB, opCodeC, opCodeD, memory);
        break;
    case (0xe):
    {
        switch (opCodeRight)
        {
        case (0x9e):
            jumpIfKeyPressed(state, opCodeB);
            break;
        case (0xa1):
            jumpIfKeyNotPressed(state, opCodeB);
            break;
        default:
            error = true;
            break;
        }
    }
    break;
    case (0xf):
    {
        switch (opCodeRight)
        {
        case (0x07):
            setRegisterToDelayTimer(state, opCodeB);
            break;
        case (0x0a):
            waitForKey(state, opCodeB);
            break;
        case (0x15):
            setDelayTimerFromRegister(state, opCodeB);
            break;
        case (0x18):
            setSoundTimerFromRegister(state, opCodeB);
            break;
        case (0x1e):
            addRegToI(state, opCodeB);
            break;
        case (0x29):
            setIToSprite(state, opCodeB);
            break;
        case (0x33):
            setIToBCD(state, opCodeB, memory);
            break;
        case (0x55):
            saveRegisters(state, opCodeB, memory);
            break;
        case (0x65):
            loadRegisters(state, opCodeB, memory);
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
    if (error)
    {
        // we could do a bit more like dumping the state/memory
        SDL_Log("Unknown/unimplemented opcode %02x%02x", opCodeLeft, opCodeRight);
        exit(1);
    }
}

void updateScreen2(SDL_Renderer *renderer, SDL_Texture *texture, bool myPixels[], uint32_t pixels[])
{
    for (int pixelIndex = 0; pixelIndex < SCREEN_HEIGHT*SCREEN_WIDTH; pixelIndex++) {
        pixels[pixelIndex] = myPixels[pixelIndex] ? PIXEL_ON : PIXEL_OFF;
    }
    SDL_UpdateTexture(texture, NULL, pixels, SCREEN_WIDTH * sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void updateScreen(SDL_Renderer *renderer, SDL_Texture *texture, uint8_t memory[], uint32_t pixels[])
{
    // assume we're filling by row
    uint8_t values;
    int pixelIndex = 0;
    uint16_t offset = MEM_DISPLAY_START;
    for (int row = 0; row < SCREEN_HEIGHT; row++)
    {
        // our memory unit is a byte - so each block of 8 bits represents 8 pixels
        for (int colGroup = 0; colGroup < SCREEN_WIDTH / 8; colGroup++)
        {
            values = memory[offset];
            // we now bit-shift to get the state of each pixel
            for (int shift = 7; shift >= 0; shift--)
            {
                pixels[pixelIndex] = ((values >> shift) & 0x1) ? PIXEL_ON : PIXEL_OFF;
                pixelIndex++;
            }
            offset += 1;
        }
    }
    SDL_UpdateTexture(texture, NULL, pixels, SCREEN_WIDTH * sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void processInput(State *state, const uint8_t keyStates[])
{
    state->input[0x1] = keyStates[SDL_SCANCODE_1];
    state->input[0x2] = keyStates[SDL_SCANCODE_2];
    state->input[0x3] = keyStates[SDL_SCANCODE_3];
    state->input[0xc] = keyStates[SDL_SCANCODE_4];
    state->input[0x4] = keyStates[SDL_SCANCODE_Q];
    state->input[0x5] = keyStates[SDL_SCANCODE_W];
    state->input[0x6] = keyStates[SDL_SCANCODE_E];
    state->input[0xd] = keyStates[SDL_SCANCODE_R];
    state->input[0x7] = keyStates[SDL_SCANCODE_A];
    state->input[0x8] = keyStates[SDL_SCANCODE_S];
    state->input[0x9] = keyStates[SDL_SCANCODE_D];
    state->input[0xe] = keyStates[SDL_SCANCODE_F];
    state->input[0xa] = keyStates[SDL_SCANCODE_Z];
    state->input[0x0] = keyStates[SDL_SCANCODE_X];
    state->input[0xb] = keyStates[SDL_SCANCODE_C];
    state->input[0xf] = keyStates[SDL_SCANCODE_V];
    for (int i = 0; i < 16; i++)
    {
        if (state->input[i])
        {
            SDL_Log("%x pressed", i);
        }
    }
}

void processEvent(State *state, SDL_Event *event)
{
    switch (event->type)
    {
    case SDL_QUIT:
        state->quit = true;
        SDL_Log("Quit pressed");
        break;
    default:
        break;
    }
}