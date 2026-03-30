#define CHIPS_IMPL
#include "MZ800ChipsImpl.h"

#include <string.h>

#define CHIPS_UTIL_IMPL
#include "util/z80dasm.h"
#include "util/m6502dasm.h"

mz800_sys_t g_mz800_sys;

// --- PAL/NTSC video standard ---
// NTPL=0 (PAL):  CLK=17,734,475 Hz, CPU=CLK/5=3,546,895 Hz, 312 lines × 1136 px → 50 Hz
// NTPL=1 (NTSC): CLK=14,318,180 Hz, CPU=CLK/4=3,579,545 Hz, 262 lines × 912 px  → ~60 Hz
// NTPL selects /4 or /5 to keep CPU near 3.55 MHz in both standards.
// CTC0 = 16 px clocks tick (CLK/16). PSG = 16 CPU ticks (80 px PAL, 64 px NTSC).
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
        vt->hblank_start       = 1114;   // after display: 186 + 928
        vt->hblank_end         = 186;    // back porch ends
        vt->display_start_col  = 186;    // 80 sync + 106 back porch
        vt->vblank_first_line  = 288;    // 46 top border + 200 canvas + 42 bottom border
        vt->vblank_resume_line = 22;     // 3 vsync + 19 back porch
        vt->vsync_first_line   = 309;    // last 3 lines
        vt->border_top         = 46;
        vt->border_bottom      = 42;
        vt->display_height     = 288;    // 46 + 200 + 42
    } else {
        vt->cpu_divider        = 4;    // NTPL=1: CLK/4 path selected
        vt->psg_pixel_divider  = 64;   // 16 CPU ticks × 4 px
        vt->lines_per_frame    = 262;
        vt->pixels_per_line    = 912;
        vt->tempo_toggle_lines = 231;  // 262×60÷(2×34) = 230.9
        vt->hsync_width        = 64;
        vt->hblank_start       = 846;    // after display: 150 + 696, scaled proportionally
        vt->hblank_end         = 150;    // back porch ends
        vt->display_start_col  = 150;    // 64 sync + 86 back porch
        vt->vblank_first_line  = 242;    // 20 top border + 200 canvas + 22 bottom border
        vt->vblank_resume_line = 18;     // 3 vsync + 15 back porch
        vt->vsync_first_line   = 259;    // last 3 lines
        vt->border_top         = 20;
        vt->border_bottom      = 22;
        vt->display_height     = 242;    // 20 + 200 + 22
    }
}

