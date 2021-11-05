#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#include "mylib.h"

static void test_clear_display(void **state)
{
    /*
    Let's assume we have some pixels set in the display section
    Calling 0x00e0 should zero those out
    */

    // init
    State chip8State = {.pc = ROM_OFFSET};
    uint8_t memory[MEM_SIZE];
    memset(memory, 0x0, MEM_SIZE*sizeof(uint8_t));
    // let's turn on some pixels
    memset(memory + MEM_DISPLAY_START, 0xa, 256*sizeof(uint8_t));
    // add a "clear display" instruction
    memory[ROM_OFFSET] = 0x0;
    memory[ROM_OFFSET+1] = 0xe0;
    // process
    processOp(&chip8State, memory);
    // validate
    uint8_t expected[256];
    memset(expected, 0, 256*sizeof(uint8_t));
    // ensure we have incremented the program counter to point to the next instruction
    assert_int_equal(ROM_OFFSET+2, chip8State.pc);
    // we should set the draw flag since we (presumably) modified the pixels
    assert_true(chip8State.draw);
    // and that the memory is indeed zero
    assert_memory_equal(memory+MEM_DISPLAY_START, expected, 256);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_clear_display),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
