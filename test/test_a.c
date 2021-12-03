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
    for (int i = 0l; i < 0xf; i++)
    {
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

static void test_bitwise_shift(void **state)
{
    /*
    The test ROM will look like this:
        0x0200 0x61f0 # set register 1 to 0xf0
        0x0202 0x810e # store the MSB in register 0xf, left shift by 1
        0x0204 0x6f00 # set regsiter 0xf to 0
        0x0206 0x6201 # set register 1 to 0x01
        0x0208 0x8206 # store the LSB in register 0xf, right shift by 1
    */

    // init
    State chip8State = {.pc = ROM_OFFSET};
    uint8_t memory[MEM_SIZE];
    memset(memory, 0x0, MEM_SIZE * sizeof(uint8_t));
    uint8_t rom[] = {0x61, 0xf0, 0x81, 0x0e, 0x6f, 0x0, 0x62, 0x01, 0x82, 0x06};
    memcpy(memory + ROM_OFFSET, rom, sizeof(rom[0]) * 10);

    processOp(&chip8State, memory);
    // left shift
    processOp(&chip8State, memory);
    assert_int_equal(chip8State.registers[1], 0xe0);
    // the "overflown" bit should be stored in 0xf
    assert_int_equal(chip8State.registers[0xf], 0x1);
    // reset 0xf
    processOp(&chip8State, memory);
    assert_int_equal(chip8State.registers[0xf], 0x0);
    // store 1 in r2
    processOp(&chip8State, memory);
    // bitshift to the right
    processOp(&chip8State, memory);
    assert_int_equal(chip8State.registers[2], 0x0);
    assert_int_equal(chip8State.registers[0xf], 0x1);
}

static void test_register_maths(void **state)
{
    /*
    The test ROM will look like this:
        0x0200 0x61f0 # set register 1 to 0xf0
        0x0202 0x6210 # set register 2 to 0x10
        0x0204 0x8124 # add r2 to r1
        0x0206 0x8f00 # set register f to 0x0
        0x0208 0x6301 # set register 3 to 0xf0
        0x020a 0x640f # set register 4 to 0x10
        0x020c 0x8345 # subtract r4 from r3
        0x020e 0x8335 # subtract r3 from r3
        0x0210 0x6502 # set register 5 to 0x1
        0x0212 0x6601 # set register 6 to 0x2
        0x0214 0x8567 # subtract r5 from r6 and store in r5
    */

    // init
    State chip8State = {.pc = ROM_OFFSET};
    uint8_t memory[MEM_SIZE];
    memset(memory, 0x0, MEM_SIZE * sizeof(uint8_t));
    uint8_t rom[] = {0x61, 0xf0, 0x62, 0x10, 0x81, 0x24, 0x6f, 0x0, 0x63, 0x01, 0x64, 0x0f, 0x83, 0x45, 0x83, 0x35, 0x65, 0x02, 0x66, 0x01, 0x85, 0x67};
    memcpy(memory + ROM_OFFSET, rom, sizeof(rom[0]) * 22);

    // set both registers
    processOp(&chip8State, memory);
    processOp(&chip8State, memory);
    // perform the addition
    processOp(&chip8State, memory);
    assert_int_equal(chip8State.registers[1], 0x00);
    // carry flag should be set
    assert_int_equal(chip8State.registers[0xf], 0x1);
    // reset flag register
    processOp(&chip8State, memory);
    // set both registers
    processOp(&chip8State, memory);
    processOp(&chip8State, memory);
    // perform the addition
    processOp(&chip8State, memory);
    assert_int_equal(chip8State.registers[3], 0xf1);
    // there *was* a borrow, so this should be 0
    assert_int_equal(chip8State.registers[0xf], 0x0);
    // subtract r3 from r3
    processOp(&chip8State, memory);
    assert_int_equal(chip8State.registers[3], 0x0);
    // there wasn't any borrow, so this should be 1
    assert_int_equal(chip8State.registers[0xf], 0x1);
    // set the registers
    processOp(&chip8State, memory);
    processOp(&chip8State, memory);
    // subtract r6 from r5
    processOp(&chip8State, memory);
    assert_int_equal(chip8State.registers[5], 0xfe);
    assert_int_equal(chip8State.registers[0xf], 0x0);
}

static void test_keyboard(void **state)
{
    /*
    The test ROM will look like this:
        0x0200 0xe19e # skip the next instruction if key 0x1 is down
        0x0202 0x6001 # set register r0 to 1
        0x0204 0xe1a1 # skip the next instruction if key 0x1 is *not* down

    This test is a little different in that state will be changed from the outside.
    */

    // init
    State chip8State = {.pc = ROM_OFFSET};
    uint8_t memory[MEM_SIZE];
    memset(memory, 0x0, MEM_SIZE * sizeof(uint8_t));
    uint8_t rom[] = {0xe1, 0x9e, 0x60, 0x01, 0xe1, 0xa1};
    memcpy(memory + ROM_OFFSET, rom, sizeof(rom[0]) * 6);

    // check that key 1 is depressed
    assert_int_equal(chip8State.input[1], false);
    // skip 0x0202 if the key is pressed (which it isn't)
    processOp(&chip8State, memory);
    assert_int_equal(chip8State.pc, 0x202);
    // set the register
    processOp(&chip8State, memory);
    // key 1 is still not pressed so we should skip the next instruction
    processOp(&chip8State, memory);
    assert_int_equal(chip8State.pc, 0x208);

    // now run through this again but setting the key as pressed
    State chip8State2 = {.pc = ROM_OFFSET};
    chip8State2.input[0x1] = true;

    // skip 0x0202 if the key is pressed (which it is this time)
    processOp(&chip8State2, memory);
    assert_int_equal(chip8State2.pc, 0x204);
    // key 1 is still pressed so we should proceed to the next instruction
    processOp(&chip8State2, memory);
    assert_int_equal(chip8State2.pc, 0x206);
}

static void test_keyboard_blocking(void **state)
{
    /*
    The test ROM will look like this:
        0x0200 0xf50a # wait until a key is pressed, store it in r5
    */

    // init
    State chip8State = {.pc = ROM_OFFSET};
    uint8_t memory[MEM_SIZE];
    memset(memory, 0x0, MEM_SIZE * sizeof(uint8_t));
    uint8_t rom[] = {0xf5, 0x0a};
    memcpy(memory + ROM_OFFSET, rom, sizeof(rom[0]) * 2);

    // wait for a key to be pressed
    processOp(&chip8State, memory);
    assert_int_equal(chip8State.pc, ROM_OFFSET);
    // simulate a key getting pressed
    chip8State.input[1] = true;
    // check again
    processOp(&chip8State, memory);
    // we should now proceed to the next instruction
    assert_int_equal(chip8State.pc, 0x202);
    // and register 5 should have value 1
    assert_int_equal(chip8State.registers[0x5], 0x1);
}

static void test_memory_set_i(void **state)
{
    /*
    The test ROM will look like this:
        0x0200 0xa123 # set i to 0x123
        0x0202 0x6102 # set r1 to 0x2
        0x0204 0xf11e # add the contents of r1 to i
    */

    // init
    State chip8State = {.pc = ROM_OFFSET};
    uint8_t memory[MEM_SIZE];
    memset(memory, 0x0, MEM_SIZE * sizeof(uint8_t));
    uint8_t rom[] = {0xa1, 0x23, 0x61, 0x02, 0xf1, 0x1e};
    memcpy(memory + ROM_OFFSET, rom, sizeof(rom[0]) * 6);

    // i should be 0 upon initialisation
    assert_int_equal(chip8State.i, 0x0);
    // i should now be 0x123
    processOp(&chip8State, memory);
    assert_int_equal(chip8State.i, 0x123);
    // set the register
    processOp(&chip8State, memory);
    // add to i
    processOp(&chip8State, memory);
    assert_int_equal(chip8State.i, 0x125);
}

static void test_memory_set_pc(void **state)
{
    /*
    The test ROM will look like this:
        0x0200 0x6002 # set r0 to 0x2
        0x0202 0xb123 # set the program counter to r0 + 0x123
    */

    // init
    State chip8State = {.pc = ROM_OFFSET};
    uint8_t memory[MEM_SIZE];
    memset(memory, 0x0, MEM_SIZE * sizeof(uint8_t));
    uint8_t rom[] = {0x60, 0x02, 0xb1, 0x23};
    memcpy(memory + ROM_OFFSET, rom, sizeof(rom[0]) * 6);

    // set the register
    processOp(&chip8State, memory);
    // set i
    processOp(&chip8State, memory);
    assert_int_equal(chip8State.pc, 0x125);
}

static void test_save_load_registers(void **state)
{
    /*
    The test ROM will look like this:
        0x0200 0xa300 # set i to 0x0300
        0x0200 0x6101 # set r1 to 0x1
        0x0200 0x6202 # set r2 to 0x2
        0x0200 0x6302 # set r3 to 0x3
        0x0202 0xf355 # store r0 to r3 at i
        0x0200 0x6109 # set r1 to 0x1
        0x0200 0x6208 # set r2 to 0x2
        0x0200 0x6307 # set r3 to 0x3
        0x0202 0xf365 # load r0 to r3 from i
    */

    // init
    State chip8State = {.pc = ROM_OFFSET};
    uint8_t memory[MEM_SIZE];
    memset(memory, 0x0, MEM_SIZE * sizeof(uint8_t));
    uint8_t rom[] = {0xa3, 0x00, 0x61, 0x01, 0x62, 0x02, 0x63, 0x03, 0xf3, 0x55, 0x61, 0x09, 0x62, 0x08, 0x63, 0x07, 0xf3, 0x65};
    memcpy(memory + ROM_OFFSET, rom, sizeof(rom[0]) * 18);

    // set the registers and save
    for (int i = 0; i < 5; i++)
    {
        processOp(&chip8State, memory);
    }
    uint8_t expected[] = {0x0, 0x1, 0x2, 0x3};
    assert_memory_equal(memory + chip8State.i, expected, 4);

    // we now set the registers to something else and load
    for (int i = 0; i < 4; i++)
    {
        processOp(&chip8State, memory);
    }
    for (int i = 0; i < 4; i++)
    {
        assert_int_equal(chip8State.registers[i], i);
    }
}

static void test_rand(void **state)
{
    /*
    The test ROM will look like this:
        0x0200 0xc50f # generate a random number between 0-255 & 0x0f
    */

    // init
    State chip8State = {.pc = ROM_OFFSET};
    uint8_t memory[MEM_SIZE];
    memset(memory, 0x0, MEM_SIZE * sizeof(uint8_t));
    uint8_t rom[] = {0xc5, 0x0f};
    memcpy(memory + ROM_OFFSET, rom, sizeof(rom[0]) * 2);

    processOp(&chip8State, memory);
    // set the seed so we always get the same random number
    srand(0xdeadbeef);
    assert_in_range(chip8State.registers[0x5],0x0,0xf);
    assert_int_equal(chip8State.registers[0x5],0x7);
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
        cmocka_unit_test(test_register_maths),
        cmocka_unit_test(test_keyboard),
        cmocka_unit_test(test_keyboard_blocking),
        cmocka_unit_test(test_memory_set_i),
        cmocka_unit_test(test_memory_set_pc),
        cmocka_unit_test(test_save_load_registers),
        cmocka_unit_test(test_rand),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}