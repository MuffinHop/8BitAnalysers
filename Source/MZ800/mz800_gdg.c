// mz800_gdg.c — GDG graphics display generator: video timing, hardware scroll, VRAM read/write
#include "MZ800ChipsImpl.h"

// --- PAL/NTSC video standard ---
// NTPL=0 (PAL):  CLK=17,734,475 Hz, CPU=CLK/5=3,546,895 Hz, 312 lines × 1136 px → 50 Hz
// NTPL=1 (NTSC): CLK=14,318,180 Hz, CPU=CLK/4=3,579,545 Hz, 262 lines × 912 px  → ~60 Hz
// NTPL selects /4 or /5 to keep CPU near 3.55 MHz in both standards.
void mz800_set_video_standard(mz800_sys_t* sys, bool pal) {
    sys->bPAL = pal;
    video_timing_t* vt = &sys->vt;

    if (pal) {
        vt->cpu_divider        = 5;    // NTPL=0: CLK/5 path selected
        vt->psg_pixel_divider  = 80;   // 16 CPU ticks × 5 px
        vt->lines_per_frame    = 312;
        vt->pixels_per_line    = 1136;
        vt->tempo_toggle_lines = 229;  // 312×50÷(2×34) = 229.4
        vt->hsync_width        = 80;
        vt->hblank_start       = 1114; // after display: 186 + 928
        vt->hblank_end         = 186;  // back porch ends
        vt->display_start_col  = 186;  // 80 sync + 106 back porch
        vt->vblank_first_line  = 288;  // 46 top border + 200 canvas + 42 bottom border
        vt->vblank_resume_line = 22;   // 3 vsync + 19 back porch
        vt->vsync_first_line   = 309;  // last 3 lines
        vt->border_top         = 46;
        vt->border_bottom      = 42;
        vt->display_height     = 288;  // 46 + 200 + 42
        // Active graphics boundaries (within 1136px line)
        // HSYNC(80) + backporch(106) + left border(154) = 340
        vt->active_start       = 340;
        vt->active_end         = 980;  // 340 + 640
        vt->border_left_start  = 186;  // HSYNC(80) + backporch(106)
        vt->border_right_end   = 1114; // 980 + 134
        // Vertical active region
        vt->vdisp_first_line   = 22;   // vblank_resume_line
        vt->vdisp_last_line    = 288;  // vblank_first_line
        vt->canvas_first_line  = 68;   // 22 + 46 top border
        vt->canvas_last_line   = 268;  // 68 + 200
    } else {
        vt->cpu_divider        = 4;    // NTPL=1: CLK/4 path selected
        vt->psg_pixel_divider  = 64;   // 16 CPU ticks × 4 px
        vt->lines_per_frame    = 262;
        vt->pixels_per_line    = 912;
        vt->tempo_toggle_lines = 231;  // 262×60÷(2×34) = 230.9
        vt->hsync_width        = 64;
        vt->hblank_start       = 846;  // after display: 150 + 696    ← 138+58+640+54=890, wrong?
        vt->hblank_end         = 150;  // back porch ends             ← wiki: 64+74=138
        vt->display_start_col  = 150;  // 64 sync + 86 back porch
        vt->vblank_first_line  = 242;  // 20 top border + 200 canvas + 22 bottom border
        vt->vblank_resume_line = 18;   // 3 vsync + 15 back porch
        vt->vsync_first_line   = 259;  // last 3 lines
        vt->border_top         = 20;
        vt->border_bottom      = 22;
        vt->display_height     = 242;  // 20 + 200 + 22
        // Active graphics boundaries (within 912px line)
        // HSYNC(64) + backporch(74) + left border(58) = 196
        vt->active_start       = 196;
        vt->active_end         = 836;  // 196 + 640
        vt->border_left_start  = 138;  // HSYNC(64) + backporch(74)
        vt->border_right_end   = 890;  // 836 + 54
        // Vertical active region
        vt->vdisp_first_line   = 18;   // vblank_resume_line
        vt->vdisp_last_line    = 242;  // vblank_first_line
        vt->canvas_first_line  = 38;   // 18 + 20 top border
        vt->canvas_last_line   = 238;  // 38 + 200
    }
}

