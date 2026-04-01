#include "MZ800ChipsImpl.h"
#include <stdbool.h>
#include <string.h>

// High-level VRAM access for CPU (read)
uint8_t mz800_gdg_mem_read(mz800_sys_t* sys, uint16_t addr, uint64_t pins) {
    bool dmd_scrw640 = (sys->gdg_dmd & 0x04) != 0;
    bool dmd_hicolor = (sys->gdg_dmd & 0x02) != 0;
    bool dmd_mz700   = (sys->gdg_dmd & 0x08) != 0;
    uint8_t addr_hi = addr >> 12;
    uint8_t data = 0xFF;
    if ((addr_hi == 0x08 || addr_hi == 0x09) && !dmd_mz700 && sys->vram_on) {
        // 8000-9FFF: MZ-800 VRAM read
        int vras_pos = (sys->vras_phase == 0)
            ? sys->vras_sub : (8 + sys->vras_sub);
        int wait_ticks = 16 - vras_pos;
        if (wait_ticks == 0) wait_ticks = 16;
        if (wait_ticks <= 7) wait_ticks += 8;
        wait_ticks += 16;
        sys->vram_read_wait_counter = (uint8_t)wait_ticks;
        sys->cpu_wait_vram = true;
        sys->vram_wait_stalls++;
        uint16_t vaddr = addr - 0x8000;
        int addr_is_odd = vaddr & 0x01;
        if (dmd_scrw640) vaddr >>= 1;
        vaddr = mz800_hwscroll_addr(sys, vaddr);
        vaddr &= 0x1FFF;
        data = mz800_vram_read(sys, vaddr, addr_is_odd, dmd_scrw640, dmd_hicolor);
    } else if ((addr_hi == 0x0A || addr_hi == 0x0B) && !dmd_mz700 && sys->vram_on && dmd_scrw640) {
        // A000-BFFF: VRAM read only in 640-wide mode (same timing as 8000-9FFF)
        int vras_pos = (sys->vras_phase == 0)
            ? sys->vras_sub : (8 + sys->vras_sub);
        int wait_ticks = 16 - vras_pos;
        if (wait_ticks == 0) wait_ticks = 16;
        if (wait_ticks <= 7) wait_ticks += 8;
        wait_ticks += 16;
        sys->vram_read_wait_counter = (uint8_t)wait_ticks;
        sys->cpu_wait_vram = true;
        sys->vram_wait_stalls++;
        uint16_t vaddr = addr - 0x8000;
        int addr_is_odd = vaddr & 0x01;
        vaddr >>= 1;
        vaddr = mz800_hwscroll_addr(sys, vaddr);
        vaddr &= 0x1FFF;
        data = mz800_vram_read(sys, vaddr, addr_is_odd, dmd_scrw640, dmd_hicolor);
    } else if (addr_hi == 0x0C && dmd_mz700 && sys->vram_on) {
        // C000-CFFF: MZ-700 CG-RAM read → VRAM plane I lower half
        if (!sys->mz700_in_hblank) {
            sys->cpu_wait_vram = true;
            sys->vram_wait_stalls++;
        }
        data = sys->vram[0][addr & 0x0FFF];
    } else if (addr_hi == 0x0D && dmd_mz700 && sys->rom_e000_on) {
        // D000-DFFF: MZ-700 char/attr VRAM read → VRAM plane I upper half
        if (!sys->mz700_in_hblank) {
            sys->cpu_wait_vram = true;
            sys->vram_wait_stalls++;
        }
        data = sys->vram[0][0x1000 | (addr & 0x0FFF)];
    } else {
        // Not VRAM: handled by caller
        data = 0xFF;
    }
    return data;
}

