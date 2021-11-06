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
    memset(memory, 0x0, MEM_SIZE * sizeof(uint8_t));
    // let's turn on some pixels
    memset(memory + MEM_DISPLAY_START, 0xa, 256 * sizeof(uint8_t));
    // add a "clear display" instruction
    memory[ROM_OFFSET] = 0x0;
    memory[ROM_OFFSET + 1] = 0xe0;
    // process
    processOp(&chip8State, memory);
    // validate
    uint8_t expected[256];
    memset(expected, 0, 256 * sizeof(uint8_t));
    // ensure we have incremented the program counter to point to the next instruction
    assert_int_equal(ROM_OFFSET + 2, chip8State.pc);
    // we should set the draw flag since we (presumably) modified the pixels
    assert_true(chip8State.draw);
    // and that the memory is indeed zero
    assert_memory_equal(memory + MEM_DISPLAY_START, expected, 256);
}

static void test_flow(void **state)
{
    /*
    When performing a call, we need to:
    - push the next address on the stack
    - increment the stack pointer
    - set the PC to the address of the subroutine
    Returning is essentially those in reverse:
    - decrement the stack pointer
    - load the address pointed to by the stack pointer into the PC

    The test ROM will look like this
        0x0200  0x00e0 # clear the screen
        0x0202  0x00ee # return
        0x0204  0x2200 # call the subroutine at 0x0200
        0x0206  0x1204 # jump to 0x204

    So after processing 4 operations, we should have PC set back to 0x0204
    */

    // here we're starting at the jump instruction
    State chip8State = {.pc = ROM_OFFSET + 4};
    uint8_t memory[MEM_SIZE];
    memset(memory, 0x0, MEM_SIZE * sizeof(uint8_t));
    uint8_t rom[] = {0x0, 0xe0, 0x0, 0xee, 0x22, 0x00, 0x12, 0x04};
    memcpy(memory + ROM_OFFSET, rom, sizeof(rom[0]) * 8);

    // process the jump
    processOp(&chip8State, memory);
    assert_int_equal(ROM_OFFSET, chip8State.pc);
    // we should have incremented our stack pointer by 1
    assert_int_equal(1, chip8State.sp);
    // and the first entry should be the return address (the one after the call)
    assert_int_equal(ROM_OFFSET + 6, chip8State.stack[0]);
    // clearing the screen - we test this elswhere
    processOp(&chip8State, memory);
    assert_int_equal(ROM_OFFSET + 2, chip8State.pc);
    // the return
    processOp(&chip8State, memory);
    // validate the return address is set correctly
    assert_int_equal(ROM_OFFSET + 6, chip8State.pc);
    // and that our stack pointer is back to 0
    assert_int_equal(0, chip8State.sp);
    // the unconditional jump
    processOp(&chip8State, memory);
    // we should have updated the program counter
    assert_int_equal(ROM_OFFSET + 4, chip8State.pc);
    // no change to the stack - this isn't a subroutine call
    assert_int_equal(0, chip8State.sp);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_clear_display),
        cmocka_unit_test(test_flow),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