// --- Hardware scroll address transform ---
uint16_t mz800_hwscroll_addr(mz800_sys_t* sys, uint16_t addr) {
    if (sys->gdg_scroll_on) {
        if (addr >= (uint16_t)sys->gdg_ssa && addr < (uint16_t)sys->gdg_sea) {
            if (addr >= (uint16_t)(sys->gdg_sea - sys->gdg_sof)) {
                return addr + sys->gdg_sof - sys->gdg_sw;
            }
            return addr + sys->gdg_sof;
        }
    }
    return addr;
}

// --- Scroll register validation (called after any scroll reg write) ---
void mz800_hwscroll_regs_changed(mz800_sys_t* sys) {
    if ((sys->gdg_sof > 0) &&
        (sys->gdg_ssa <= 0x1E00) &&
        (sys->gdg_sea >= 0x0140) &&
        (sys->gdg_sw > sys->gdg_sof) &&
        (sys->gdg_sea > sys->gdg_ssa) &&
        (sys->gdg_sw == (sys->gdg_sea - sys->gdg_ssa))) {
        sys->gdg_scroll_on = true;
    } else {
        sys->gdg_scroll_on = false;
    }
}

// --- VRAM read (shared by 8000-9FFF and A000-BFFF paths) ---
uint8_t mz800_vram_read(mz800_sys_t* sys, uint16_t vaddr, int addr_is_odd,
                         bool dmd_scrw640, bool dmd_hicolor)
{
    uint8_t data;
    if (sys->gdg_rf_search) {
        // SEARCH mode: exact color match
        uint8_t search    = 0xFF;
        uint8_t notsearch = 0x00;
        if (dmd_scrw640) {
            int p_even = sys->gdg_wfrf_vbank ? 2 : 0;
            int p_odd  = p_even + 1;
            int p = addr_is_odd ? p_odd : p_even;
            if (sys->gdg_rf_plane & 0x01) {
                search &= sys->vram[p][vaddr];
            } else {
                notsearch |= sys->vram[p][vaddr];
            }
        } else {
            for (int p = 0; p < 4; p++) {
                bool avail = dmd_hicolor || (sys->gdg_wfrf_vbank ? (p >= 2) : (p < 2));
                if (!avail) continue;
                if (sys->gdg_rf_plane & (1 << p)) {
                    search &= sys->vram[p][vaddr];
                } else {
                    notsearch |= sys->vram[p][vaddr];
                }
            }
        }
        data = search & ~notsearch;
    } else {
        // SINGLE mode: AND-fold selected planes
        if (sys->gdg_rf_plane == 0) {
            data = 0xFF;
        } else if (dmd_scrw640) {
            int p_even = sys->gdg_wfrf_vbank ? 2 : 0;
            int p_odd  = p_even + 1;
            int p = addr_is_odd ? p_odd : p_even;
            data = sys->vram[p][vaddr];
        } else {
            data = 0xFF;
            for (int p = 0; p < 4; p++) {
                if (sys->gdg_rf_plane & (1 << p)) {
                    data &= sys->vram[p][vaddr];
                }
            }
        }
    }
    return data;
}

