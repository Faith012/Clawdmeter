// Host unit test for the PWR double-press detector:
//
//   pio test -d firmware -e native -f test_double_press

#include "double_press.h"
#include <unity.h>

#define CHECK(cond) TEST_ASSERT_TRUE(cond)

void setUp(void) {}
void tearDown(void) {}

static const uint32_t W = 450;

static void a_lone_press_lands_as_single_once_the_window_closes() {
    DoublePress d = {};
    CHECK(double_press_feed(&d, 1000, true, W) == PRESS_NONE);
    CHECK(double_press_feed(&d, 1000 + W, false, W) == PRESS_NONE);   // still inside
    CHECK(double_press_feed(&d, 1001 + W, false, W) == PRESS_SINGLE);
    CHECK(double_press_feed(&d, 2000, false, W) == PRESS_NONE);       // only once
}

static void two_presses_inside_the_window_are_one_double() {
    DoublePress d = {};
    CHECK(double_press_feed(&d, 1000, true, W) == PRESS_NONE);
    CHECK(double_press_feed(&d, 1300, true, W) == PRESS_DOUBLE);
    CHECK(double_press_feed(&d, 5000, false, W) == PRESS_NONE);       // no single after it
}

static void a_second_press_after_the_window_starts_over() {
    DoublePress d = {};
    double_press_feed(&d, 1000, true, W);
    CHECK(double_press_feed(&d, 1500, false, W) == PRESS_SINGLE);
    CHECK(double_press_feed(&d, 1600, true, W) == PRESS_NONE);
    CHECK(double_press_feed(&d, 1700, true, W) == PRESS_DOUBLE);
}

static void a_late_second_press_without_a_poll_keeps_the_single() {
    DoublePress d = {};
    double_press_feed(&d, 1000, true, W);
    // No poll in between: the first press's window has passed.
    CHECK(double_press_feed(&d, 2000, true, W) == PRESS_SINGLE);
    CHECK(double_press_feed(&d, 2100, true, W) == PRESS_DOUBLE);      // the new press is pending
}

static void millis_wrapping_around_is_fine() {
    DoublePress d = {};
    double_press_feed(&d, 0xFFFFFF00u, true, W);
    CHECK(double_press_feed(&d, 0x00000040u, true, W) == PRESS_DOUBLE);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(a_lone_press_lands_as_single_once_the_window_closes);
    RUN_TEST(two_presses_inside_the_window_are_one_double);
    RUN_TEST(a_second_press_after_the_window_starts_over);
    RUN_TEST(a_late_second_press_without_a_poll_keeps_the_single);
    RUN_TEST(millis_wrapping_around_is_fine);
    return UNITY_END();
}
