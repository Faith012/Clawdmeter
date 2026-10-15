// Host unit test for direct_sprite_diff: which cells a directly drawn sprite
// hands back to LVGL and which it paints, frame to frame.
//   pio test -d firmware -e native -f test_direct_sprite

#include "direct_sprite.h"
#include <unity.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

struct Runs {
    int n;
    int r[16], c0[16], c1[16];
};
struct Log { Runs back, paint; };

static void on_back(void* ctx, int r, int c0, int c1) {
    Runs& x = ((Log*)ctx)->back;
    x.r[x.n] = r; x.c0[x.n] = c0; x.c1[x.n] = c1; x.n++;
}
static void on_paint(void* ctx, int r, int c0, int c1) {
    Runs& x = ((Log*)ctx)->paint;
    x.r[x.n] = r; x.c0[x.n] = c0; x.c1[x.n] = c1; x.n++;
}

static Log diff(const char* drawn, const char* next, int cols, int rows, bool all = false) {
    uint8_t d[64], n[64];
    for (int i = 0; i < cols * rows; i++) { d[i] = drawn[i] - '0'; n[i] = next[i] - '0'; }
    Log log;
    memset(&log, 0, sizeof(log));
    direct_sprite_diff(d, n, cols, rows, all, on_back, on_paint, &log);
    return log;
}

static void first_frame_paints_its_coloured_runs(void) {
    Log l = diff("000000"
                 "000000",
                 "011010"
                 "000330", 6, 2);
    TEST_ASSERT_EQUAL_INT(0, l.back.n);
    TEST_ASSERT_EQUAL_INT(3, l.paint.n);
    TEST_ASSERT_TRUE(l.paint.r[0] == 0 && l.paint.c0[0] == 1 && l.paint.c1[0] == 3);
    TEST_ASSERT_TRUE(l.paint.r[1] == 0 && l.paint.c0[1] == 4 && l.paint.c1[1] == 5);
    TEST_ASSERT_TRUE(l.paint.r[2] == 1 && l.paint.c0[2] == 3 && l.paint.c1[2] == 5);
}

static void a_step_right_hands_back_the_trailing_cell(void) {
    Log l = diff("0220", "0022", 4, 1);
    TEST_ASSERT_EQUAL_INT(1, l.back.n);
    TEST_ASSERT_TRUE(l.back.c0[0] == 1 && l.back.c1[0] == 2);    // left behind
    TEST_ASSERT_EQUAL_INT(1, l.paint.n);
    TEST_ASSERT_TRUE(l.paint.c0[0] == 3 && l.paint.c1[0] == 4);  // only the new cell
}

static void unchanged_cells_are_left_alone(void) {
    Log l = diff("1234", "1234", 4, 1);
    TEST_ASSERT_EQUAL_INT(0, l.back.n);
    TEST_ASSERT_EQUAL_INT(0, l.paint.n);
}

static void a_new_palette_repaints_every_coloured_cell(void) {
    Log l = diff("1204", "1204", 4, 1, true);
    TEST_ASSERT_EQUAL_INT(0, l.back.n);
    TEST_ASSERT_EQUAL_INT(2, l.paint.n);
    TEST_ASSERT_TRUE(l.paint.c0[0] == 0 && l.paint.c1[0] == 2);
    TEST_ASSERT_TRUE(l.paint.c0[1] == 3 && l.paint.c1[1] == 4);
}

static void erasing_hands_back_everything(void) {
    Log l = diff("1100"
                 "0111",
                 "0000"
                 "0000", 4, 2);
    TEST_ASSERT_EQUAL_INT(0, l.paint.n);
    TEST_ASSERT_EQUAL_INT(2, l.back.n);
    TEST_ASSERT_TRUE(l.back.r[0] == 0 && l.back.c0[0] == 0 && l.back.c1[0] == 2);
    TEST_ASSERT_TRUE(l.back.r[1] == 1 && l.back.c0[1] == 1 && l.back.c1[1] == 4);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(first_frame_paints_its_coloured_runs);
    RUN_TEST(a_step_right_hands_back_the_trailing_cell);
    RUN_TEST(unchanged_cells_are_left_alone);
    RUN_TEST(a_new_palette_repaints_every_coloured_cell);
    RUN_TEST(erasing_hands_back_everything);
    return UNITY_END();
}