// --- VRAM write (shared by 8000-9FFF and A000-BFFF paths) ---
void mz800_vram_write(mz800_sys_t* sys, uint16_t vaddr, uint8_t data,
                       int addr_is_odd, bool dmd_scrw640, bool dmd_hicolor)
{
    uint8_t avlb = dmd_hicolor ? 0x0F : 0x00;
    avlb |= sys->gdg_wfrf_vbank ? 0x0C : 0x03;
    if (dmd_scrw640) avlb &= 0x05;

    for (int p = 0; p < 4; p++) {
        int  pmask     = 1 << p;
        bool do_write  = false;

        if (dmd_scrw640) {
            bool is_even_plane = (p == 0 || p == 2);
            bool match = is_even_plane ? !addr_is_odd : addr_is_odd;
            if (!match) continue;
            int ctrl_bit = (p < 2) ? 0x01 : 0x04;
            if (!(sys->gdg_wf_mode & 4)) {
                if ((sys->gdg_wf_plane & ctrl_bit) && (avlb & (is_even_plane ? ctrl_bit : (ctrl_bit << 1))))
                    do_write = true;
            } else {
                if (avlb & pmask) {
                    do_write = (sys->gdg_wf_plane & ctrl_bit) ||
                               (sys->gdg_wf_mode == 4 || sys->gdg_wf_mode == 6);
                }
            }
        } else {
            if (!(sys->gdg_wf_mode & 4)) {
                if ((sys->gdg_wf_plane & pmask) && (avlb & pmask))
                    do_write = true;
            } else {
                if (avlb & pmask) {
                    do_write = (sys->gdg_wf_plane & pmask) ||
                               (sys->gdg_wf_mode == 4 || sys->gdg_wf_mode == 6);
                }
            }
        }

        if (!do_write) continue;

        bool plane_selected;
        if (dmd_scrw640) {
            int ctrl_bit = (p < 2) ? 0x01 : 0x04;
            plane_selected = (sys->gdg_wf_plane & ctrl_bit) != 0;
        } else {
            plane_selected = (sys->gdg_wf_plane & pmask) != 0;
        }

        switch (sys->gdg_wf_mode) {
            case 0: sys->vram[p][vaddr]  = data; break;
            case 1: sys->vram[p][vaddr] ^= data; break;
            case 2: sys->vram[p][vaddr] |= data; break;
            case 3: sys->vram[p][vaddr] &= ~data; break;
            case 4: sys->vram[p][vaddr]  = plane_selected ? data : 0x00; break;
            case 6:
                if (plane_selected) {
                    sys->vram[p][vaddr] |= data;
                } else {
                    sys->vram[p][vaddr] &= ~data;
                }
                break;
            default: sys->vram[p][vaddr] = data; break;
        }
    }
}

// ============================================================================
// GDG Display Engine — pixel-accurate rendering with VRAS arbitration
// ============================================================================

// MZ-700 color map: 3-bit attribute color → 4-bit IGRB
// MZ-700 colors are all bright (intensity bit set), matching mz800emu
static const uint8_t MZ700_COLORMAP[8] = {
    0x00, // 0: black
    0x09, // 1: blue    (I+B)
    0x0A, // 2: red     (I+R)
    0x0B, // 3: magenta (I+R+B)
    0x0C, // 4: green   (I+G)
    0x0D, // 5: cyan    (I+G+B)
    0x0E, // 6: yellow  (I+G+R)
    0x0F, // 7: white   (I+G+R+B)
};

// Called at the start of each new scanline (pixel_line wrapped to 0)
void mz800_gdg_scanline_start(mz800_sys_t* sys)
{
    uint32_t line = sys->line_counter;

    // Set fb_line pointer for this scanline
    if (line < GDG_FB_MAX_H) {
        sys->fb_line = &sys->fb[line * GDG_FB_MAX_W];
    } else {
        sys->fb_line = 0; // shouldn't happen, but safety
    }

    // Reset per-line GDG state
    sys->gdg_vram_col = 0;
    sys->gdg_shift_cnt = 0;

    // Compute VRAM row base address for display fetch
    bool dmd_mz700 = (sys->gdg_dmd & 0x08) != 0;
    bool in_canvas = (line >= sys->vt.canvas_first_line && line < sys->vt.canvas_last_line);

    if (in_canvas) {
        uint32_t canvas_row = line - sys->vt.canvas_first_line;
        if (dmd_mz700) {
            // MZ-700 mode: 40 chars per row, 8 pixel rows per character
            uint32_t char_row = canvas_row / 8;
            sys->gdg_vram_row_base = char_row * 40;
            sys->gdg_row_within_char = canvas_row & 7;
        } else {
            // MZ-800 mode: all modes use 40 bytes/line per plane pair.
            // 640 mode gets double width by interleaving even/odd planes,
            // not by doubling the address range.
            sys->gdg_vram_row_base = canvas_row * 40;
        }
    } else {
        sys->gdg_vram_row_base = 0;
    }
}

// Flush the CPU→VRAM write buffer. Called during DISP cycle.
static void gdg_flush_write_buffer(mz800_sys_t* sys)
{
    if (sys->gdg_wr_pending) {
        mz800_vram_write(sys, sys->gdg_wr_addr, sys->gdg_wr_data,
                         sys->gdg_wr_odd, sys->gdg_wr_scrw640, sys->gdg_wr_hicolor);
        sys->gdg_wr_pending = false;
    }
}

