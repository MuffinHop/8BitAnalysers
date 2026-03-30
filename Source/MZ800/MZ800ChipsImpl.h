#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "chips/z80.h"
#include "chips/i8255.h"
#include "i8253.h"
#include "chips/kbd.h"

#include <stdbool.h>
#include <stdint.h>

// --- PSG (SN76489AN) ---
#define PSG_CHANNELS 4

typedef struct {
    uint8_t  type;        // 0=tone, 1=noise
    uint16_t divider;     // 10-bit tone period (or noise config)
    uint16_t latch_div;   // latched low nibble for 2-byte tone writes
    uint8_t  attn;        // 4-bit attenuation (0=max, 15=off)
    uint16_t timer;       // countdown timer
    uint8_t  output;      // current output signal (0 or 1)
    // Noise-specific
    uint8_t  noise_div_type; // 0-3 (3 = use ch2 divider)
    uint8_t  noise_type;     // 0=periodic, 1=white
    uint16_t shift_reg;      // 16-bit LFSR
} psg_channel_t;

typedef struct {
    uint8_t       latch_cs;   // last latched channel (0-3)
    uint8_t       latch_attn; // last latch was attenuation? (0 or 1)
    psg_channel_t ch[PSG_CHANNELS];
} psg_t;

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

typedef struct {
    z80_t cpu;
    uint64_t pins;
    i8255_t ppi;
    i8253_t pit;
    kbd_t kbd;
    psg_t psg;
    cmt_hack_t cmt;
    joy_dev_t joy[2];

    uint32_t tick_count;
    uint8_t ram[64 * 1024];
    uint8_t rom[16 * 1024];     // 0x0000: MZ-700 monitor (4KB) + CGROM (4KB) + MZ-800 monitor (8KB)
    uint8_t vram[4][8 * 1024];  // 4 planes x 8KB each (I, II, III=EXVRAM, IV=EXVRAM)
    
    // Memory mapping flags
    bool rom_0000_on;    // 0x0000-0x0FFF Monitor ROM
    bool rom_1000_on;    // 0x1000-0x1FFF CGROM
    bool rom_e000_on;    // 0xE000-0xFFFF Monitor ROM (also gates memory-mapped IO at E000-E008)
    bool vram_on;        // VRAM mapped at 0x8000+ (MZ-800) / CG-RAM+attr at C000-DFFF (MZ-700)

    // GDG DMD register (4 bits)
    uint8_t gdg_dmd;     // bit3=MZ700, bit2=SCRW640, bit1=HICOLOR, bit0=VBANK
    bool    mz800_switch; // Hardware DIP switch: true=MZ-800 mode

    // GDG WF register (decomposed from port 0xCC)
    uint8_t gdg_wf_plane;  // WF plane mask (bits 0-3)
    uint8_t gdg_wf_mode;   // WF mode: 0=SINGLE,1=EXOR,2=OR,3=RESET,4=REPLACE,6=PSET
    uint8_t gdg_wfrf_vbank; // Shared VBANK bit 4 from WF or RF write

    // GDG RF register (decomposed from port 0xCD)
    uint8_t gdg_rf_plane;  // RF plane mask (bits 0-3)
    uint8_t gdg_rf_search; // RF search mode (bit 7): 0=SINGLE, 1=SEARCH

    // GDG palette
    uint8_t gdg_palgrp;    // Palette group (0-3)
    uint8_t gdg_pal[4];    // PAL0-PAL3 IGRB nibbles
    uint8_t gdg_border;    // Border color IGRB nibble

    // GDG hardware scroll (decomposed register values)
    int     gdg_sof;       // Scroll offset (13-bit: SOF_HI:SOF_LO << 3)
    int     gdg_sw;        // Scroll width
    int     gdg_ssa;       // Scroll start address
    int     gdg_sea;       // Scroll end address
    bool    gdg_scroll_on; // Scroll enabled (derived from register validation)

    // Video timing — PAL: 312 lines x 227 cy/line
    uint32_t line_counter;   // Current scanline (0..311)
    uint32_t line_cycle;     // Cycle within current line (0..226)
    uint32_t tempo;          // Tempo counter (bit 0 = TEMPO signal)
    uint32_t tempo_divider;  // Tempo divider counter

    // DBUS latch (last byte on data bus — returned for unconnected reads)
    uint8_t dbus_latch;
} mz800_sys_t;

extern mz800_sys_t g_mz800_sys;

void mz800_sys_init(mz800_sys_t* sys);
void mz800_sys_tick(mz800_sys_t* sys);
void mz800_sys_reset(mz800_sys_t* sys);
void mz800_sys_key_down(mz800_sys_t* sys, int key);
void mz800_sys_key_up(mz800_sys_t* sys, int key);

// CMT hack: load an MZF file into the hack buffer (call before emulation or from UI)
// Returns true on success. After this, the ROM hack patches on OUT 0x01/0x02 will use this data.
bool mz800_sys_load_mzf(mz800_sys_t* sys, const uint8_t* data, uint32_t size);

// PSG step: advance all PSG channels by one PSG clock
void psg_step(psg_t* psg);

#ifdef __cplusplus
}
#endif