// High-level VRAM access for CPU (write)
void mz800_gdg_mem_write(mz800_sys_t* sys, uint16_t addr, uint8_t data) {
    bool dmd_scrw640 = (sys->gdg_dmd & 0x04) != 0;
    bool dmd_hicolor = (sys->gdg_dmd & 0x02) != 0;
    bool dmd_mz700   = (sys->gdg_dmd & 0x08) != 0;
    uint8_t addr_hi = addr >> 12;
    if ((addr_hi == 0x08 || addr_hi == 0x09) && !dmd_mz700 && sys->vram_on) {
        // 8000-9FFF: MZ-800 VRAM write (buffered during DISP phase)
        uint16_t vaddr = addr - 0x8000;
        int addr_is_odd = vaddr & 0x01;
        if (dmd_scrw640) vaddr >>= 1;
        vaddr = mz800_hwscroll_addr(sys, vaddr);
        vaddr &= 0x1FFF;
        if (sys->vras_phase == 0) {
            sys->gdg_wr_pending = true;
            sys->gdg_wr_addr = vaddr;
            sys->gdg_wr_data = data;
            sys->gdg_wr_odd = addr_is_odd;
            sys->gdg_wr_scrw640 = dmd_scrw640;
            sys->gdg_wr_hicolor = dmd_hicolor;
        } else {
            mz800_vram_write(sys, vaddr, data, addr_is_odd, dmd_scrw640, dmd_hicolor);
            sys->vram_wr = true;
            sys->vram_wait_stalls++;
        }
    } else if ((addr_hi == 0x0A || addr_hi == 0x0B) && !dmd_mz700 && sys->vram_on && dmd_scrw640) {
        // A000-BFFF: VRAM write only in 640-wide mode (buffered during DISP phase)
        uint16_t vaddr = addr - 0x8000;
        int addr_is_odd = vaddr & 0x01;
        vaddr >>= 1;
        vaddr = mz800_hwscroll_addr(sys, vaddr);
        vaddr &= 0x1FFF;
        if (sys->vras_phase == 0) {
            sys->gdg_wr_pending = true;
            sys->gdg_wr_addr = vaddr;
            sys->gdg_wr_data = data;
            sys->gdg_wr_odd = addr_is_odd;
            sys->gdg_wr_scrw640 = dmd_scrw640;
            sys->gdg_wr_hicolor = dmd_hicolor;
        } else {
            mz800_vram_write(sys, vaddr, data, addr_is_odd, dmd_scrw640, dmd_hicolor);
            sys->vram_wr = true;
            sys->vram_wait_stalls++;
        }
    } else if (addr_hi == 0x0C && dmd_mz700 && sys->vram_on) {
        // C000-CFFF: MZ-700 CG-RAM write
        if (!sys->mz700_in_hblank && sys->mz700_wr_latch_count > 0) {
            sys->cpu_wait_vram = true;
            sys->vram_wait_stalls++;
        }
        sys->mz700_wr_latch_count++;
        sys->vram[0][addr & 0x0FFF] = data;
    } else if (addr_hi == 0x0D && dmd_mz700 && sys->rom_e000_on) {
        // D000-DFFF: MZ-700 char/attr VRAM write
        if (!sys->mz700_in_hblank && sys->mz700_wr_latch_count > 0) {
            sys->cpu_wait_vram = true;
            sys->vram_wait_stalls++;
        }
        sys->mz700_wr_latch_count++;
        sys->vram[0][0x1000 | (addr & 0x0FFF)] = data;
    }
}
// GDG state initialization
void mz800_gdg_init(mz800_sys_t* sys, bool pal) {
    sys->gdg_dmd = 0;
    sys->gdg_regct53g7 = 0;
    sys->mz800_switch = true;
    mz800_set_video_standard(sys, pal);
    sys->gdg_wf_plane = 0;
    sys->gdg_wf_mode = 0;
    sys->gdg_wfrf_vbank = 0;
    sys->gdg_rf_plane = 0;
    sys->gdg_rf_search = 0;
    sys->gdg_palgrp = 0;
    sys->gdg_pal[0] = 0x09;
    sys->gdg_pal[1] = 0x0F;
    sys->gdg_pal[2] = 0x09;
    sys->gdg_pal[3] = 0x0F;
    sys->gdg_border = 0;
    sys->gdg_sof = 0;
    sys->gdg_sw = 0;
    sys->gdg_ssa = 0;
    sys->gdg_sea = 0;
    sys->gdg_scroll_on = false;
    sys->line_counter = 0;
    sys->line_cycle = 0;
    sys->pixel_tick = 0;
    sys->pixel_line = 0;
    sys->ctc0_sub = 0;
    sys->psg_sub = 0;
    sys->tempo = 0;
    sys->tempo_divider = 0;
    sys->dbus_latch = 0xFF;
    memset(sys->fb, 0, sizeof(sys->fb));
    sys->fb_line = sys->fb;
    sys->vras_phase = 1;
    sys->vras_sub = 0;
    sys->cpu_wait_vram = false;
    sys->vram_read_wait_counter = 0;
    sys->mz700_wr_latch_count = 0;
    sys->mz700_in_hblank = true;
    sys->vram_wait_stalls = 0;
    memset(sys->gdg_shift, 0, sizeof(sys->gdg_shift));
    sys->gdg_shift_cnt = 0;
    sys->gdg_vram_col = 0;
    sys->gdg_vram_row_base = 0;
    sys->gdg_row_within_char = 0;
    sys->gdg_wr_pending = false;
    sys->gdg_wr_addr = 0;
    sys->gdg_wr_data = 0;
    sys->gdg_wr_odd = 0;
    sys->gdg_wr_scrw640 = false;
    sys->gdg_wr_hicolor = false;
    sys->mz700_char_code = 0;
    sys->mz700_attr = 0;
    sys->mz700_cg_pattern = 0;
    sys->mz700_fg_color = 0;
    sys->mz700_bg_color = 0;
    sys->gdg_superimpose = false;
    sys->forbidden_mode = false;
}

