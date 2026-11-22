#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    TIME_STRETCH_UNITY = 256,
    TIME_STRETCH_MAX_RATE = 48000,
    TIME_STRETCH_CHUNK = 32,
    TIME_STRETCH_MEMORY_BYTES = 8320
};

typedef struct TimeStretch {
    int16_t *input;
    int16_t *tail;
    uint32_t capacity;
    uint32_t hop;
    uint32_t search;
    uint32_t filled;
    uint32_t start;
    uint32_t emitted;
    uint32_t fraction;
    uint16_t speed_q8;
    bool emitting;
    bool primed;
} TimeStretch;

bool time_stretch_init(TimeStretch *stretch, void *memory, uint32_t bytes, uint32_t rate);
void time_stretch_set_speed(TimeStretch *stretch, uint16_t speed_q8);
uint32_t time_stretch_push(TimeStretch *stretch, const int32_t *frames, uint32_t count);
uint32_t time_stretch_pull(TimeStretch *stretch, int32_t *frames, uint32_t max);
