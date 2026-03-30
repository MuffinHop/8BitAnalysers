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

// --- Z80 PIO (PIOZ80) --- ports 0xFC-0xFF
// Port A: bit0-1 = 0x03 always, bit4 = ~CTC0_output, bit5 = vblank
// Port B: general-purpose (open)
typedef struct {
    uint8_t port_a_out;   // Port A output latch (written by CPU)
    uint8_t port_b_out;   // Port B output latch
    uint8_t io_mask_a;    // Port A IO mask: 1=input, 0=output (default 0xFF = all inputs)
    uint8_t io_mask_b;    // Port B IO mask
    uint8_t int_vector;   // Mode 2 interrupt vector
    uint8_t ctrl_word_a;  // Last control word for port A
    uint8_t ctrl_word_b;  // Last control word for port B
    bool    int_pending;  // Interrupt pending from PIOZ80
} pioz80_t;

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
// NTPL=0 (low) = PAL:  CLK=17,734,475 Hz, CPU=CLK/5=3,546,895 Hz, 1 CPU tick = 5 pixel clocks
// NTPL=1 (high)= NTSC: CLK=14,318,180 Hz, CPU=CLK/4=3,579,545 Hz, 1 CPU tick = 4 pixel clocks
// Both give ~3.55 MHz CPU — NTPL selects /4 or /5 divider path so CPU speed is preserved.
// CTC0 = CLK/16 (pixel clocks — same divisor in px clocks for both standards).
// PSG  = 16 CPU ticks = 80 px (PAL, 16×5) or 64 px (NTSC, 16×4).
typedef struct {
    uint32_t cpu_divider;         // Pixel clocks per CPU tick: PAL=5, NTSC=4
    uint32_t psg_pixel_divider;   // Pixel clocks per PSG step: PAL=80, NTSC=64
    uint32_t lines_per_frame;     // PAL=312, NTSC=262
    uint32_t pixels_per_line;     // PAL=1136, NTSC=912
    uint32_t tempo_toggle_lines;  // PAL=229, NTSC=231 (both ≈34 Hz: lines×fps÷(2×34))

    // Horizontal timing (pixel clock positions within line)
    uint32_t hsync_width;         // PAL=80, NTSC=64
    uint32_t hblank_start;        // first pixel of HBLANK (after display)
    uint32_t hblank_end;          // last pixel of HBLANK (before display starts)
    uint32_t display_start_col;   // first visible pixel column

    // Vertical timing (scanline numbers)
    uint32_t vblank_first_line;   // PAL=288, NTSC=242
    uint32_t vblank_resume_line;  // PAL=22, NTSC=20  (display resumes here)
    uint32_t vsync_first_line;    // PAL=309, NTSC=259

    // Canvas geometry
    uint32_t border_top;          // PAL=46, NTSC=20
    uint32_t border_bottom;       // PAL=42, NTSC=20
    uint32_t display_height;      // PAL=288, NTSC=240 (borders + 200 canvas)
} video_timing_t;

typedef struct {
    z80_t cpu;
    uint64_t pins;
    i8255_t ppi;
    i8253_t pit;
    kbd_t kbd;
    psg_t psg;
    pioz80_t pioz80;
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

    // Video standard
    bool    bPAL;          // true=PAL (50Hz, 312 lines), false=NTSC (60Hz, 262 lines)
    video_timing_t vt;     // current timing parameters (derived from bPAL)

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

    // Video timing — parameterized by vt (PAL or NTSC)
    // NTPL selects /4 (NTSC) or /5 (PAL) CPU divider; 1 CPU tick = vt.cpu_divider pixel clocks.
    // Mid-frame PAL↔NTSC switching: counters wrap naturally at new limits.
    uint32_t line_counter;   // Current scanline (0..vt.lines_per_frame-1)
    uint32_t line_cycle;     // CPU cycle within current line (derived)
    uint32_t pixel_tick;     // Sub-CPU pixel clock accumulator (0..4)
    uint32_t pixel_line;     // Pixel clock position within line (0..vt.pixels_per_line-1)
    uint32_t ctc0_sub;       // CTC0 fractional accumulator (counts pixel clocks 0..15)
    uint32_t psg_sub;        // PSG fractional accumulator (counts pixel clocks 0..vt.psg_pixel_divider-1)
    uint32_t tempo;          // Tempo counter (bit 0 = TEMPO signal)
    uint32_t tempo_divider;  // Tempo divider: toggles every 229 scanlines
    bool     vblank_active;  // True when in vertical blanking interval (fed to PIOZ80 PA5)
    uint8_t  ctc1_prev_out;  // Previous CTC1 output state (for falling-edge detection → clock CTC2)
    uint8_t  ctc0_prev_out;  // Previous CTC0 output state (for edge detection → PIOZ80 PA4)

    // DBUS latch (last byte on data bus — returned for unconnected reads)
    uint8_t dbus_latch;
} mz800_sys_t;

extern mz800_sys_t g_mz800_sys;

// --- Core system ---
void mz800_sys_init(mz800_sys_t* sys);
void mz800_sys_tick(mz800_sys_t* sys);
void mz800_sys_reset(mz800_sys_t* sys);
void mz800_sys_key_down(mz800_sys_t* sys, int key);
void mz800_sys_key_up(mz800_sys_t* sys, int key);

// --- GDG / video (mz800_gdg.c) ---
void     mz800_set_video_standard(mz800_sys_t* sys, bool pal);
void     mz800_hwscroll_regs_changed(mz800_sys_t* sys);
uint16_t mz800_hwscroll_addr(mz800_sys_t* sys, uint16_t addr);
uint8_t  mz800_vram_read(mz800_sys_t* sys, uint16_t vaddr, int addr_is_odd, bool dmd_scrw640, bool dmd_hicolor);
void     mz800_vram_write(mz800_sys_t* sys, uint16_t vaddr, uint8_t data, int addr_is_odd, bool dmd_scrw640, bool dmd_hicolor);

// --- PSG SN76489AN (mz800_psg.c) ---
void psg_step(psg_t* psg);
void mz800_psg_write_byte(psg_t* psg, uint8_t value);

// --- PIOZ80 (mz800_pioz80.c) ---
// Returns port A data: bits 0-1=0x03, bit4=~CTC0, bit5=VBLN
uint8_t  pioz80_read(pioz80_t* pio, uint8_t addr, mz800_sys_t* sys);
void     pioz80_write(pioz80_t* pio, uint8_t addr, uint8_t value);
void     pioz80_init(pioz80_t* pio);

// --- CMT hack (mz800_cmt.c) ---
// Load an MZF file; ROM patches on OUT 0x01/0x02 will use this data.
bool mz800_sys_load_mzf(mz800_sys_t* sys, const uint8_t* data, uint32_t size);

#ifdef __cplusplus
}
#endif