void mz800_gdg_reset(mz800_sys_t* sys) {
    // Preserve PAL/NTSC setting across reset, but refresh timing struct
    mz800_gdg_init(sys, sys->bPAL);
}
// --- GDG register write handlers ---
void mz800_gdg_write_wf(mz800_sys_t* sys, uint8_t data) {
    sys->gdg_wf_plane = data & 0x0F;
    sys->gdg_wfrf_vbank = (data >> 4) & 0x01;
    if (data & 0x80) {
        sys->gdg_wf_mode = (data >> 5) & 0x06; // 0,2,4,6
    } else {
        sys->gdg_wf_mode = (data >> 5) & 0x07; // 0-7
    }
}

void mz800_gdg_write_rf(mz800_sys_t* sys, uint8_t data) {
    sys->gdg_rf_plane = data & 0x0F;
    sys->gdg_wfrf_vbank = (data >> 4) & 0x01;
    sys->gdg_rf_search = (data >> 7) & 0x01;
}

void mz800_gdg_write_dmd(mz800_sys_t* sys, uint8_t data) {
    sys->gdg_dmd = data & 0x0F;
    // DMD→CTC0 gate: MZ-800 mode forces gate HIGH,
    // MZ-700 mode uses cached regct53g7
    if (data & 0x08) {
        i8253_gate(&sys->pit, 0, sys->gdg_regct53g7);
    } else {
        i8253_gate(&sys->pit, 0, 1);
    }
}

void mz800_gdg_write_scroll(mz800_sys_t* sys, uint8_t port_hi, uint8_t data) {
    switch (port_hi) {
        case 1: // SOF_LO
            sys->gdg_sof &= 0x03 << 11;
            sys->gdg_sof |= (data & 0xFF) << 3;
            break;
        case 2: // SOF_HI
            sys->gdg_sof &= 0xFF << 3;
            sys->gdg_sof |= (data & 0x03) << 11;
            break;
        case 3: // SW
            sys->gdg_sw = (data & 0x7F) << 6;
            break;
        case 4: // SSA
            sys->gdg_ssa = (data & 0x7F) << 6;
            break;
        case 5: // SEA
            sys->gdg_sea = (data & 0x7F) << 6;
            break;
    }
    mz800_hwscroll_regs_changed(sys);
}

void mz800_gdg_write_border(mz800_sys_t* sys, uint8_t data) {
    sys->gdg_border = data & 0x0F;
}

void mz800_gdg_write_superimpose(mz800_sys_t* sys, uint8_t data) {
    // PAL/NTSC selection: bit 7 set = NTSC
    mz800_set_video_standard(sys, (data & 0x80) == 0);
    // Superimpose/CKSW: bit 0 = superimpose enable, bit 1 = CKSW
    sys->gdg_superimpose = (data & 0x01) != 0;
    // CKSW could be stored if needed: bool cksw = (data & 0x02) != 0;
}

