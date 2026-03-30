#define CHIPS_IMPL
#include "MZ800ChipsImpl.h"

#include <string.h>

#define CHIPS_UTIL_IMPL
#include "util/z80dasm.h"
#include "util/m6502dasm.h"

mz800_sys_t g_mz800_sys;

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
    sys->gdg_regct53g7 = 0;
    sys->mz800_switch = true; // Hardware DIP: MZ-800 mode
    mz800_set_video_standard(sys, true); // Default: PAL
    sys->gdg_wf_plane = 0;
    sys->gdg_wf_mode = 0;
    sys->gdg_wfrf_vbank = 0;
    sys->gdg_rf_plane = 0;
    sys->gdg_rf_search = 0;
    sys->gdg_palgrp = 0;
    // Palette defaults match mz800emu: PAL0=0x09, PAL1=0x0F, PAL2=0x09, PAL3=0x0F
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

    // GDG display engine init
    memset(sys->fb, 0, sizeof(sys->fb));
    sys->fb_line = sys->fb;
    sys->vras_phase = 1;  // Start in CPU phase
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

    // PIOZ80
    pioz80_init(&sys->pioz80);
    // CTC edge detection / VBLN
    sys->vblank_active  = false;
    sys->ctc1_prev_out  = 0;
    sys->ctc0_prev_out  = 0;
    // CTC0 gate starts LOW (controlled by E008); CTC1/CTC2 gates set in i8253_init

    // Joystick — all released (active low = 0xFF)
    sys->joy[0].state = 0xFF;
    sys->joy[1].state = 0xFF;
}

