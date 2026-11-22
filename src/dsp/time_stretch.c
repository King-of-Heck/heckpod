#include "dsp/time_stretch.h"

enum {
    HOP_DIVISOR = 100,
    SEARCH_PER_MILLE = 6,
    PER_MILLE = 1000,
    CHANNELS = 2,
    FRAME_BYTES = CHANNELS * (int)sizeof(int16_t),
    SAMPLE_SHIFT_SCALE = 256,
    COARSE_LAG = 4,
    COARSE_STEP = 8,
    FINE_RADIUS = 3,
    FINE_STEP = 2,
    FRACTION_BITS = 8,
    FRACTION_MASK = 0xFF,
    INT16_TOP = 32767,
    INT16_BOTTOM = -32768
};

static int16_t narrow(int32_t sample)
{
    int32_t value = sample / SAMPLE_SHIFT_SCALE;
    return (int16_t)(value > INT16_TOP ? INT16_TOP : value < INT16_BOTTOM ? INT16_BOTTOM : value);
}

bool time_stretch_init(TimeStretch *stretch, void *memory, uint32_t bytes, uint32_t rate)
{
    if (rate == 0 || rate > TIME_STRETCH_MAX_RATE) {
        return false;
    }
    uint32_t hop = rate / HOP_DIVISOR;
    uint32_t search = rate * SEARCH_PER_MILLE / PER_MILLE;
    uint32_t capacity = 2 * search + 2 * hop + TIME_STRETCH_CHUNK;
    if (hop == 0 || (uint64_t)(capacity + hop) * FRAME_BYTES > bytes) {
        return false;
    }
    *stretch = (TimeStretch){.input = memory,
                             .tail = (int16_t *)memory + CHANNELS * capacity,
                             .capacity = capacity,
                             .hop = hop,
                             .search = search,
                             .speed_q8 = TIME_STRETCH_UNITY};
    return true;
}

void time_stretch_set_speed(TimeStretch *stretch, uint16_t speed_q8)
{
    stretch->speed_q8 = speed_q8;
}

uint32_t time_stretch_push(TimeStretch *stretch, const int32_t *frames, uint32_t count)
{
    uint32_t room = stretch->capacity - stretch->filled;
    uint32_t take = count < room ? count : room;
    int16_t *to = stretch->input + CHANNELS * stretch->filled;
    for (uint32_t index = 0; index < take * CHANNELS; index++) {
        to[index] = narrow(frames[index]);
    }
    stretch->filled += take;
    return take;
}

static uint32_t difference(const TimeStretch *stretch, uint32_t start, uint32_t step,
                           uint32_t limit)
{
    uint32_t total = 0;
    for (uint32_t index = 0; index < stretch->hop && total < limit; index += step) {
        const int16_t *tail = stretch->tail + CHANNELS * index;
        const int16_t *input = stretch->input + CHANNELS * (start + index);
        int32_t gap = (int32_t)tail[0] + tail[1] - input[0] - input[1];
        total += (uint32_t)(gap < 0 ? -gap : gap);
    }
    return total;
}

static uint32_t best_start(const TimeStretch *stretch)
{
    uint32_t span = 2 * stretch->search;
    uint32_t best = stretch->search;
    uint32_t best_cost = UINT32_MAX;
    for (uint32_t start = 0; start <= span; start += COARSE_LAG) {
        uint32_t cost = difference(stretch, start, COARSE_STEP, best_cost);
        if (cost < best_cost) {
            best_cost = cost;
            best = start;
        }
    }
    uint32_t low = best > FINE_RADIUS ? best - FINE_RADIUS : 0;
    uint32_t high = best + FINE_RADIUS < span ? best + FINE_RADIUS : span;
    best_cost = UINT32_MAX;
    for (uint32_t start = low; start <= high; start++) {
        uint32_t cost = difference(stretch, start, FINE_STEP, best_cost);
        if (cost < best_cost) {
            best_cost = cost;
            best = start;
        }
    }
    return best;
}

static bool ready(const TimeStretch *stretch)
{
    return stretch->filled >= 2 * stretch->search + 2 * stretch->hop;
}

static void finish_hop(TimeStretch *stretch)
{
    __builtin_memcpy(stretch->tail, stretch->input + CHANNELS * (stretch->start + stretch->hop),
                     stretch->hop * FRAME_BYTES);
    stretch->primed = true;
    stretch->emitting = false;
    uint32_t advance = stretch->hop * stretch->speed_q8 + stretch->fraction;
    uint32_t drop = advance >> FRACTION_BITS;
    stretch->fraction = advance & FRACTION_MASK;
    __builtin_memmove(stretch->input, stretch->input + CHANNELS * drop,
                      (stretch->filled - drop) * FRAME_BYTES);
    stretch->filled -= drop;
}

uint32_t time_stretch_pull(TimeStretch *stretch, int32_t *frames, uint32_t max)
{
    uint32_t made = 0;
    while (made < max) {
        if (!stretch->emitting) {
            if (!ready(stretch)) {
                break;
            }
            stretch->start = stretch->primed ? best_start(stretch) : stretch->search;
            stretch->emitted = 0;
            stretch->emitting = true;
        }
        while (made < max && stretch->emitted < stretch->hop) {
            uint32_t index = stretch->emitted;
            const int16_t *input = stretch->input + CHANNELS * (stretch->start + index);
            for (uint32_t channel = 0; channel < CHANNELS; channel++) {
                int32_t sample = input[channel];
                if (stretch->primed) {
                    int32_t from = stretch->tail[CHANNELS * index + channel];
                    sample = from + (sample - from) * (int32_t)index / (int32_t)stretch->hop;
                }
                frames[CHANNELS * made + channel] = sample * SAMPLE_SHIFT_SCALE;
            }
            stretch->emitted++;
            made++;
        }
        if (stretch->emitted == stretch->hop) {
            finish_hop(stretch);
        }
    }
    return made;
}