void mz800_gdg_write_palette(mz800_sys_t* sys, uint8_t data) {
    if (data & 0x40) {
        sys->gdg_palgrp = data & 0x03;
    } else {
        uint8_t pal_value = data & 0x0F;
        uint8_t pal_idx = (data >> 4) & 0x03;
        sys->gdg_pal[pal_idx] = pal_value;
    }
}
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
        vt->hblank_start       = 846;  // after display: 150 + 696
        vt->hblank_end         = 150;  // back porch ends
        vt->display_start_col  = 150;  // 64 sync + 86 back porch

        // --- Updated vertical timing based on Nobomi reference ---
        vt->vblank_resume_line = 18;   // first non-black line (border starts)
        vt->border_top         = 23;   // lines 18–40: 23 lines of top border
        vt->canvas_first_line  = 41;   // first line with image data inside border
        vt->canvas_last_line   = 241;  // exclusive, so 200 lines: 41–240
        vt->border_bottom      = 17;   // lines 241–257: 17 lines of bottom border
        vt->vblank_first_line  = 258;  // lines 258–261: black
        vt->vsync_first_line   = 259;  // last 3 lines: 259–261

        vt->display_height     = 240;  // 23 + 200 + 17 = 240 (matches 41–240)
        // Active graphics boundaries (within 912px line)
        // HSYNC(64) + backporch(74) + left border(58) = 196
        vt->active_start       = 196;
        vt->active_end         = 836;  // 196 + 640
        vt->border_left_start  = 138;  // HSYNC(64) + backporch(74)
        vt->border_right_end   = 890;  // 836 + 54

        // Vertical active region
        vt->vdisp_first_line   = 18;   // first non-black line (border starts)
        vt->vdisp_last_line    = 258;  // first black line (exclusive)
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

// GDG per-pixel timing and hardware signal logic
void mz800_gdg_tick(mz800_sys_t* sys) {
    // --- Hardware signal: HBLN (HBLANK) ---
    if (sys->pixel_line >= sys->vt.hblank_start || sys->pixel_line < sys->vt.hblank_end) {
        sys->hbln = true;
    } else {
        sys->hbln = false;
    }

    // --- VRAM WAIT logic: decrement counter and clear wait flag ---
    if (sys->vram_read_wait_counter > 0) {
        sys->vram_read_wait_counter--;
        if (sys->vram_read_wait_counter == 0) {
            sys->cpu_wait_vram = false;
        }
    }

    // --- Hardware signal: VRAS (VRAM Row Strobe) ---
    sys->vras = (sys->vras_phase == 0);

    // --- Hardware signal: VCAS (VRAM Column Strobe) ---
    sys->vcas = (sys->vras_phase == 0 && sys->vras_sub == 0);

    // --- Hardware signal: VOE (Video Output Enable) ---
    sys->voe =
        (sys->line_counter >= sys->vt.vdisp_first_line &&
         sys->line_counter < sys->vt.vdisp_last_line &&
         !(sys->pixel_line >= sys->vt.hblank_start || sys->pixel_line < sys->vt.hblank_end));

    // --- Hardware signal: VOD (VRAM address output) ---
    if (sys->vras_phase == 0) {
        sys->vod = (uint8_t)(sys->gdg_vram_col & 0xFF);
    } else {
        sys->vod = 0;
    }

    // --- Hardware signal: VRWR (VRAM write strobe) ---
    if (sys->vras_phase == 0 && sys->gdg_wr_pending) {
        sys->vrwr = true;
    } else {
        sys->vrwr = false;
    }

    // --- Hardware signal: CPU.WR (CPU write cycle) ---
    if ((sys->pins & Z80_MREQ) && (sys->pins & Z80_WR)) {
        sys->cpu_wr = true;
    } else {
        sys->cpu_wr = false;
    }

    // --- Hardware signal: VRAM.WR (actual VRAM write occurs) ---
    if (sys->vram_wr) {
        sys->vram_wr = false;
    }
}

int mz800Palette[16][3] = {
  {0x00, 0x00, 0x00}, {0x40, 0x40, 0xAC}, {0xD0, 0x34, 0x00}, {0xB4, 0x0C, 0x8C},
  {0x40, 0x6C, 0x00}, {0x24, 0xCC, 0xFF}, {0xE8, 0xD4, 0x30}, {0xD0, 0xD0, 0xD0},
  {0x84, 0x84, 0x84}, {0x00, 0x8C, 0xE8}, {0xFF, 0x00, 0x00}, {0xF0, 0x54, 0xCC},
  {0x54, 0xFF, 0x54}, {0x80, 0xFF, 0xFF}, {0xFF, 0xFF, 0x28}, {0xFF, 0xFF, 0xFF}
};

// Stub: GDG display engine per-pixel hook
void mz800_gdg_display_pixel(mz800_sys_t* sys) {
    // TODO: Implement pixel rendering logic
    (void)sys;
}

// Stub: GDG display engine scanline start hook
void mz800_gdg_scanline_start(mz800_sys_t* sys) {
    // TODO
}