void mz800_sys_tick(mz800_sys_t* sys)
{
    uint64_t pins = z80_tick(&sys->cpu, sys->pins);

    // RETI detection: Z80 sets RETI pin when it decodes the instruction
    if (pins & Z80_RETI) {
        pioz80_interrupt_reti(&sys->pioz80, sys);
    }

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
                        // E008: HBLANK (bit7) + TEMPO (bit0) — NOT the same as port 0xCE
                        data = 0x00;
                        if (sys->pixel_line >= sys->vt.hblank_start ||
                            sys->pixel_line < sys->vt.hblank_end) {
                            data |= 0x80;
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
                // 8000-9FFF: MZ-800 VRAM read
                // WAIT: CPU reads are stalled for one full DRAM access cycle.
                // Reference formula: wait = (16 - vras_pos) + conditional_8 + 16
                // This ensures the read aligns to a CPU phase boundary + one access cycle.
                {
                    int vras_pos = (sys->vras_phase == 0)
                        ? sys->vras_sub : (8 + sys->vras_sub);
                    int wait_ticks = 16 - vras_pos;
                    if (wait_ticks == 0) wait_ticks = 16;
                    if (wait_ticks <= 7) wait_ticks += 8;
                    wait_ticks += 16;
                    sys->vram_read_wait_counter = (uint8_t)wait_ticks;
                    sys->cpu_wait_vram = true;
                }
                uint16_t vaddr = addr - 0x8000;
                int addr_is_odd = vaddr & 0x01;
                if (dmd_scrw640) vaddr >>= 1;
                vaddr = mz800_hwscroll_addr(sys, vaddr);
                vaddr &= 0x1FFF;
                data = mz800_vram_read(sys, vaddr, addr_is_odd, dmd_scrw640, dmd_hicolor);
            } else if ((addr_hi == 0x0A || addr_hi == 0x0B) && !dmd_mz700 && sys->vram_on && dmd_scrw640) {
                // A000-BFFF: VRAM read only in 640-wide mode (same timing as 8000-9FFF)
                {
                    int vras_pos = (sys->vras_phase == 0)
                        ? sys->vras_sub : (8 + sys->vras_sub);
                    int wait_ticks = 16 - vras_pos;
                    if (wait_ticks == 0) wait_ticks = 16;
                    if (wait_ticks <= 7) wait_ticks += 8;
                    wait_ticks += 16;
                    sys->vram_read_wait_counter = (uint8_t)wait_ticks;
                    sys->cpu_wait_vram = true;
                }
                uint16_t vaddr = addr - 0x8000;
                int addr_is_odd = vaddr & 0x01;
                vaddr >>= 1;
                vaddr = mz800_hwscroll_addr(sys, vaddr);
                vaddr &= 0x1FFF;
                data = mz800_vram_read(sys, vaddr, addr_is_odd, dmd_scrw640, dmd_hicolor);
            } else if (addr_hi == 0x0C && dmd_mz700 && sys->vram_on) {
                // C000-CFFF: MZ-700 CG-RAM read → VRAM plane I lower half
                // MZ-700 mode: CPU waits for HBLANK if beam is in visible area
                if (!sys->mz700_in_hblank) {
                    sys->cpu_wait_vram = true;
                }
                data = sys->vram[0][addr & 0x0FFF];
            } else if (addr_hi == 0x0D && dmd_mz700 && sys->rom_e000_on) {
                // D000-DFFF: MZ-700 char/attr VRAM read → VRAM plane I upper half
                // MZ-700 mode: CPU waits for HBLANK if beam is in visible area
                if (!sys->mz700_in_hblank) {
                    sys->cpu_wait_vram = true;
                }
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
                        // E008: Gate CTC0 (bit0: 1=counting enabled, 0=stopped)
                        // Cache value in gdg_regct53g7; only call gate if changed
                        uint8_t new_g7 = data & 0x01;
                        if (new_g7 != sys->gdg_regct53g7) {
                            sys->gdg_regct53g7 = new_g7;
                            // In MZ-700 mode, CTC0 gate tracks regct53g7
                            // In MZ-800 mode, CTC0 gate is always 1 (set by DMD write)
                            if (sys->gdg_dmd & 0x08) {
                                i8253_gate(&sys->pit, 0, new_g7);
                            }
                        }
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
                // 8000-9FFF: MZ-800 VRAM write (buffered during DISP phase)
                uint16_t vaddr = addr - 0x8000;
                int addr_is_odd = vaddr & 0x01;
                if (dmd_scrw640) vaddr >>= 1;
                vaddr = mz800_hwscroll_addr(sys, vaddr);
                vaddr &= 0x1FFF;
                if (sys->vras_phase == 0) {
                    // DISP phase: buffer the write for later flush
                    sys->gdg_wr_pending = true;
                    sys->gdg_wr_addr = vaddr;
                    sys->gdg_wr_data = data;
                    sys->gdg_wr_odd = addr_is_odd;
                    sys->gdg_wr_scrw640 = dmd_scrw640;
                    sys->gdg_wr_hicolor = dmd_hicolor;
                } else {
                    mz800_vram_write(sys, vaddr, data, addr_is_odd, dmd_scrw640, dmd_hicolor);
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
                }
            } else if (addr_hi == 0x0C && dmd_mz700 && sys->vram_on) {
                // C000-CFFF: MZ-700 CG-RAM write
                // MZ-700 write latch: first write per visible scanline is free,
                // second and subsequent writes wait for HBLANK
                if (!sys->mz700_in_hblank && sys->mz700_wr_latch_count > 0) {
                    sys->cpu_wait_vram = true;
                }
                sys->mz700_wr_latch_count++;
                sys->vram[0][addr & 0x0FFF] = data;
            } else if (addr_hi == 0x0D && dmd_mz700 && sys->rom_e000_on) {
                // D000-DFFF: MZ-700 char/attr VRAM write
                // Same write latch logic as C000-CFFF
                if (!sys->mz700_in_hblank && sys->mz700_wr_latch_count > 0) {
                    sys->cpu_wait_vram = true;
                }
                sys->mz700_wr_latch_count++;
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
                        // DMD→CTC0 gate: MZ-800 mode forces gate HIGH,
                        // MZ-700 mode uses cached regct53g7
                        if (data_io & 0x08) {
                            i8253_gate(&sys->pit, 0, sys->gdg_regct53g7);
                        } else {
                            i8253_gate(&sys->pit, 0, 1);
                        }
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
                            mz800_hwscroll_regs_changed(sys);
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
                        mz800_psg_write_byte(&sys->psg, data_io);
                        break;
                    case 0xFC: case 0xFD: case 0xFE: case 0xFF: // Z80 PIO (PIOZ80)
                        pioz80_write(&sys->pioz80, port & 0x03, data_io, sys);
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
                    if ((port & 0x03) == 3) {
                        data = sys->dbus_latch; // D7 (CW register) is write-only
                    } else {
                        data = i8253_read(&sys->pit, port);
                    }
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
                    case 0xFC: case 0xFD: case 0xFE: case 0xFF: // Z80 PIO (PIOZ80)
                        data = pioz80_read(&sys->pioz80, port & 0x03, sys);
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
            uint8_t prev_ctc0_out = sys->pit.channels[0].out;
            i8253_tick(&sys->pit, 0);
            // CTC0 output change → PIOZ80 port A event (bit 4 = ~CTC0)
            if (sys->pit.channels[0].out != prev_ctc0_out) {
                pioz80_port_event(&sys->pioz80, 0, sys);
            }
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

            // CTC1 clocked by HSYNC (once per line); CTC2 clocked by CTC1 falling edge
            sys->ctc1_prev_out = sys->pit.channels[1].out;
            i8253_tick(&sys->pit, 1);
            if (sys->ctc1_prev_out && !sys->pit.channels[1].out) {
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

            // Start-of-scanline GDG display engine setup
            mz800_gdg_scanline_start(sys);

            bool new_vblank = (sys->line_counter >= sys->vt.vblank_first_line ||
                               sys->line_counter < sys->vt.vblank_resume_line);
            if (new_vblank != sys->vblank_active) {
                sys->vblank_active = new_vblank;
                // VBLANK edge → PIOZ80 port A event (bit 5)
                pioz80_port_event(&sys->pioz80, 0, sys);
            }
        }

        // GDG display engine: emit one pixel per pixel clock
        mz800_gdg_pixel_tick(sys);
    }
    // Derive CPU-level line_cycle for status register reads
    sys->line_cycle = sys->pixel_line / sys->vt.cpu_divider;

    // VRAM access WAIT — stall CPU during DISP phase
    if (sys->cpu_wait_vram) {
        pins |= Z80_WAIT;
    }

    // CTC2 output drives Z80 INT (masked by PPI PC2)
    uint8_t pc2_mask = (sys->ppi.pc.outp >> 2) & 0x01;
    if (sys->pit.channels[2].out && pc2_mask) {
        pins |= Z80_INT;
    }
    // PIOZ80 interrupt pending → Z80 INT
    if (sys->pioz80.interrupt == PIOZ80_INT_PENDING) {
        pins |= Z80_INT;
    }
    
    // Interrupt acknowledge (M1 + IORQ)
    if ((pins & Z80_M1) && (pins & Z80_IORQ)) {
        if (sys->pioz80.interrupt == PIOZ80_INT_PENDING) {
            // PIOZ80 has higher priority — deliver IM2 vector
            uint8_t vector = pioz80_interrupt_ack_im2(&sys->pioz80, sys);
            Z80_SET_DATA(pins, vector);
        }
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
    sys->gdg_regct53g7 = 0;
    // Preserve PAL/NTSC setting across reset, but refresh timing struct
    mz800_set_video_standard(sys, sys->bPAL);
    sys->gdg_wf_plane = 0;
    sys->gdg_wf_mode = 0;
    sys->gdg_wfrf_vbank = 0;
    sys->gdg_rf_plane = 0;
    sys->gdg_rf_search = 0;
    sys->gdg_palgrp = 0;
    // Palette defaults match mz800emu: PAL0=0x09, PAL1=0x0F, PAL2=0x09, PAL3=0x0F
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

    // GDG display engine reset
    memset(sys->fb, 0, sizeof(sys->fb));
    sys->fb_line = sys->fb;
    sys->vras_phase = 1;
    sys->vras_sub = 0;
    sys->cpu_wait_vram = false;
    sys->vram_read_wait_counter = 0;
    sys->mz700_wr_latch_count = 0;
    sys->mz700_in_hblank = true;  // start in HBLANK (before first visible line)
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

    // PIOZ80 reset
    pioz80_init(&sys->pioz80);
    sys->vblank_active = false;
    sys->ctc1_prev_out = 0;
    sys->ctc0_prev_out = 0;
    // CTC0 gate starts LOW (i8253_init sets it); CTC1/CTC2 already HIGH from init

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



