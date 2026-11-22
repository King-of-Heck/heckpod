#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include "check.h"
#include "dsp/time_stretch.h"

enum {
    RATE = 48000,
    SECONDS = 2,
    INPUT_FRAMES = RATE * SECONDS,
    OUTPUT_CAPACITY = INPUT_FRAMES * 2 + 1024,
    AMPLITUDE = 4194304
};

static const double PI = 3.14159265358979323846;
static const double SAMPLE_RATE = RATE;
static const double PEAK = AMPLITUDE;
static int32_t memory[TIME_STRETCH_MEMORY_BYTES / 4];
static int32_t reference[OUTPUT_CAPACITY * 2];
static int32_t input[INPUT_FRAMES * 2];
static int32_t output[OUTPUT_CAPACITY * 2];

static void fill_sine(double frequency)
{
    for (uint32_t frame = 0; frame < INPUT_FRAMES; frame++) {
        int32_t sample = (int32_t)lround(PEAK * sin(2.0 * PI * frequency * frame / SAMPLE_RATE));
        input[2 * frame] = sample;
        input[2 * frame + 1] = sample;
    }
}

static uint32_t run(uint16_t speed_q8)
{
    TimeStretch stretch;
    CHECK(time_stretch_init(&stretch, memory, sizeof memory, RATE));
    time_stretch_set_speed(&stretch, speed_q8);
    uint32_t pushed = 0;
    uint32_t made = 0;
    while (pushed < INPUT_FRAMES) {
        uint32_t count =
            INPUT_FRAMES - pushed < TIME_STRETCH_CHUNK ? INPUT_FRAMES - pushed : TIME_STRETCH_CHUNK;
        pushed += time_stretch_push(&stretch, &input[2 * pushed], count);
        made += time_stretch_pull(&stretch, &output[2 * made], OUTPUT_CAPACITY - made);
    }
    made += time_stretch_pull(&stretch, &output[2 * made], OUTPUT_CAPACITY - made);
    return made;
}

static uint32_t run_in_pieces(uint16_t speed_q8, uint32_t piece)
{
    TimeStretch stretch;
    CHECK(time_stretch_init(&stretch, memory, sizeof memory, RATE));
    time_stretch_set_speed(&stretch, speed_q8);
    uint32_t pushed = 0;
    uint32_t made = 0;
    while (pushed < INPUT_FRAMES) {
        uint32_t count =
            INPUT_FRAMES - pushed < TIME_STRETCH_CHUNK ? INPUT_FRAMES - pushed : TIME_STRETCH_CHUNK;
        pushed += time_stretch_push(&stretch, &input[2 * pushed], count);
        uint32_t got;
        do {
            got = time_stretch_pull(&stretch, &output[2 * made], piece);
            made += got;
        } while (got > 0);
    }
    return made;
}

static int32_t largest_step(uint32_t from, uint32_t frames)
{
    int32_t largest = 0;
    for (uint32_t frame = from + 1; frame < frames; frame++) {
        int32_t step = abs(output[2 * frame] - output[2 * (frame - 1)]);
        largest = step > largest ? step : largest;
    }
    return largest;
}

static uint32_t zero_crossings(uint32_t frames)
{
    uint32_t count = 0;
    for (uint32_t frame = 1; frame < frames; frame++) {
        count += (output[2 * (frame - 1)] < 0) != (output[2 * frame] < 0) ? 1u : 0u;
    }
    return count;
}

static void refuses_bad_rates_and_small_memory(void)
{
    TimeStretch stretch;
    CHECK(!time_stretch_init(&stretch, memory, sizeof memory, 0));
    CHECK(!time_stretch_init(&stretch, memory, sizeof memory, 96000));
    CHECK(!time_stretch_init(&stretch, memory, 4096, RATE));
    CHECK(time_stretch_init(&stretch, memory, sizeof memory, 44100));
}

static void memory_constant_is_exact(void)
{
    TimeStretch stretch;
    CHECK(time_stretch_init(&stretch, memory, TIME_STRETCH_MEMORY_BYTES, RATE));
    CHECK(!time_stretch_init(&stretch, memory, TIME_STRETCH_MEMORY_BYTES - 1, RATE));
}

