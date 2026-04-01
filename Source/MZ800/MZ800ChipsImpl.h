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

    // Active graphics pixel boundaries within line
    uint32_t active_start;        // PAL=340, NTSC=196 (hsync + back porch + left border)
    uint32_t active_end;          // PAL=980, NTSC=836 (active_start + 640)
    // Left/right border boundaries
    uint32_t border_left_start;   // PAL=186, NTSC=138 (hsync + back porch)
    uint32_t border_right_end;    // PAL=1114, NTSC=890 (active_end + right border)

    // Vertical active lines (first/last display line including borders)
    uint32_t vdisp_first_line;    // First visible line (after top blanking)
    uint32_t vdisp_last_line;     // Last visible line (before bottom blanking), exclusive
    uint32_t canvas_first_line;   // First active graphics line (after top border)
    uint32_t canvas_last_line;    // Last active graphics line (before bottom border), exclusive
} video_timing_t;

typedef struct {
    z80_t cpu;
    uint64_t pins;
    i8255_t ppi;
    i8253_t pit;
    kbd_t kbd;
    sn76489an_t psg;
    z80pio_t pio;
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
    uint8_t gdg_regct53g7; // Cached E008 bit 0 — gates CTC0 in MZ-700 mode
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
    uint16_t    gdg_sof;       // Scroll offset (13-bit: SOF_HI:SOF_LO << 3)
    uint8_t     gdg_sw;        // Scroll width
    uint8_t     gdg_ssa;       // Scroll start address
    uint8_t     gdg_sea;       // Scroll end address
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

    // === GDG Display Engine ===
    // Framebuffer: full beam output (1136×312 PAL or 912×262 NTSC)
    // Each byte = 4-bit IGRB palette index (0-15), 0 = black
    // Host converts to RGBA for display texture.
    #define GDG_FB_MAX_W 1136
    #define GDG_FB_MAX_H 312
    uint8_t  fb[GDG_FB_MAX_W * GDG_FB_MAX_H];
    uint8_t* fb_line;           // Pointer to start of current scanline in fb

    // VRAS arbitration: 8px DISP + 8px CPU = 16px cycle
    // vras_phase toggles every 8 pixel clocks: 0=DISP(GDG owns VRAM), 1=CPU(VRAM available)
    uint8_t  vras_phase;        // 0=DISP cycle, 1=CPU cycle
    uint8_t  vras_sub;          // pixel clock counter within phase (0..7)
    bool     cpu_wait_vram;     // true if CPU is stalled waiting for VRAM access to complete
    uint8_t  vram_read_wait_counter; // pixel-tick countdown for MZ-800 VRAM read access cycle

    // MZ-700 mode VRAM access timing (HBLANK-based arbitration)
    uint8_t  mz700_wr_latch_count;  // writes to VRAM during current visible scanline (0=first free)
    bool     mz700_in_hblank;       // true when beam is in HBLANK zone (CPU can access VRAM freely)

    // GDG internal shift register (fetched during DISP cycle)
    // In 320-mode: 1 fetch = 8 pixels (planes I+II or III+IV, 2bpp)
    // In 640-mode: 1 fetch = 8 pixels (1 plane at a time, 1bpp)
    uint8_t  gdg_shift[4];     // Shift register per plane (8 bits each)
    uint8_t  gdg_shift_cnt;    // bits remaining in current shift group
    uint16_t gdg_vram_col;     // VRAM byte offset for current scanline (GDG fetch pointer)
    uint16_t gdg_vram_row_base;// VRAM base address for current display row
    uint8_t  gdg_row_within_char; // For MZ-700 mode: pixel row within character cell (0-7)

    // CPU→VRAM write buffer (single-byte latch, flushed during next DISP cycle)
    bool     gdg_wr_pending;   // A write is buffered
    uint16_t gdg_wr_addr;      // Buffered VRAM address
    uint8_t  gdg_wr_data;      // Buffered data byte
    bool     gdg_wr_scrw640;   // DMD state at time of write
    bool     gdg_wr_hicolor;   // DMD state at time of write
    int      gdg_wr_odd;       // Address odd flag at time of write

    // MZ-700 mode character rendering state
    uint8_t  mz700_char_code;  // Last fetched character code
    uint8_t  mz700_attr;       // Last fetched attribute byte
    uint8_t  mz700_cg_pattern; // CG-RAM fetched pattern for this row
    uint8_t  mz700_fg_color;   // Current foreground IGRB
    uint8_t  mz700_bg_color;   // Current background IGRB

    // Debug / status
    uint32_t vram_wait_stalls; // count of VRAM wait-state insertions this frame (debug)
    // --- MZ-800 hardware extensions ---
    bool gdg_superimpose;      // Superimpose/CKSW state (set by port 0xCF, high byte 0x07)
    bool forbidden_mode;       // True if GDG is in a forbidden/illegal mode

    // --- Hardware signal emulation (for debugging/analysis) ---
    bool vras;        // VRAM row strobe (VRAS)
    bool vcas;        // VRAM column strobe (VCAS)
    bool voe;         // Video output enable (VOE)
    uint8_t vod;      // VRAM address output (VOD0..7)
    bool vrwr;        // VRAM write strobe (VRWR)
    bool hbln;        // HBLANK signal (HBLN)
    bool cpu_wr;      // CPU write cycle (CPU.WR)
    bool vram_wr;     // Actual VRAM write occurs (VRAM.WR)
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
uint8_t  mz800_gdg_mem_read(mz800_sys_t* sys, uint16_t addr, uint64_t pins);
void     mz800_gdg_mem_write(mz800_sys_t* sys, uint16_t addr, uint8_t data);
void     mz800_gdg_pixel_tick(mz800_sys_t* sys);
void     mz800_gdg_scanline_start(mz800_sys_t* sys);
void     mz800_gdg_tick(mz800_sys_t* sys);
// GDG state init/reset
void     mz800_gdg_init(mz800_sys_t* sys, bool pal);
void     mz800_gdg_reset(mz800_sys_t* sys);

// --- CMT hack (mz800_cmt.c) ---
// Load an MZF file; ROM patches on OUT 0x01/0x02 will use this data.
bool mz800_sys_load_mzf(mz800_sys_t* sys, const uint8_t* data, uint32_t size);

// GDG register write handlers
void mz800_gdg_write_wf(mz800_sys_t* sys, uint8_t data);
void mz800_gdg_write_rf(mz800_sys_t* sys, uint8_t data);
void mz800_gdg_write_dmd(mz800_sys_t* sys, uint8_t data);
void mz800_gdg_write_scroll(mz800_sys_t* sys, uint8_t port_hi, uint8_t data);
void mz800_gdg_write_border(mz800_sys_t* sys, uint8_t data);
void mz800_gdg_write_superimpose(mz800_sys_t* sys, uint8_t data);
void mz800_gdg_write_palette(mz800_sys_t* sys, uint8_t data);
void mz800_gdg_display_pixel(mz800_sys_t* sys);

#ifdef __cplusplus
}
#endif
