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

static void test_const(void **state)
{
    /*
    The test ROM will look like this:
        0x0200 0x6001 # set register 0 to 0x1
        0x0202 0x6cab # set register c to to 0xab
        0x0204 0x70ff # add 0xff to register 0 - carry flag remains unchanged
    */

    // init
    State chip8State = {.pc = ROM_OFFSET};
    uint8_t memory[MEM_SIZE];
    memset(memory, 0x0, MEM_SIZE * sizeof(uint8_t));
    uint8_t rom[] = {0x60, 0x01, 0x6c, 0xab, 0x70, 0xff};
    memcpy(memory + ROM_OFFSET, rom, sizeof(rom[0]) * 6);

    // let's make sure our registers are all 0
    for (int i=0l;i < 0xf; i++){
        assert_int_equal(chip8State.registers[i], 0);
    }
    processOp(&chip8State, memory);
    // register 0 should be set to 1
    assert_int_equal(chip8State.registers[0], 0x1);
    processOp(&chip8State, memory);
    // register c should be set to 0xab
    assert_int_equal(chip8State.registers[0xc], 0xab);
    // add 0xff to register 0
    processOp(&chip8State, memory);
    // and adding such that it overflows discards any other digits
    assert_int_equal(chip8State.registers[0], 0x0);
    // and the carry flag hasn't changed
    assert_int_equal(chip8State.registers[0xf], 0x0);
}

static void test_cond(void **state)
{
    /*
    The test ROM will look like this:
        0x0200 0x31ab # compare register 1 to 0xab, jump over if equal to 0xab
        0x0202 0x61ab # set register 1 to 0xab
        0x0204 0x41ab # compare register 1 to 0xab, jump over if not equal to 0xab
        0x0206 0x62ab # set register 2 to 0xab
        0x0208 0x5100 # compare register 1 to register 0, jump over if equal
        0x020a 0x9120 # compare register 1 to register 2, jump over if equal
    */

    // init
    State chip8State = {.pc = ROM_OFFSET};
    uint8_t memory[MEM_SIZE];
    memset(memory, 0x0, MEM_SIZE * sizeof(uint8_t));
    uint8_t rom[] = {0x31, 0xab, 0x61, 0xab, 0x41, 0xab, 0x62, 0xab, 0x51, 0x0, 0x91, 0x20};
    memcpy(memory + ROM_OFFSET, rom, sizeof(rom[0]) * 12);

    processOp(&chip8State, memory);
    // we should not have skipped over 0x0202
    assert_int_equal(chip8State.pc, 0x202);
    // set register 1 to 0xab
    processOp(&chip8State, memory);
    assert_int_equal(chip8State.pc, 0x204);
    // compare
    processOp(&chip8State, memory);
    assert_int_equal(chip8State.pc, 0x206);
    // set register 2 to 0xab
    processOp(&chip8State, memory);
    assert_int_equal(chip8State.pc, 0x208);
    // compare register 1 and register 0, jump if equal
    processOp(&chip8State, memory);
    assert_int_equal(chip8State.pc, 0x20a);
    // compare register 1 and register 2, jump if not equal
    processOp(&chip8State, memory);
    // they're both the same so we should just be at the next isntruction
    assert_int_equal(chip8State.pc, 0x20c);
}

static void test_assign(void **state)
{
    /*
    The test ROM will look like this:
        0x0200 0x61ab # set register 1 to 0xab
        0x0202 0x8120 # set register 2 to the same value as register 1
        0x0204 0x5120 # compare regsiter 1 and 2, jump over if equal
    */

    // init
    State chip8State = {.pc = ROM_OFFSET};
    uint8_t memory[MEM_SIZE];
    memset(memory, 0x0, MEM_SIZE * sizeof(uint8_t));
    uint8_t rom[] = {0x61, 0xab, 0x81, 0x20, 0x51, 0x20};
    memcpy(memory + ROM_OFFSET, rom, sizeof(rom[0]) * 6);

    // make sure both regs are 0 at the start
    assert_int_equal(chip8State.registers[1], 0x0);
    assert_int_equal(chip8State.registers[2], 0x0);
    // assign 0xab to register 1
    processOp(&chip8State, memory);
    assert_int_equal(chip8State.registers[1], 0xab);
    // set register 2 to register 1
    processOp(&chip8State, memory);
    assert_int_equal(chip8State.registers[2], 0xab);
    // compare both registers
    processOp(&chip8State, memory);
    // they should be equal, so the pc should have incremented by 4
    assert_int_equal(chip8State.pc, 0x208);
}

static void test_bitwise_operators(void **state)
{
    /*
    The test ROM will look like this:
        0x0200 0x610f # set register 1 to 0x0f
        0x0202 0x62f0 # set register 2 to 0xf0
        0x0204 0x8121 # set register 1 to r1 | r2
        0x0206 0x8112 # set register 1 to r1 & r1
        0x0208 0x8113 # set register 1 to r1 ^ r1
    */

    // init
    State chip8State = {.pc = ROM_OFFSET};
    uint8_t memory[MEM_SIZE];
    memset(memory, 0x0, MEM_SIZE * sizeof(uint8_t));
    uint8_t rom[] = {0x61, 0x0f, 0x62, 0xf0, 0x81, 0x21, 0x81, 0x12, 0x81, 0x13};
    memcpy(memory + ROM_OFFSET, rom, sizeof(rom[0]) * 10);
    
    // set the regs
    processOp(&chip8State, memory);
    processOp(&chip8State, memory);
    // |=
    processOp(&chip8State, memory);
    assert_int_equal(chip8State.registers[1], 0xff);
    // &=
    processOp(&chip8State, memory);
    assert_int_equal(chip8State.registers[1], 0xff);
    // ^=
    processOp(&chip8State, memory);
    assert_int_equal(chip8State.registers[1], 0x0);
}

static void test_bitwise_shift(void  **state) {
    /*
    The test ROM will look like this:
        0x0200 0x610f # set register 1 to 0x0f
        0x0202 0x62f0 # set register 2 to 0xf0
        0x0204 0x8121 # set register 1 to r1 | r2
        0x0206 0x8112 # set register 1 to r1 & r1
        0x0208 0x8113 # set register 1 to r1 ^ r1
    */

    // init
    State chip8State = {.pc = ROM_OFFSET};
    uint8_t memory[MEM_SIZE];
    memset(memory, 0x0, MEM_SIZE * sizeof(uint8_t));
    uint8_t rom[] = {0x61, 0x0f, 0x62, 0xf0, 0x81, 0x21, 0x81, 0x12, 0x81, 0x13};
    memcpy(memory + ROM_OFFSET, rom, sizeof(rom[0]) * 10);
    
    processOp(&chip8State, memory);

}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_clear_display),
        cmocka_unit_test(test_flow),
        cmocka_unit_test(test_const),
        cmocka_unit_test(test_cond),
        cmocka_unit_test(test_assign),
        cmocka_unit_test(test_bitwise_operators),
        cmocka_unit_test(test_bitwise_shift),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