// Fetch the next display byte(s) from VRAM into shift registers.
// Called once per 16-pixel-clock DISP cycle during active graphics area.
static void gdg_fetch_display_byte(mz800_sys_t* sys)
{
    bool dmd_mz700   = (sys->gdg_dmd & 0x08) != 0;
    bool dmd_scrw640 = (sys->gdg_dmd & 0x04) != 0;
    uint16_t col = sys->gdg_vram_col;

    if (dmd_mz700) {
        // MZ-700 mode: fetch character + attribute + CG pattern
        // Layout within VRAM plane I (matching mz800emu):
        //   0x0000-0x07FF: CG-RAM (character generator patterns, loaded from ROM at boot)
        //   0x1000-0x17FF: Character codes (40×25 = 1000 bytes)
        //   0x1800-0x1FFF: Attributes (40×25 = 1000 bytes)
        uint16_t char_offset = sys->gdg_vram_row_base + col;
        if (char_offset < 0x0800) {
            sys->mz700_char_code = sys->vram[0][0x1000 + char_offset];
            sys->mz700_attr = sys->vram[0][0x1800 + char_offset];
        } else {
            sys->mz700_char_code = 0;
            sys->mz700_attr = 0x70; // white on black default
        }

        // Decode attribute: bits 6-4 = FG color (3-bit), bits 2-0 = BG color (3-bit)
        sys->mz700_fg_color = MZ700_COLORMAP[(sys->mz700_attr >> 4) & 0x07];
        sys->mz700_bg_color = MZ700_COLORMAP[sys->mz700_attr & 0x07];

        // CG-RAM lookup: bit 7 of attribute selects upper CG bank (0x0800 offset)
        // CG patterns are in VRAM plane I at 0x0000, NOT in ROM
        uint16_t cg_bank = ((sys->mz700_attr & 0x80) ? 0x0800 : 0x0000);
        uint16_t cg_addr = cg_bank | (sys->mz700_char_code * 8) | sys->gdg_row_within_char;
        if (cg_addr < 0x1000) {
            sys->mz700_cg_pattern = sys->vram[0][cg_addr];
        } else {
            sys->mz700_cg_pattern = 0;
        }
        sys->gdg_shift_cnt = 8; // 8 pixels per character
    } else if (dmd_scrw640) {
        // 640 mode: fetch from same VRAM address across planes
        // Even 8 pixels from plane I (or III), odd 8 pixels from plane II (or IV)
        // In 4-color 640 mode (SCRW640+HICOLOR): even from I+III, odd from II+IV
        uint16_t vaddr_base = sys->gdg_vram_row_base + col;
        uint16_t vaddr = mz800_hwscroll_addr(sys, vaddr_base);
        vaddr &= 0x1FFF;
        bool dmd_hicolor = (sys->gdg_dmd & 0x02) != 0;
        bool dmd_vbank = (sys->gdg_dmd & 0x01) != 0;
        if (dmd_hicolor) {
            if (dmd_vbank) {
                // Undocumented mode 0x07: only planes III+IV
                sys->gdg_shift[0] = 0;
                sys->gdg_shift[1] = 0;
                sys->gdg_shift[2] = sys->vram[2][vaddr]; // plane III
                sys->gdg_shift[3] = sys->vram[3][vaddr]; // plane IV
            } else {
                // Mode 0x06: all 4 planes, even bytes I+III, odd bytes II+IV
                sys->gdg_shift[0] = sys->vram[0][vaddr]; // plane I (even LSB)
                sys->gdg_shift[1] = sys->vram[1][vaddr]; // plane II (odd LSB)
                sys->gdg_shift[2] = sys->vram[2][vaddr]; // plane III (even MSB)
                sys->gdg_shift[3] = sys->vram[3][vaddr]; // plane IV (odd MSB)
            }
        } else {
            // 640×200 2-color: 2 planes selected by DMD bit 0
            int p_even = dmd_vbank ? 2 : 0;
            int p_odd  = p_even + 1;
            sys->gdg_shift[0] = sys->vram[p_even][vaddr]; // even 8 pixels
            sys->gdg_shift[1] = sys->vram[p_odd][vaddr];  // odd 8 pixels
            sys->gdg_shift[2] = 0;
            sys->gdg_shift[3] = 0;
        }
        sys->gdg_shift_cnt = 16; // 16 pixels total
    } else {
        // 320 mode: fetch 1 byte from each active plane (2 or 4 planes)
        uint16_t vaddr_base = sys->gdg_vram_row_base + col;
        uint16_t vaddr = mz800_hwscroll_addr(sys, vaddr_base);
        vaddr &= 0x1FFF;
        bool dmd_hicolor = (sys->gdg_dmd & 0x02) != 0;
        if (dmd_hicolor) {
            // 16-color (modes 0x02/0x03): all 4 planes
            sys->gdg_shift[0] = sys->vram[0][vaddr];
            sys->gdg_shift[1] = sys->vram[1][vaddr];
            sys->gdg_shift[2] = sys->vram[2][vaddr];
            sys->gdg_shift[3] = sys->vram[3][vaddr];
        } else {
            // 4-color: 2 planes selected by DMD bit 0
            int p0 = (sys->gdg_dmd & 0x01) ? 2 : 0;
            int p1 = p0 + 1;
            sys->gdg_shift[0] = sys->vram[p0][vaddr];
            sys->gdg_shift[1] = sys->vram[p1][vaddr];
            sys->gdg_shift[2] = 0;
            sys->gdg_shift[3] = 0;
        }
        sys->gdg_shift_cnt = 8; // 8 pixels per byte in 320-wide mode
    }
    sys->gdg_vram_col++;
}