void mz800_sys_init(mz800_sys_t* sys)
{
    sys->pins = z80_init(&sys->cpu);
    sys->tick_count = 0;
    
    i8253_init(&sys->pit);
    i8255_init(&sys->ppi);
    kbd_init(&sys->kbd, 1);
    kbd_register_key(&sys->kbd, 130, 0, 7, 0);
    kbd_register_key(&sys->kbd, 131, 0, 6, 0);
    kbd_register_key(&sys->kbd, 132, 0, 5, 0);
    kbd_register_key(&sys->kbd, 133, 0, 4, 0);
    kbd_register_key(&sys->kbd, 9, 0, 3, 0);
    kbd_register_key(&sys->kbd, 59, 0, 2, 0);
    kbd_register_key(&sys->kbd, 58, 0, 1, 0);
    kbd_register_key(&sys->kbd, 13, 0, 0, 0);
    kbd_register_key(&sys->kbd, 89, 1, 7, 0);
    kbd_register_key(&sys->kbd, 90, 1, 6, 0);
    kbd_register_key(&sys->kbd, 64, 1, 5, 0);
    kbd_register_key(&sys->kbd, 91, 1, 4, 0);
    kbd_register_key(&sys->kbd, 93, 1, 3, 0);
    kbd_register_key(&sys->kbd, 81, 2, 7, 0);
    kbd_register_key(&sys->kbd, 82, 2, 6, 0);
    kbd_register_key(&sys->kbd, 83, 2, 5, 0);
    kbd_register_key(&sys->kbd, 84, 2, 4, 0);
    kbd_register_key(&sys->kbd, 85, 2, 3, 0);
    kbd_register_key(&sys->kbd, 86, 2, 2, 0);
    kbd_register_key(&sys->kbd, 87, 2, 1, 0);
    kbd_register_key(&sys->kbd, 88, 2, 0, 0);
    kbd_register_key(&sys->kbd, 73, 3, 7, 0);
    kbd_register_key(&sys->kbd, 74, 3, 6, 0);
    kbd_register_key(&sys->kbd, 75, 3, 5, 0);
    kbd_register_key(&sys->kbd, 76, 3, 4, 0);
    kbd_register_key(&sys->kbd, 77, 3, 3, 0);
    kbd_register_key(&sys->kbd, 78, 3, 2, 0);
    kbd_register_key(&sys->kbd, 79, 3, 1, 0);
    kbd_register_key(&sys->kbd, 80, 3, 0, 0);
    kbd_register_key(&sys->kbd, 65, 4, 7, 0);
    kbd_register_key(&sys->kbd, 66, 4, 6, 0);
    kbd_register_key(&sys->kbd, 67, 4, 5, 0);
    kbd_register_key(&sys->kbd, 68, 4, 4, 0);
    kbd_register_key(&sys->kbd, 69, 4, 3, 0);
    kbd_register_key(&sys->kbd, 70, 4, 2, 0);
    kbd_register_key(&sys->kbd, 71, 4, 1, 0);
    kbd_register_key(&sys->kbd, 72, 4, 0, 0);
    kbd_register_key(&sys->kbd, 49, 5, 7, 0);
    kbd_register_key(&sys->kbd, 50, 5, 6, 0);
    kbd_register_key(&sys->kbd, 51, 5, 5, 0);
    kbd_register_key(&sys->kbd, 52, 5, 4, 0);
    kbd_register_key(&sys->kbd, 53, 5, 3, 0);
    kbd_register_key(&sys->kbd, 54, 5, 2, 0);
    kbd_register_key(&sys->kbd, 55, 5, 1, 0);
    kbd_register_key(&sys->kbd, 56, 5, 0, 0);
    kbd_register_key(&sys->kbd, 92, 6, 7, 0);
    kbd_register_key(&sys->kbd, 126, 6, 6, 0);
    kbd_register_key(&sys->kbd, 45, 6, 5, 0);
    kbd_register_key(&sys->kbd, 32, 6, 4, 0);
    kbd_register_key(&sys->kbd, 48, 6, 3, 0);
    kbd_register_key(&sys->kbd, 57, 6, 2, 0);
    kbd_register_key(&sys->kbd, 44, 6, 1, 0);
    kbd_register_key(&sys->kbd, 46, 6, 0, 0);
    kbd_register_key(&sys->kbd, 134, 7, 7, 0);
    kbd_register_key(&sys->kbd, 135, 7, 6, 0);
    kbd_register_key(&sys->kbd, 136, 7, 5, 0);
    kbd_register_key(&sys->kbd, 137, 7, 4, 0);
    kbd_register_key(&sys->kbd, 138, 7, 3, 0);
    kbd_register_key(&sys->kbd, 139, 7, 2, 0);
    kbd_register_key(&sys->kbd, 63, 7, 1, 0);
    kbd_register_key(&sys->kbd, 47, 7, 0, 0);
    kbd_register_key(&sys->kbd, 27, 8, 7, 0);
    kbd_register_modifier(&sys->kbd, 1, 8, 6);
    kbd_register_modifier(&sys->kbd, 0, 8, 0);
    kbd_register_key(&sys->kbd, 142, 9, 7, 0);
    kbd_register_key(&sys->kbd, 143, 9, 6, 0);
    kbd_register_key(&sys->kbd, 144, 9, 5, 0);
    kbd_register_key(&sys->kbd, 145, 9, 4, 0);
    kbd_register_key(&sys->kbd, 146, 9, 3, 0);
    
    // Initial memory state — matches mz800emu memory_reset():
    // ROM_0000, ROM_1000, ROM_E000 on; VRAM off (monitor ROM enables via OUT E4)
    sys->rom_0000_on = true;
    sys->rom_1000_on = true;
    sys->rom_e000_on = true;
    sys->vram_on = false;

    // GDG defaults
    sys->gdg_dmd = 0;
    sys->mz800_switch = true; // Hardware DIP: MZ-800 mode
    mz800_set_video_standard(sys, true); // Default: PAL
    sys->gdg_wf_plane = 0;
    sys->gdg_wf_mode = 0;
    sys->gdg_wfrf_vbank = 0;
    sys->gdg_rf_plane = 0;
    sys->gdg_rf_search = 0;
    sys->gdg_palgrp = 0;
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

    // PSG init — all channels off
    memset(&sys->psg, 0, sizeof(sys->psg));
    for (int i = 0; i < PSG_CHANNELS; i++) {
        sys->psg.ch[i].attn = 15; // OFF
        sys->psg.ch[i].output = 1;
    }
    sys->psg.ch[3].type = 1; // noise
    sys->psg.ch[3].shift_reg = 1 << 15;

    // CMT hack
    memset(&sys->cmt, 0, sizeof(sys->cmt));

    // Joystick — all released (active low = 0xFF)
    sys->joy[0].state = 0xFF;
    sys->joy[1].state = 0xFF;
}

