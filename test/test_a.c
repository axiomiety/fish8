#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#include "mylib.h"

static void test_dummy(void **state) {
    assert_int_equal(0xff, 0xff);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_dummy),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}