// Compute pixel color from shift registers without advancing.
// The caller is responsible for shifting at the right rate.
static uint8_t gdg_current_pixel(mz800_sys_t* sys)
{
    bool dmd_mz700   = (sys->gdg_dmd & 0x08) != 0;
    bool dmd_scrw640 = (sys->gdg_dmd & 0x04) != 0;

    if (dmd_mz700) {
        // MZ-700: 1bpp from CG pattern, LSB first (bit 0 = leftmost pixel)
        uint8_t bit = sys->mz700_cg_pattern & 1;
        return bit ? sys->mz700_fg_color : sys->mz700_bg_color;
    } else if (dmd_scrw640) {
        // 640 mode: even 8 pixels from shift[0], odd 8 from shift[1]
        // gdg_shift_cnt counts down from 16: 16..9 = even (shift[0]), 8..1 = odd (shift[1])
        bool dmd_hicolor = (sys->gdg_dmd & 0x02) != 0;
        if (dmd_hicolor) {
            bool dmd_vbank = (sys->gdg_dmd & 0x01) != 0;
            uint8_t plt_idx;
            if (dmd_vbank) {
                // Undocumented mode 0x07: both palette bits from same plane
                if (sys->gdg_shift_cnt > 8) {
                    // Even byte: plane III provides both bits
                    plt_idx = (sys->gdg_shift[2] & 1) | ((sys->gdg_shift[2] & 1) << 1);
                } else {
                    // Odd byte: plane IV provides both bits
                    plt_idx = (sys->gdg_shift[3] & 1) | ((sys->gdg_shift[3] & 1) << 1);
                }
            } else {
                // Mode 0x06: even pixels from I+III, odd from II+IV
                if (sys->gdg_shift_cnt > 8) {
                    plt_idx = (sys->gdg_shift[0] & 1) | ((sys->gdg_shift[2] & 1) << 1);
                } else {
                    plt_idx = (sys->gdg_shift[1] & 1) | ((sys->gdg_shift[3] & 1) << 1);
                }
            }
            return sys->gdg_pal[plt_idx];
        } else {
            // 640×200 2-color: LSB first from even or odd plane
            uint8_t byte_idx = (sys->gdg_shift_cnt > 8) ? 0 : 1;
            uint8_t bit = sys->gdg_shift[byte_idx] & 1;
            return bit ? sys->gdg_pal[1] : sys->gdg_pal[0];
        }
    } else {
        // 320 mode: multi-plane bitfield, LSB first (bit 0 = leftmost pixel)
        bool dmd_hicolor = (sys->gdg_dmd & 0x02) != 0;
        uint8_t plt_idx;
        if (dmd_hicolor) {
            bool dmd_vbank = (sys->gdg_dmd & 0x01) != 0;
            if (dmd_vbank) {
                // Undocumented mode 0x03: PALGRP-dependent plane swizzle
                uint8_t palgrp_test = ((sys->gdg_shift[3] & 1) << 1) | (sys->gdg_shift[2] & 1);
                if (sys->gdg_palgrp == palgrp_test) {
                    // PALGRP match: all 4 bits from planes III+IV only
                    plt_idx = (sys->gdg_shift[2] & 1)       |  // plane III → bit 0
                              ((sys->gdg_shift[3] & 1) << 1) |  // plane IV  → bit 1
                              ((sys->gdg_shift[2] & 1) << 2) |  // plane III → bit 2
                              ((sys->gdg_shift[3] & 1) << 3);   // plane IV  → bit 3
                } else {
                    // No PALGRP match: normal 4-plane index
                    plt_idx = (sys->gdg_shift[0] & 1)       |  // plane I   → bit 0
                              ((sys->gdg_shift[1] & 1) << 1) |  // plane II  → bit 1
                              ((sys->gdg_shift[2] & 1) << 2) |  // plane III → bit 2
                              ((sys->gdg_shift[3] & 1) << 3);   // plane IV  → bit 3
                }
            } else {
                // Mode 0x02: standard 16-color
                plt_idx = (sys->gdg_shift[0] & 1)       |  // plane I  → bit 0
                          ((sys->gdg_shift[1] & 1) << 1) |  // plane II → bit 1
                          ((sys->gdg_shift[2] & 1) << 2) |  // plane III→ bit 2
                          ((sys->gdg_shift[3] & 1) << 3);   // plane IV → bit 3
            }
            // Palette group check: if PALGRP matches upper 2 bits, use palette
            if (sys->gdg_palgrp == ((plt_idx >> 2) & 0x03)) {
                return sys->gdg_pal[plt_idx & 0x03];
            }
            return plt_idx; // Direct 4-bit IGRB
        } else {
            // 4-color: 2 planes, LSB first
            plt_idx = (sys->gdg_shift[0] & 1) | ((sys->gdg_shift[1] & 1) << 1);
            return sys->gdg_pal[plt_idx];
        }
    }
}

