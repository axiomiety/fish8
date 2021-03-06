#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 32
#define SCALE 10
#define MEM_SIZE 4096
#define SPRITES_OFFSET 0x0
#define ROM_OFFSET 0x200
#define MAX_ROM_SIZE (0xea0 - 0x200)
#define MEM_DISPLAY_START 0xf00
#define PIXEL_ON 0xffffffff
#define PIXEL_OFF 0x000000ff

typedef struct {
    uint8_t registers[16];
    uint16_t i;
    uint16_t pc;
    uint8_t sp;
    uint16_t stack[12];
    uint8_t delay_timer;
    uint8_t sound_timer;
    bool input[16];
    bool quit;
    bool draw;
    bool pixels[SCREEN_WIDTH*SCREEN_HEIGHT];
} State;

void
fillScreen(uint32_t pixels[], uint32_t pixel);

void
loadROM(char *fileName, uint8_t memory[]);

void
processOp(State *state, uint8_t memory[]);

void
copySpritesToMemory(uint8_t memory[]);

void
updateScreen(SDL_Renderer *renderer, SDL_Texture *texture, uint8_t memory[], uint32_t pixels[]);

void
updateScreen2(SDL_Renderer *renderer, SDL_Texture *texture, bool myPixels[], uint32_t pixels[]);

void
processInput(State * state, const uint8_t keyStates[]);

void
processEvent(State * state, SDL_Event *event);
