#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Minimal Intel 8253 PIT implementation targeting MZ-800 needs.

typedef struct {
    uint16_t initial_value;
    uint16_t value;
    uint16_t latch;
    
    uint8_t mode;
    uint8_t bcd;
    uint8_t rl; // Read/Load mode
    uint8_t state; // LSB/MSB state tracking
    bool latched;
    
    bool output;
    bool gate;
    bool load_done;
    bool null_count;
} i8253_channel_t;

typedef struct {
    i8253_channel_t channels[3];
} i8253_t;

void i8253_init(i8253_t* pit);
void i8253_reset(i8253_t* pit);
void i8253_write(i8253_t* pit, uint16_t port, uint8_t data);
uint8_t i8253_read(i8253_t* pit, uint16_t port);
void i8253_tick(i8253_t* pit, uint8_t channel);

#ifdef __cplusplus
}
#endif