// Advance the shift registers by one pixel position
static void gdg_shift_advance(mz800_sys_t* sys)
{
    bool dmd_mz700   = (sys->gdg_dmd & 0x08) != 0;
    bool dmd_scrw640 = (sys->gdg_dmd & 0x04) != 0;

    if (dmd_mz700) {
        sys->mz700_cg_pattern >>= 1; // LSB first — shift right
    } else if (dmd_scrw640) {
        bool dmd_hicolor = (sys->gdg_dmd & 0x02) != 0;
        // Shift the active byte(s) — LSB first, shift right
        if (sys->gdg_shift_cnt > 8) {
            sys->gdg_shift[0] >>= 1;
            if (dmd_hicolor) sys->gdg_shift[2] >>= 1; // plane III for even
        } else {
            sys->gdg_shift[1] >>= 1;
            if (dmd_hicolor) sys->gdg_shift[3] >>= 1; // plane IV for odd
        }
    } else {
        // 320 mode: shift all active planes — LSB first, shift right
        bool dmd_hicolor = (sys->gdg_dmd & 0x02) != 0;
        sys->gdg_shift[0] >>= 1;
        sys->gdg_shift[1] >>= 1;
        if (dmd_hicolor) {
            sys->gdg_shift[2] >>= 1;
            sys->gdg_shift[3] >>= 1;
        }
    }
    sys->gdg_shift_cnt--;
}