// --- Hardware scroll address transform ---
static inline uint16_t hwscroll_shift_addr(mz800_sys_t* sys, uint16_t addr) {
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
static void hwscroll_regs_changed(mz800_sys_t* sys) {
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

// --- VRAM read helper (shared by 8000-9FFF and A000-BFFF paths) ---
static inline uint8_t mz800_vram_read(mz800_sys_t* sys, uint16_t vaddr, int addr_is_odd,
                                       bool dmd_scrw640, bool dmd_hicolor)
{
    uint8_t data;
    if (sys->gdg_rf_search) {
        // SEARCH mode: exact color match
        uint8_t search = 0xFF;
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

// --- VRAM write helper (shared by 8000-9FFF and A000-BFFF paths) ---
static inline void mz800_vram_write(mz800_sys_t* sys, uint16_t vaddr, uint8_t data,
                                     int addr_is_odd, bool dmd_scrw640, bool dmd_hicolor)
{
    uint8_t avlb = dmd_hicolor ? 0x0F : 0x00;
    avlb |= sys->gdg_wfrf_vbank ? 0x0C : 0x03;
    if (dmd_scrw640) avlb &= 0x05;

    for (int p = 0; p < 4; p++) {
        int pmask = 1 << p;
        bool do_write = false;

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
            case 0: sys->vram[p][vaddr] = data; break;
            case 1: sys->vram[p][vaddr] ^= data; break;
            case 2: sys->vram[p][vaddr] |= data; break;
            case 3: sys->vram[p][vaddr] &= ~data; break;
            case 4: sys->vram[p][vaddr] = plane_selected ? data : 0x00; break;
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

// --- PSG write byte (SN76489AN register protocol) ---
static void psg_write_byte_internal(psg_t* psg, uint8_t value) {
    int latch = value & 0x80;
    int cs, attn;

    if (latch) {
        cs = (value >> 5) & 0x03;
        attn = (value >> 4) & 0x01;
        psg->latch_cs = cs;
        psg->latch_attn = attn;
    } else {
        cs = psg->latch_cs;
        attn = psg->latch_attn;
    }

    psg_channel_t* ch = &psg->ch[cs];

    if (attn) {
        ch->attn = value & 0x0F;
    } else if (latch && ch->type == 0) {
        // Tone latch: store low nibble
        ch->latch_div = value & 0x0F;
    } else {
        if (ch->type == 0) {
            // Tone data: combine high bits with latched low nibble
            ch->divider = ((uint16_t)(value & 0x3F) << 4) | ch->latch_div;
        } else {
            // Noise: set div type and noise type
            ch->noise_div_type = value & 0x03;
            ch->noise_type = (value >> 2) & 0x01;
        }
    }
}

// --- PSG step: advance all channels by one PSG clock ---
void psg_step(psg_t* psg) {
    for (int cs = 0; cs < PSG_CHANNELS; cs++) {
        psg_channel_t* ch = &psg->ch[cs];

        if (ch->attn == 15) continue; // OFF

        if (ch->type == 0) {
            // Tone channel
            if (ch->divider < 2) {
                ch->output = 1;
            } else if (ch->timer == 0) {
                ch->timer = ch->divider - 1;
                ch->output ^= 1;
            } else {
                ch->timer--;
            }
        } else {
            // Noise channel
            if (ch->noise_div_type == 3 && psg->ch[2].divider < 2) {
                ch->output = 1;
            } else if (ch->timer == 0) {
                if (ch->noise_div_type == 3) {
                    ch->timer = psg->ch[2].divider - 1;
                } else {
                    ch->timer = (0x10 << ch->noise_div_type) - 1;
                }

                uint8_t bit0 = ch->shift_reg & 0x01;

                if (ch->noise_type == 1) {
                    // White noise: XOR bit 0 and bit 3
                    uint8_t bit3 = (ch->shift_reg >> 3) & 0x01;
                    ch->shift_reg = (ch->shift_reg >> 1) | ((uint16_t)(bit0 ^ bit3) << 15);
                } else {
                    // Periodic noise
                    ch->shift_reg = (ch->shift_reg >> 1) | ((uint16_t)bit0 << 15);
                }
                ch->output = ch->shift_reg & 0x01;
            } else {
                ch->timer--;
            }
        }
    }
}

void mz800_sys_tick(mz800_sys_t* sys)
{
    uint64_t pins = z80_tick(&sys->cpu, sys->pins);
    bool dmd_scrw640 = (sys->gdg_dmd & 0x04) != 0;
    bool dmd_hicolor = (sys->gdg_dmd & 0x02) != 0;
    bool dmd_mz700   = (sys->gdg_dmd & 0x08) != 0;

    if (pins & Z80_MREQ) {
        const uint16_t addr = Z80_GET_ADDR(pins);
        if (pins & Z80_RD) {
            uint8_t data = 0xFF;
            uint8_t addr_hi = addr >> 12;
            
            if (addr_hi == 0x00 && sys->rom_0000_on) {
                data = sys->rom[addr]; // 0000-0FFF Monitor ROM
            } else if (addr_hi == 0x01 && sys->rom_1000_on) {
                data = sys->rom[addr]; // 1000-1FFF CGROM
            } else if (addr_hi == 0x0E && sys->rom_e000_on) {
                // E000-EFFF: memory-mapped IO at E000-E008, ROM above
                uint16_t addr_low = addr & 0x0FFF;
                if (addr_low <= 0x08) {
                    // Memory-mapped peripherals
                    if (addr_low == 0x08) {
                        // E008: DMD status register (same as IN 0xCE)
                        data = sys->mz800_switch ? 0x02 : 0x00;
                        if (sys->line_counter >= 200) data |= 0x40;
                        if (sys->line_counter >= 200 && sys->line_counter < 203) data |= 0x10;
                        if (sys->line_cycle >= 200) {
                            data |= 0x80;
                            if (sys->line_cycle >= 210) data |= 0x20;
                        }
                        data |= (sys->tempo & 1);
                    } else if (addr_low & 0x04) {
                        // E004-E006: CTC8253 read. E007: control (unreadable)
                        if (addr_low == 0x07) {
                            data = sys->dbus_latch;
                        } else {
                            data = i8253_read(&sys->pit, addr_low & 0x03);
                        }
                    } else {
                        // E000-E003: PIO8255 read
                        int key_col = sys->ppi.pa.outp & 0x0F;
                        uint8_t kb_lines = 0xFF;
                        if (key_col < 10) {
                            kb_lines = ~(kbd_test_lines(&sys->kbd, (1 << key_col))) & 0xFF;
                        }
                        uint64_t ppi_pins = 0;
                        ppi_pins |= (addr_low & 3);
                        ppi_pins |= I8255_CS | I8255_RD;
                        I8255_SET_PB(ppi_pins, kb_lines);
                        ppi_pins = i8255_tick(&sys->ppi, ppi_pins);
                        data = I8255_GET_DATA(ppi_pins);
                    }
                } else {
                    data = sys->rom[addr & 0x3FFF]; // E010-EFFF: upper ROM
                }
            } else if (addr_hi == 0x0F && sys->rom_e000_on) {
                data = sys->rom[addr & 0x3FFF]; // F000-FFFF: upper ROM
            } else if ((addr_hi == 0x08 || addr_hi == 0x09) && !dmd_mz700 && sys->vram_on) {
                // 8000-9FFF: MZ-800 VRAM (always mapped when vram_on in 800 mode)
                uint16_t vaddr = addr - 0x8000;
                int addr_is_odd = vaddr & 0x01;
                if (dmd_scrw640) vaddr >>= 1;
                vaddr = hwscroll_shift_addr(sys, vaddr);
                vaddr &= 0x1FFF;
                data = mz800_vram_read(sys, vaddr, addr_is_odd, dmd_scrw640, dmd_hicolor);
            } else if ((addr_hi == 0x0A || addr_hi == 0x0B) && !dmd_mz700 && sys->vram_on && dmd_scrw640) {
                // A000-BFFF: VRAM only in 640-wide mode
                uint16_t vaddr = addr - 0x8000;
                int addr_is_odd = vaddr & 0x01;
                vaddr >>= 1;
                vaddr = hwscroll_shift_addr(sys, vaddr);
                vaddr &= 0x1FFF;
                data = mz800_vram_read(sys, vaddr, addr_is_odd, dmd_scrw640, dmd_hicolor);
            } else if (addr_hi == 0x0C && dmd_mz700 && sys->vram_on) {
                // C000-CFFF: MZ-700 CG-RAM → VRAM plane I lower half
                data = sys->vram[0][addr & 0x0FFF];
            } else if (addr_hi == 0x0D && dmd_mz700 && sys->rom_e000_on) {
                // D000-DFFF: MZ-700 attribute VRAM → VRAM plane I upper half
                data = sys->vram[0][0x1000 | (addr & 0x0FFF)];
            } else {
                data = sys->ram[addr];
            }
            
            sys->dbus_latch = data;
            Z80_SET_DATA(pins, data);
        } else if (pins & Z80_WR) {
            uint8_t data = Z80_GET_DATA(pins);
            uint8_t addr_hi = addr >> 12;
            
            // Write protection: writes under mapped ROM are discarded silently
            if (addr_hi == 0x00 && sys->rom_0000_on) {
                // discard write under ROM
            } else if (addr_hi == 0x01 && sys->rom_1000_on) {
                // discard write under CGROM
            } else if (addr_hi == 0x0E && sys->rom_e000_on) {
                // E000-E008: memory-mapped IO write
                uint16_t addr_low = addr & 0x0FFF;
                if (addr_low <= 0x08) {
                    if (addr_low == 0x08) {
                        // E008: DMD write (same as OUT 0xCE)
                        sys->gdg_dmd = data & 0x0F;
                    } else if (addr_low & 0x04) {
                        // E004-E007: CTC8253 write
                        i8253_write(&sys->pit, addr_low & 0x03, data);
                    } else {
                        // E000-E003: PIO8255 write
                        int key_col = sys->ppi.pa.outp & 0x0F;
                        uint8_t kb_lines = 0xFF;
                        if (key_col < 10) {
                            kb_lines = ~(kbd_test_lines(&sys->kbd, (1 << key_col))) & 0xFF;
                        }
                        uint64_t ppi_pins = 0;
                        ppi_pins |= (addr_low & 3);
                        ppi_pins |= I8255_CS | I8255_WR;
                        I8255_SET_DATA(ppi_pins, data);
                        I8255_SET_PB(ppi_pins, kb_lines);
                        i8255_tick(&sys->ppi, ppi_pins);
                    }
                }
                // E010+: writes under ROM are discarded
            } else if (addr_hi == 0x0F && sys->rom_e000_on) {
                // discard write under ROM
            } else if ((addr_hi == 0x08 || addr_hi == 0x09) && !dmd_mz700 && sys->vram_on) {
                // 8000-9FFF: MZ-800 VRAM write
                uint16_t vaddr = addr - 0x8000;
                int addr_is_odd = vaddr & 0x01;
                if (dmd_scrw640) vaddr >>= 1;
                vaddr = hwscroll_shift_addr(sys, vaddr);
                vaddr &= 0x1FFF;
                mz800_vram_write(sys, vaddr, data, addr_is_odd, dmd_scrw640, dmd_hicolor);
            } else if ((addr_hi == 0x0A || addr_hi == 0x0B) && !dmd_mz700 && sys->vram_on && dmd_scrw640) {
                // A000-BFFF: VRAM write only in 640-wide mode
                uint16_t vaddr = addr - 0x8000;
                int addr_is_odd = vaddr & 0x01;
                vaddr >>= 1;
                vaddr = hwscroll_shift_addr(sys, vaddr);
                vaddr &= 0x1FFF;
                mz800_vram_write(sys, vaddr, data, addr_is_odd, dmd_scrw640, dmd_hicolor);
            } else if (addr_hi == 0x0C && dmd_mz700 && sys->vram_on) {
                // C000-CFFF: MZ-700 CG-RAM write
                sys->vram[0][addr & 0x0FFF] = data;
            } else if (addr_hi == 0x0D && dmd_mz700 && sys->rom_e000_on) {
                // D000-DFFF: MZ-700 attribute VRAM write
                sys->vram[0][0x1000 | (addr & 0x0FFF)] = data;
            } else {
                sys->ram[addr] = data;
            }
        }
    } else if (pins & Z80_IORQ) {
        const uint16_t port_full = Z80_GET_ADDR(pins); // Full 16-bit port address
        const uint16_t port = port_full & 0xFF;
        const uint8_t port_hi = (port_full >> 8) & 0xFF;
        
        if (pins & Z80_WR) {
            bool dmd_mz700_mode = (sys->gdg_dmd & 0x08) != 0;
            if (port >= 0xD4 && port <= 0xD7) {
                if (!dmd_mz700_mode) {
                    i8253_write(&sys->pit, port, Z80_GET_DATA(pins));
                }
            } else if (port >= 0xD0 && port <= 0xD3) {
                if (!dmd_mz700_mode) {
                    // Determine keyboard active column based on port A output
                    int key_col = sys->ppi.pa.outp & 0x0F;
                    uint8_t kb_lines = 0xFF; // unpressed
                    if (key_col < 10) {
                        kb_lines = ~(kbd_test_lines(&sys->kbd, (1 << key_col))) & 0xFF;
                    }

                    // Map Z80 pins to i8255 pins before tick
                    uint64_t ppi_pins = 0;
                    ppi_pins |= (port & 3); // A0, A1
                    ppi_pins |= I8255_CS | I8255_WR;
                    I8255_SET_DATA(ppi_pins, Z80_GET_DATA(pins));
                    I8255_SET_PB(ppi_pins, kb_lines); // Feed keyboard lines to port B
                    i8255_tick(&sys->ppi, ppi_pins);
                }
            } else {
                uint8_t data_io = Z80_GET_DATA(pins);
                switch (port) {
                    case 0xCC: // WF register
                        sys->gdg_wf_plane = data_io & 0x0F;
                        sys->gdg_wfrf_vbank = (data_io >> 4) & 0x01;
                        if (data_io & 0x80) {
                            sys->gdg_wf_mode = (data_io >> 5) & 0x06; // 0,2,4,6
                        } else {
                            sys->gdg_wf_mode = (data_io >> 5) & 0x07; // 0-7
                        }
                        break;
                    case 0xCD: // RF register
                        sys->gdg_rf_plane = data_io & 0x0F;
                        sys->gdg_wfrf_vbank = (data_io >> 4) & 0x01;
                        sys->gdg_rf_search = (data_io >> 7) & 0x01;
                        break;
                    case 0xCE: // DMD register (4 bits only)
                        sys->gdg_dmd = data_io & 0x0F;
                        break;
                    case 0xCF: { // Hardware scroll + border (selected by port high byte)
                        if (port_hi >= 1 && port_hi <= 5) {
                            switch (port_hi) {
                                case 1: // SOF_LO
                                    sys->gdg_sof &= 0x03 << 11;
                                    sys->gdg_sof |= (data_io & 0xFF) << 3;
                                    break;
                                case 2: // SOF_HI
                                    sys->gdg_sof &= 0xFF << 3;
                                    sys->gdg_sof |= (data_io & 0x03) << 11;
                                    break;
                                case 3: // SW
                                    sys->gdg_sw = (data_io & 0x7F) << 6;
                                    break;
                                case 4: // SSA
                                    sys->gdg_ssa = (data_io & 0x7F) << 6;
                                    break;
                                case 5: // SEA
                                    sys->gdg_sea = (data_io & 0x7F) << 6;
                                    break;
                            }
                            hwscroll_regs_changed(sys);
                        } else if (port_hi == 6) {
                            sys->gdg_border = data_io & 0x0F;
                        } else if (port_hi == 7) {
                            // PAL/NTSC selection: bit 7 set = NTSC
                            mz800_set_video_standard(sys, (data_io & 0x80) == 0);
                        }
                        break;
                    }
                    case 0xF0: { // Palette register (single port, bit 6 selects group vs color)
                        if (data_io & 0x40) {
                            // Set palette group
                            sys->gdg_palgrp = data_io & 0x03;
                        } else {
                            // Set palette color: bits 4-5 = combination index, bits 0-3 = IGRB
                            uint8_t pal_value = data_io & 0x0F;
                            uint8_t pal_idx = (data_io >> 4) & 0x03;
                            sys->gdg_pal[pal_idx] = pal_value;
                        }
                        break;
                    }
                    
                    case 0xE0: // OUT E0: 0x0000-0x7FFF to DRAM
                        sys->rom_0000_on = false;
                        sys->rom_1000_on = false;
                        break;
                    case 0xE1: // OUT E1: 0xE000-0xFFFF to DRAM
                        sys->rom_e000_on = false;
                        break;
                    case 0xE2: // OUT E2: 0x0000-0x0FFF to Monitor ROM
                        sys->rom_0000_on = true;
                        break;
                    case 0xE3: // OUT E3: 0xE000-0xFFFF to Monitor ROM
                        sys->rom_e000_on = true;
                        break;
                    case 0xE4: // OUT E4: Full reset state (MZ-700 mode: no CGROM/VRAM)
                        sys->rom_0000_on = true;
                        sys->rom_1000_on = true;
                        sys->rom_e000_on = true;
                        sys->vram_on = true;
                        if (sys->gdg_dmd & 0x08) { // MZ-700 mode
                            sys->rom_1000_on = false;
                            sys->vram_on = false;
                        }
                        break;
                    
                    case 0xF2: // PSG SN76489AN write
                        psg_write_byte_internal(&sys->psg, data_io);
                        break;
                    
                    case 0x01: { // CMT hack RHEAD — copy pre-loaded header to Z80 HL
                        if (sys->cmt.has_file) {
                            uint16_t dest = sys->cpu.hl;
                            for (int i = 0; i < MZF_HEADER_SIZE; i++) {
                                sys->ram[(dest + i) & 0xFFFF] = sys->cmt.header[i];
                            }
                            sys->cpu.a = 0;
                            sys->cpu.f &= ~0x01; // clear carry
                            sys->cmt.header_loaded = true;
                        } else {
                            sys->cpu.a = 2; // BREAK
                            sys->cpu.f |= 0x01; // set carry
                        }
                        break;
                    }
                    case 0x02: { // CMT hack RDATA — copy pre-loaded body to Z80 HL for BC bytes
                        if (sys->cmt.has_file && sys->cmt.header_loaded && sys->cmt.body) {
                            uint16_t dest = sys->cpu.hl;
                            uint16_t size = sys->cpu.bc;
                            uint32_t copy_size = (size <= sys->cmt.body_size) ? size : sys->cmt.body_size;
                            for (uint32_t i = 0; i < copy_size; i++) {
                                sys->ram[(dest + i) & 0xFFFF] = sys->cmt.body[i];
                            }
                            sys->cpu.a = 0;
                            sys->cpu.f &= ~0x01; // clear carry
                        } else {
                            sys->cpu.a = 2;
                            sys->cpu.f |= 0x01;
                        }
                        break;
                    }
                }
            }
        } else if (pins & Z80_RD) {
            bool dmd_mz700_mode = (sys->gdg_dmd & 0x08) != 0;
            uint8_t data = sys->dbus_latch; // default: last bus value
            if (port >= 0xD4 && port <= 0xD7) {
                if (!dmd_mz700_mode) {
                    data = i8253_read(&sys->pit, port);
                }
            } else if (port >= 0xD0 && port <= 0xD3) {
                if (!dmd_mz700_mode) {
                    // Determine keyboard active column based on port A output
                    int key_col = sys->ppi.pa.outp & 0x0F;
                    uint8_t kb_lines = 0xFF; // unpressed
                    if (key_col < 10) {
                        kb_lines = ~(kbd_test_lines(&sys->kbd, (1 << key_col))) & 0xFF;
                    }

                    // Read from 8255
                    uint64_t ppi_pins = 0;
                    ppi_pins |= (port & 3); // A0, A1
                    ppi_pins |= I8255_CS | I8255_RD;
                    I8255_SET_PB(ppi_pins, kb_lines); // Feed keyboard lines to port B
                    ppi_pins = i8255_tick(&sys->ppi, ppi_pins);
                    data = I8255_GET_DATA(ppi_pins);
                }
            } else {
                switch (port) {
                    case 0xCE: {
                        // Video status register (pixel-accurate, PAL/NTSC parameterized)
                        // Bit 7: HBLANK, Bit 6: VBLANK, Bit 5: HSYNC, Bit 4: VSYNC
                        // Bit 1: hardware switch (1=MZ-800), Bit 0: TEMPO
                        data = sys->mz800_switch ? 0x02 : 0x00;
                        // VBLANK: outside display area
                        if (sys->line_counter >= sys->vt.vblank_first_line ||
                            sys->line_counter < sys->vt.vblank_resume_line) {
                            data |= 0x40;
                        }
                        // VSYNC: last 3 lines
                        if (sys->line_counter >= sys->vt.vsync_first_line) {
                            data |= 0x10;
                        }
                        // HBLANK: outside enabled region
                        if (sys->pixel_line >= sys->vt.hblank_start ||
                            sys->pixel_line < sys->vt.hblank_end) {
                            data |= 0x80;
                        }
                        // HSYNC: beginning of line
                        if (sys->pixel_line < sys->vt.hsync_width) {
                            data |= 0x20;
                        }
                        data |= (sys->tempo & 1);
                        break;
                    }
                    case 0xE0: // IN E0: map CGROM + VRAM
                        sys->rom_1000_on = true;
                        sys->vram_on = true;
                        break;
                    case 0xE1: // IN E1: unmap CGROM + VRAM
                        sys->rom_1000_on = false;
                        sys->vram_on = false;
                        break;
                    case 0xF0: // JOY1 — gated by PPI PA bit 5 (active low)
                        if (!(sys->ppi.pa.outp & 0x20)) {
                            data = sys->joy[0].state;
                        }
                        break;
                    case 0xF1: // JOY2 — gated by PPI PA bit 6 (active low)
                        if (!(sys->ppi.pa.outp & 0x40)) {
                            data = sys->joy[1].state;
                        }
                        break;
                }
            }
            Z80_SET_DATA(pins, data);
        }
    }

    // === Timing: advance vt.cpu_divider pixel clocks per CPU tick ===
    // PAL: cpu_divider=5 (CLK/5). NTSC: cpu_divider=4 (CLK/4). Both give ~3.55 MHz CPU.
    // CTC0 clocks every 16 pixel clocks (CLK/16, standard-independent).
    // PSG steps every vt.psg_pixel_divider pixel clocks (16 CPU ticks: 80 px PAL, 64 px NTSC).
    // Line = vt.pixels_per_line pixel clocks (1136 PAL, 912 NTSC).
    // Mid-frame PAL↔NTSC switching: counters wrap naturally at new limits.
    
    const uint32_t cpu_div = sys->vt.cpu_divider;
    for (uint32_t px = 0; px < cpu_div; px++) {
        sys->ctc0_sub++;
        if (sys->ctc0_sub >= 16) {
            sys->ctc0_sub = 0;
            i8253_tick(&sys->pit, 0);
        }
        
        sys->psg_sub++;
        if (sys->psg_sub >= sys->vt.psg_pixel_divider) {
            sys->psg_sub = 0;
            psg_step(&sys->psg);
        }
        
        sys->pixel_line++;
        if (sys->pixel_line >= sys->vt.pixels_per_line) {
            sys->pixel_line = 0;
            sys->line_counter++;

            // CTC1 clocked by HSYNC falling edge (once per line)
            i8253_tick(&sys->pit, 1);
            if (sys->pit.channels[1].output) {
                i8253_tick(&sys->pit, 2);
            }

            // TEMPO toggles every N scanlines (~34 Hz for both PAL and NTSC)
            sys->tempo_divider++;
            if (sys->tempo_divider >= sys->vt.tempo_toggle_lines) {
                sys->tempo_divider = 0;
                sys->tempo++;
            }

            if (sys->line_counter >= sys->vt.lines_per_frame) {
                sys->line_counter = 0;
            }
        }
    }
    // Derive CPU-level line_cycle for status register reads
    sys->line_cycle = sys->pixel_line / sys->vt.cpu_divider;

    // CTC2 output drives Z80 INT (masked by PPI PC2)
    uint8_t pc2_mask = (sys->ppi.pc.outp >> 2) & 0x01;
    if (sys->pit.channels[2].output && pc2_mask) {
        pins |= Z80_INT;
    }
    
    // Interrupt acknowledge (M1 + IORQ)
    if ((pins & Z80_M1) && (pins & Z80_IORQ)) {
        sys->pit.channels[2].output = false; 
    }

    sys->pins = pins;
    sys->tick_count++;
}

void mz800_sys_reset(mz800_sys_t* sys)
{
    sys->pins = z80_reset(&sys->cpu);
    i8253_reset(&sys->pit);
    i8255_reset(&sys->ppi);
    sys->rom_0000_on = true;
    sys->rom_1000_on = true;
    sys->rom_e000_on = true;
    sys->vram_on = false;  // OFF at reset — monitor ROM enables via OUT E4
    sys->gdg_dmd = 0;
    // Preserve PAL/NTSC setting across reset, but refresh timing struct
    mz800_set_video_standard(sys, sys->bPAL);
    sys->gdg_wf_plane = 0;
    sys->gdg_wf_mode = 0;
    sys->gdg_wfrf_vbank = 0;
    sys->gdg_rf_plane = 0;
    sys->gdg_rf_search = 0;
    sys->gdg_palgrp = 0;
    memset(sys->gdg_pal, 0, sizeof(sys->gdg_pal));
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

    // PSG reset — all channels off
    memset(&sys->psg, 0, sizeof(sys->psg));
    for (int i = 0; i < PSG_CHANNELS; i++) {
        sys->psg.ch[i].attn = 15;
        sys->psg.ch[i].output = 1;
    }
    sys->psg.ch[3].type = 1;
    sys->psg.ch[3].shift_reg = 1 << 15;

    // CMT: keep loaded file, reset load state
    sys->cmt.header_loaded = false;

    // Joystick: all released
    sys->joy[0].state = 0xFF;
    sys->joy[1].state = 0xFF;
}

void mz800_sys_key_down(mz800_sys_t* sys, int key) {
    if (key >= 0 && key < 256) {
        kbd_key_down(&sys->kbd, key);
    }
}

void mz800_sys_key_up(mz800_sys_t* sys, int key) {
    if (key >= 0 && key < 256) {
        kbd_key_up(&sys->kbd, key);
    }
}

// --- CMT hack: install ROM patches for RHEAD/RDATA interception ---
static void cmthack_install_rom_patches(mz800_sys_t* sys) {
    // RHEAD patch at 0x04D8: push HL; ld HL,0x10F0; out (0x01),a; pop HL; ret
    sys->rom[0x04D8] = 0xE5;
    sys->rom[0x04D9] = 0x21;
    sys->rom[0x04DA] = 0xF0;
    sys->rom[0x04DB] = 0x10;
    sys->rom[0x04DC] = 0xD3;
    sys->rom[0x04DD] = 0x01;
    sys->rom[0x04DE] = 0xE1;
    sys->rom[0x04DF] = 0xC9;

    // RDATA patch at 0x04F8: push HL; push BC; ld HL,(0x1104); ld BC,(0x1102); out (0x02),a; pop BC; pop HL; ret
    sys->rom[0x04F8] = 0xE5;
    sys->rom[0x04F9] = 0xC5;
    sys->rom[0x04FA] = 0x2A;
    sys->rom[0x04FB] = 0x04;
    sys->rom[0x04FC] = 0x11;
    sys->rom[0x04FD] = 0xED;
    sys->rom[0x04FE] = 0x4B;
    sys->rom[0x04FF] = 0x02;
    sys->rom[0x0500] = 0x11;
    sys->rom[0x0501] = 0xD3;
    sys->rom[0x0502] = 0x02;
    sys->rom[0x0503] = 0xC1;
    sys->rom[0x0504] = 0xE1;
    sys->rom[0x0505] = 0xC9;
}

bool mz800_sys_load_mzf(mz800_sys_t* sys, const uint8_t* data, uint32_t size) {
    if (!data || size < MZF_HEADER_SIZE) return false;

    memcpy(sys->cmt.header, data, MZF_HEADER_SIZE);
    sys->cmt.body = (uint8_t*)(data + MZF_HEADER_SIZE);
    sys->cmt.body_size = size - MZF_HEADER_SIZE;
    sys->cmt.has_file = true;
    sys->cmt.header_loaded = false;

    // Install ROM patches so monitor ROM's RHEAD/RDATA calls trigger OUT 0x01/0x02
    cmthack_install_rom_patches(sys);

    return true;
}

