#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "chips/z80.h"
#include "chips/z80pio.h"
#include "chips/i8255.h"
#include "i8253.h"
#include "chips/kbd.h"
#include <chips/sn76489an.h>
#include <chips/gdg_whid65040.h>

#include <stdbool.h>
#include <stdint.h>

// --- CMT hack (MZF direct loading) ---
#define MZF_HEADER_SIZE 128

typedef struct {
    uint8_t  header[MZF_HEADER_SIZE]; // last loaded MZF header
    uint8_t* body;                     // pointer to body data (caller-owned)
    uint32_t body_size;                // body data size
    bool     header_loaded;            // header available for body read
    bool     has_file;                 // an MZF is loaded in the hack
} cmt_hack_t;

// --- Joystick ---
typedef struct {
    uint8_t state;  // bits: 0=up,1=down,2=left,3=right,4=trig1,5=trig2 (active low)
} joy_dev_t;

// --- Video timing (PAL/NTSC parameterized) ---
// Now fully encapsulated in gdg_whid65040_t (gdg_video_timing_t).
// The video_timing_t alias is kept for any remaining system-level references.
typedef gdg_video_timing_t video_timing_t;

typedef struct {
    z80_t cpu;
    uint64_t pins;
    i8255_t ppi;
    i8253_t pit;
    kbd_t kbd;
    sn76489an_t psg;
    z80pio_t pio;
    gdg_whid65040_t gdg;   // GDG WHID 65040-032 (video, VRAM, bank switching)
    cmt_hack_t cmt;
    joy_dev_t joy[2];

    uint32_t tick_count;
    uint8_t ram[64 * 1024];
    uint8_t rom[16 * 1024];     // 0x0000: MZ-700 monitor (4KB) + CGROM (4KB) + MZ-800 monitor (8KB)

    // CTC edge detection
    uint8_t  ctc1_prev_out;  // Previous CTC1 output state (for falling-edge detection -> clock CTC2)
    uint8_t  ctc0_prev_out;  // Previous CTC0 output state (for edge detection -> PIOZ80 PA4)

    // DBUS latch (last byte on data bus — returned for unconnected reads)
    uint8_t dbus_latch;

    // --- Audio ---
    struct {
        chips_audio_callback_t callback;
        int num_samples;        // samples per push (default 128)
        int sample_pos;         // write cursor in sample_buffer
        float sample_buffer[1024];
        int sample_period;      // fixed-point period (PSG ticks between audio samples)
        int sample_counter;     // fixed-point downcounter
    } audio;

    // --- IO write hooks (set by analyser for logging) ---
    void* hook_user;  // opaque pointer passed to hook callbacks
    void (*pit_write_hook)(void* user, uint16_t pc, uint16_t port, uint8_t data);
    void (*psg_write_hook)(void* user, uint16_t pc, uint8_t data);
    void (*gdg_write_hook)(void* user, uint16_t pc, uint16_t port, uint8_t data);

    // --- CPU tick hook (called after every z80_tick for code analysis integration) ---
    void (*cpu_tick_hook)(uint64_t pins, void* user_data);
} mz800_sys_t;

extern mz800_sys_t g_mz800_sys;

// --- Core system ---
void mz800_sys_init(mz800_sys_t* sys);
void mz800_sys_tick(mz800_sys_t* sys);
uint32_t mz800_sys_exec(mz800_sys_t* sys, uint32_t micro_seconds);
void mz800_sys_reset(mz800_sys_t* sys);
void mz800_sys_key_down(mz800_sys_t* sys, int key);
void mz800_sys_key_up(mz800_sys_t* sys, int key);
void mz800_sys_setup_audio(mz800_sys_t* sys, chips_audio_callback_t callback, int sample_rate, int num_samples);

// --- CMT hack (mz800_cmt.c) ---
// Load an MZF file; ROM patches on OUT 0x01/0x02 will use this data.
bool mz800_sys_load_mzf(mz800_sys_t* sys, const uint8_t* data, uint32_t size);

#ifdef __cplusplus
}
#endif
