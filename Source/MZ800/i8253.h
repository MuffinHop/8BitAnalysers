#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Intel 8253 PIT — full state machine matching MZ-800 hardware behaviour.

typedef enum {
    I8253_RLF_LSB    = 1,
    I8253_RLF_MSB    = 2,
    I8253_RLF_LSBMSB = 3
} i8253_rlf_t;

typedef enum {
    I8253_MODE0 = 0,  // Interrupt on Terminal Count
    I8253_MODE1,      // Programmable One-Shot
    I8253_MODE2,      // Rate Generator
    I8253_MODE3,      // Square Wave Generator
    I8253_MODE4,      // Software Triggered Strobe
    I8253_MODE5       // Hardware Triggered Strobe
} i8253_mode_t;

typedef enum {
    I8253_STATE_INIT,               // CW written, waiting for first CLK fall
    I8253_STATE_INIT_DONE,          // CW written, first CLK fall arrived
    I8253_STATE_LOAD,               // LOAD begun (mode 0 mid-count reload)
    I8253_STATE_PRESET_ERROR,       // preset==1 in mode 2 — can't count
    I8253_STATE_LOAD_DONE,          // LOAD complete, PRESET follows unconditionally
    I8253_STATE_WAIT_GATE1,         // waiting for GATE=1
    I8253_STATE_PRESET,             // about to load preset_value into value
    I8253_STATE_PRESET32,           // about to load constant 32 into value (mode 1 error recovery)
    I8253_STATE_COUNTDOWN,          // counting toward target
    I8253_STATE_MODE1_TRIGGER_ERR,  // mode 1: GATE triggered before LOAD done
    I8253_STATE_BLIND_COUNT         // counting without target (after terminal count)
} i8253_state_t;

typedef struct {
    uint32_t value;               // current counter value
    uint32_t preset_value;        // loaded preset (0→0x10000)
    uint32_t preset_latch;        // intermediate latch while loading LSB/MSB
    uint32_t read_latch;          // latched value for read-back
    uint32_t mode3_half_value;    // half of preset (for square wave)
    uint32_t mode3_destination_value; // target value toggles 0 ↔ half

    uint8_t  out;                 // output state: 0 or 1
    uint8_t  gate;                // gate input: 0 or 1
    i8253_mode_t  mode;
    uint8_t  bcd;
    i8253_rlf_t   rlf;            // Read/Load format
    i8253_state_t state;
    uint8_t  load_done;           // deferred load-done flag (set during INIT)

    uint8_t  rl_byte;             // LSB/MSB flip-flop for R/W (shared register on real HW)
    uint8_t  latch_op;            // 1 = read_latch valid
} i8253_channel_t;

typedef struct {
    i8253_channel_t channels[3];
} i8253_t;

void    i8253_init(i8253_t* pit);
void    i8253_reset(i8253_t* pit);
void    i8253_write(i8253_t* pit, uint16_t port, uint8_t data);
uint8_t i8253_read(i8253_t* pit, uint16_t port);
void    i8253_tick(i8253_t* pit, uint8_t channel);
void    i8253_gate(i8253_t* pit, uint8_t channel, uint8_t gate_value);

#ifdef __cplusplus
}
#endif
