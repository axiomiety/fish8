#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 32
#define SCALE 10
#define MEM_SIZE 4096
#define PIXEL_ON 0x0000ffff

typedef struct {
    uint8_t registers[16];
    uint16_t i;
    uint8_t pc;
    uint8_t sp;
    uint8_t stack[12];
    uint8_t delay_timer;
    uint8_t sound_timer;
    bool input[16];
    bool quit;
    bool draw;
} State;

void
fillScreen(uint32_t pixels[], uint32_t pixel);

void
loadRom(char *fileName, uint8_t memory[]);

void
processOp(State *state, uint8_t memory[]);

void
updateScreen(SDL_Renderer *renderer, SDL_Texture *texture, uint32_t pixels[]);

void
processEvent(State * state, SDL_Event *event);