static void small_pulls_match_one_big_pull(void)
{
    fill_sine(440.0);
    uint32_t whole = run(384);
    CHECK(whole > 0);
    for (uint32_t index = 0; index < whole * 2; index++) {
        reference[index] = output[index];
    }
    uint32_t pieces = run_in_pieces(384, 7);
    CHECK(pieces == whole);
    for (uint32_t index = 0; index < whole * 2; index++) {
        CHECK(output[index] == reference[index]);
    }
}

static void output_length_follows_the_speed(void)
{
    static const uint16_t SPEEDS[] = {192, 256, 320, 384, 448, 512};
    fill_sine(440.0);
    for (size_t index = 0; index < sizeof SPEEDS / sizeof SPEEDS[0]; index++) {
        uint32_t made = run(SPEEDS[index]);
        uint32_t expected = (uint32_t)((uint64_t)INPUT_FRAMES * 256 / SPEEDS[index]);
        uint32_t slack = RATE * 60 / 1000;
        CHECK(made + slack >= expected);
        CHECK(made <= expected + slack);
    }
}

static void pitch_is_kept(void)
{
    fill_sine(440.0);
    static const uint16_t SPEEDS[] = {192, 512};
    for (size_t index = 0; index < 2; index++) {
        uint32_t made = run(SPEEDS[index]);
        double expected = 2.0 * 440.0 * made / SAMPLE_RATE;
        double counted = zero_crossings(made);
        CHECK(fabs(counted - expected) <= expected * 0.03);
    }
}

static void joins_are_smooth(void)
{
    fill_sine(440.0);
    uint32_t made = run(384);
    int32_t largest = largest_step(0, made);
    double natural = PEAK * 2.0 * PI * 440.0 / SAMPLE_RATE;
    CHECK(largest <= (int32_t)(natural * 2.0));
}

static void silence_and_full_scale_stay_in_range(void)
{
    for (uint32_t frame = 0; frame < INPUT_FRAMES; frame++) {
        int32_t sample = (frame / 100) % 2 == 0 ? 8388607 : -8388608;
        input[2 * frame] = frame < RATE ? 0 : sample;
        input[2 * frame + 1] = frame < RATE ? 0 : sample;
    }
    uint32_t made = run(512);
    int32_t peak = 0;
    for (uint32_t index = 0; index < made * 2; index++) {
        CHECK(output[index] >= -8388608 && output[index] <= 8388607);
        peak = abs(output[index]) > peak ? abs(output[index]) : peak;
    }
    for (uint32_t index = 0; index < RATE / 4 * 2; index++) {
        CHECK(output[index] == 0);
    }
    CHECK(peak > 8388607 / 10 * 9);
}

static void speed_can_change_mid_stream(void)
{
    fill_sine(440.0);
    TimeStretch stretch;
    CHECK(time_stretch_init(&stretch, memory, sizeof memory, RATE));
    uint32_t pushed = 0;
    uint32_t made = 0;
    while (pushed < INPUT_FRAMES) {
        if (pushed >= INPUT_FRAMES / 2) {
            time_stretch_set_speed(&stretch, 512);
        }
        pushed += time_stretch_push(&stretch, &input[2 * pushed], TIME_STRETCH_CHUNK);
        made += time_stretch_pull(&stretch, &output[2 * made], OUTPUT_CAPACITY - made);
    }
    uint32_t expected = INPUT_FRAMES / 2 + INPUT_FRAMES / 4;
    CHECK(made + RATE / 20 >= expected && made <= expected + RATE / 20);
    double natural = PEAK * 2.0 * PI * 440.0 / SAMPLE_RATE;
    CHECK(largest_step(0, made) <= (int32_t)(natural * 2.0));
}

int main(void)
{
    static const TestCase CASES[] = {
        TEST_CASE(refuses_bad_rates_and_small_memory),
        TEST_CASE(memory_constant_is_exact),
        TEST_CASE(small_pulls_match_one_big_pull),
        TEST_CASE(output_length_follows_the_speed),
        TEST_CASE(pitch_is_kept),
        TEST_CASE(joins_are_smooth),
        TEST_CASE(silence_and_full_scale_stay_in_range),
        TEST_CASE(speed_can_change_mid_stream),
    };
    return test_run_all(CASES, TEST_CASE_COUNT(CASES));
}