// Called once per pixel clock from the main timing loop.
// Handles VRAS arbitration, display fetch, pixel output, and write buffer flush.
void mz800_gdg_pixel_tick(mz800_sys_t* sys)
{
    uint32_t px = sys->pixel_line;
    uint32_t line = sys->line_counter;
    const video_timing_t* vt = &sys->vt;

    // === VRAS phase arbitration (MZ-800 mode: 8px DISP + 8px CPU) ===
    sys->vras_sub++;
    if (sys->vras_sub >= 8) {
        sys->vras_sub = 0;
        sys->vras_phase ^= 1; // Toggle DISP ↔ CPU

        if (sys->vras_phase == 0) {
            // Entering DISP cycle: flush pending CPU write buffer
            gdg_flush_write_buffer(sys);
        }
        // Note: cpu_wait_vram is now managed by vram_read_wait_counter, not phase toggle
    }

    // === MZ-800 VRAM read access cycle countdown ===
    if (sys->vram_read_wait_counter > 0) {
        sys->vram_read_wait_counter--;
        if (sys->vram_read_wait_counter == 0) {
            sys->cpu_wait_vram = false;
        }
    }

    // === MZ-700 HBLANK event: release VRAM wait & reset write latch ===
    // HBLANK zone starts 3 pixels before canvas end (matching reference: HBLN_FIRST = CANVAS_LAST - 3)
    // In MZ-700 mode, the CPU can freely access VRAM during HBLANK/VBLANK/border
    {
        bool beam_in_hblank_zone = (px >= (vt->active_end - 3)) || (px < vt->active_start);
        bool beam_in_vblank = (line >= vt->vblank_first_line || line < vt->vblank_resume_line);
        bool beam_in_canvas_v = (line >= vt->canvas_first_line && line < vt->canvas_last_line);
        bool new_hblank = beam_in_hblank_zone || beam_in_vblank || !beam_in_canvas_v;

        if (new_hblank && !sys->mz700_in_hblank) {
            // Rising edge: entering HBLANK — release MZ-700 VRAM wait, reset write latch
            sys->mz700_wr_latch_count = 0;
            // If CPU was waiting for HBLANK (MZ-700 mode), release it now
            if ((sys->gdg_dmd & 0x08) && sys->cpu_wait_vram && sys->vram_read_wait_counter == 0) {
                sys->cpu_wait_vram = false;
            }
        }
        sys->mz700_in_hblank = new_hblank;
    }

    // === Determine what to output ===
    bool in_vblank = (line >= vt->vblank_first_line || line < vt->vblank_resume_line);
    bool in_hblank = (px >= vt->hblank_start || px < vt->hblank_end);
    bool in_canvas_v = (line >= vt->canvas_first_line && line < vt->canvas_last_line);
    bool in_active_h = (px >= vt->active_start && px < vt->active_end);
    bool in_border_h = !in_hblank && !in_active_h &&
                       (px >= vt->border_left_start && px < vt->border_right_end);
    bool in_border_v = !in_vblank && !in_canvas_v;

    uint8_t pixel_color = 0x00; // black (HSYNC/HBLANK/VBLANK)

    if (!in_vblank && !in_hblank) {
        if (in_canvas_v && in_active_h) {
            // === Active graphics area ===
            // Fetch new display byte(s) when shift register is exhausted
            // Fetches happen during DISP phase at the start of a VRAS cycle
            if (sys->gdg_shift_cnt == 0 && sys->vras_phase == 0 && sys->vras_sub == 0) {
                gdg_fetch_display_byte(sys);
            }

            if (sys->gdg_shift_cnt > 0) {
                pixel_color = gdg_current_pixel(sys);

                bool dmd_scrw640 = (sys->gdg_dmd & 0x04) != 0;
                bool dmd_mz700   = (sys->gdg_dmd & 0x08) != 0;
                if (dmd_scrw640) {
                    // 640 mode: 1 pixel per pixel clock. Always advance.
                    gdg_shift_advance(sys);
                } else if (dmd_mz700) {
                    // MZ-700: 8px chars, 16px wide per char (doubled).
                    // Advance MZ-700 every other pixel clock.
                    uint32_t active_px = px - vt->active_start;
                    if (active_px & 1) {
                        gdg_shift_advance(sys);
                    }
                } else {
                    // 320 mode: 1 logical pixel = 2 pixel clocks.
                    // Advance shift register every other pixel clock.
                    uint32_t active_px = px - vt->active_start;
                    if (active_px & 1) {
                        gdg_shift_advance(sys);
                    }
                }
            }
        } else {
            // Border area
            pixel_color = sys->gdg_border;
        }
    }

    // Write to framebuffer
    if (sys->fb_line && px < GDG_FB_MAX_W) {
        sys->fb_line[px] = pixel_color;
    }
}
