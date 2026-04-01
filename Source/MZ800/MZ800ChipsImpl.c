

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

    // GDG state initialization
    mz800_gdg_init(sys, true);

    // PSG init
    sn76489an_init(&sys->psg);

    // CMT hack
    memset(&sys->cmt, 0, sizeof(sys->cmt));

    // Z80 PIO (chips library)
    z80pio_init(&sys->pio);
    // CTC edge detection / VBLN
    sys->vblank_active  = false;
    sys->ctc1_prev_out  = 0;
    sys->ctc0_prev_out  = 0;
    // CTC0 gate starts LOW (controlled by E008); CTC1/CTC2 gates set in i8253_init

    // Joystick — all released (active low = 0xFF)
    sys->joy[0].state = 0xFF;
    sys->joy[1].state = 0xFF;

    // --- Hardware signal emulation fields ---
    sys->vras = false;
    sys->vcas = false;
    sys->voe = false;
    sys->vod = 0;
    sys->vrwr = false;
    sys->hbln = false;
    sys->cpu_wr = false;
    sys->vram_wr = false;
}

void mz800_sys_tick(mz800_sys_t* sys)
{

    // --- GDG: update per-pixel timing and VRAM wait logic ---
    mz800_gdg_tick(sys);

    // --- Z80 WAIT pin assertion for VRAM stalls ---
    if (sys->cpu_wait_vram) {
        sys->pins |= Z80_WAIT;
    } else {
        sys->pins &= ~Z80_WAIT;
    }

    uint64_t pins = z80_tick(&sys->cpu, sys->pins);

    // RETI handled by z80pio_tick via shared RETI pin

    bool dmd_scrw640 = (sys->gdg_dmd & 0x04) != 0;
    bool dmd_hicolor = (sys->gdg_dmd & 0x02) != 0;
    bool dmd_mz700   = (sys->gdg_dmd & 0x08) != 0;

    // --- Forbidden/illegal GDG mode detection ---
    // Forbidden if both SCRW640 and HICOLOR are set (MZ-800 mode)
    sys->forbidden_mode = (!dmd_mz700) && dmd_scrw640 && dmd_hicolor;

    if (pins & Z80_MREQ) {
        const uint16_t addr = Z80_GET_ADDR(pins);
        if (pins & Z80_RD) {
            uint8_t data = 0xFF;
            uint8_t addr_hi = addr >> 12;
            if (addr_hi == 0x00 && sys->rom_0000_on) {
                data = sys->rom[addr];
            } else if (addr_hi == 0x01 && sys->rom_1000_on) {
                data = sys->rom[addr];
            } else if (addr_hi == 0x0E && sys->rom_e000_on) {
                uint16_t addr_low = addr & 0x0FFF;
                if (addr_low <= 0x08) {
                    if (addr_low == 0x08) {
                        data = 0x00;
                        if (sys->pixel_line >= sys->vt.hblank_start ||
                            sys->pixel_line < sys->vt.hblank_end) {
                            data |= 0x80;
                        }
                        data |= (sys->tempo & 1);
                    } else if (addr_low & 0x04) {
                        if (addr_low == 0x07) {
                            data = sys->dbus_latch;
                        } else {
                            data = i8253_read(&sys->pit, addr_low & 0x03);
                        }
                    } else {
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
                    data = sys->rom[addr & 0x3FFF];
                }
            } else if (addr_hi == 0x0F && sys->rom_e000_on) {
                data = sys->rom[addr & 0x3FFF];
            } else if (
                (addr_hi == 0x08 || addr_hi == 0x09 || addr_hi == 0x0A || addr_hi == 0x0B || addr_hi == 0x0C || addr_hi == 0x0D)
            ) {
                data = mz800_gdg_mem_read(sys, addr, pins);
                if (data == 0xFF) {
                    data = sys->ram[addr];
                }
            } else {
                data = sys->ram[addr];
            }
            sys->dbus_latch = data;
            Z80_SET_DATA(pins, data);
        } else if (pins & Z80_WR) {
            uint8_t data = Z80_GET_DATA(pins);
            uint8_t addr_hi = addr >> 12;
            if (addr_hi == 0x00 && sys->rom_0000_on) {
                // discard write under ROM
            } else if (addr_hi == 0x01 && sys->rom_1000_on) {
                // discard write under CGROM
            } else if (addr_hi == 0x0E && sys->rom_e000_on) {
                uint16_t addr_low = addr & 0x0FFF;
                if (addr_low <= 0x08) {
                    if (addr_low == 0x08) {
                        uint8_t new_g7 = data & 0x01;
                        if (new_g7 != sys->gdg_regct53g7) {
                            sys->gdg_regct53g7 = new_g7;
                            if (sys->gdg_dmd & 0x08) {
                                i8253_gate(&sys->pit, 0, new_g7);
                            }
                        }
                    } else if (addr_low & 0x04) {
                        i8253_write(&sys->pit, addr_low & 0x03, data);
                    } else {
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
            } else if (addr_hi == 0x0F && sys->rom_e000_on) {
                // discard write under ROM
            } else if (
                (addr_hi == 0x08 || addr_hi == 0x09 || addr_hi == 0x0A || addr_hi == 0x0B || addr_hi == 0x0C || addr_hi == 0x0D)
            ) {
                mz800_gdg_mem_write(sys, addr, data);
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
                        mz800_gdg_write_wf(sys, data_io);
                        break;
                    case 0xCD: // RF register
                        mz800_gdg_write_rf(sys, data_io);
                        break;
                    case 0xCE: // DMD register (4 bits only)
                        mz800_gdg_write_dmd(sys, data_io);
                        break;
                    case 0xCF: // Hardware scroll + border + superimpose/CKSW (selected by port high byte)
                        if (port_hi >= 1 && port_hi <= 5) {
                            mz800_gdg_write_scroll(sys, port_hi, data_io);
                        } else if (port_hi == 6) {
                            mz800_gdg_write_border(sys, data_io);
                        } else if (port_hi == 7) {
                            mz800_gdg_write_superimpose(sys, data_io);
                        }
                        break;
                    case 0xF0: // Palette register (single port, bit 6 selects group vs color)
                        mz800_gdg_write_palette(sys, data_io);
                        break;
                    
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
                        sn76489an_write(&sys->psg, data_io);
                        break;
                    // Z80 PIO (0xFC-0xFF) handled by z80pio_tick below
                    
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
                    case 0xFC: case 0xFD: case 0xFE: case 0xFF: // Z80 PIO handled by z80pio_tick below
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
        // --- Hardware signal: HBLN (HBLANK) ---
        if (sys->pixel_line >= sys->vt.hblank_start || sys->pixel_line < sys->vt.hblank_end) {
            sys->hbln = true;
        } else {
            sys->hbln = false;
        }

        // --- Hardware signal: VRAS (VRAM Row Strobe) ---
        // DISP phase (vras_phase==0): GDG owns VRAM, VRAS active
        // CPU phase (vras_phase==1): CPU can access VRAM, VRAS inactive
        sys->vras = (sys->vras_phase == 0);

        // --- Hardware signal: VCAS (VRAM Column Strobe) ---
        // VCAS is strobed at the first pixel of each 8-pixel DISP phase (vras_phase==0 && vras_sub==0)
        sys->vcas = (sys->vras_phase == 0 && sys->vras_sub == 0);

        // --- Hardware signal: VOE (Video Output Enable) ---
        // VOE is true during visible scanlines and outside HBLANK
        sys->voe =
            (sys->line_counter >= sys->vt.vdisp_first_line &&
             sys->line_counter < sys->vt.vdisp_last_line &&
             !(sys->pixel_line >= sys->vt.hblank_start || sys->pixel_line < sys->vt.hblank_end));

        // --- Hardware signal: VOD (VRAM address output) ---
        // VOD is the current VRAM address output by GDG during DISP phase, else 0
        if (sys->vras_phase == 0) {
            // Use the current GDG VRAM address (gdg_vram_col) as VOD
            sys->vod = (uint8_t)(sys->gdg_vram_col & 0xFF);
        } else {
            sys->vod = 0;
        }

        // --- Hardware signal: VRWR (VRAM write strobe) ---
        // VRWR pulses true for one pixel tick when a buffered VRAM write is flushed during DISP phase
        // We detect this by checking if a write is pending and DISP phase is active
        if (sys->vras_phase == 0 && sys->gdg_wr_pending) {
            sys->vrwr = true;
        } else {
            sys->vrwr = false;
        }

        // --- Hardware signal: CPU.WR (CPU write cycle) ---
        // True if the CPU is performing a write cycle to memory (MREQ+WR)
        if ((sys->pins & Z80_MREQ) && (sys->pins & Z80_WR)) {
            sys->cpu_wr = true;
        } else {
            sys->cpu_wr = false;
        }

        // --- Hardware signal: VRAM.WR (actual VRAM write occurs) ---
        // Cleared after one tick; set in the CPU write handler
        if (sys->vram_wr) {
            sys->vram_wr = false;
        }
        sys->ctc0_sub++;
        if (sys->ctc0_sub >= 16) {
            sys->ctc0_sub = 0;
            i8253_tick(&sys->pit, 0);
        }
        
        sys->psg_sub++;
        if (sys->psg_sub >= sys->vt.psg_pixel_divider) {
            sys->psg_sub = 0;
            sn76489an_tick(&sys->psg);
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

            // Start-of-scanline GDG display engine setup (now in GDG module)
            mz800_gdg_scanline_start(sys);

            bool new_vblank = (sys->line_counter >= sys->vt.vblank_first_line ||
                               sys->line_counter < sys->vt.vblank_resume_line);
            if (new_vblank != sys->vblank_active) {
                sys->vblank_active = new_vblank;
            }
        }

        // GDG display engine: emit one pixel per pixel clock (now in GDG module)
        mz800_gdg_display_pixel(sys);
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

    // === Z80 PIO tick (chips library) ===
    // Build PIO pin mask from Z80 pins (shared pin positions: M1, IORQ, RD, INT, RETI, data bus)
    {
        uint64_t pio_pins = pins;

        // Address decode: CE if IORQ (without M1) and port 0xFC-0xFF
        bool is_pio_io = false;
        if ((pins & Z80_IORQ) && !(pins & Z80_M1)) {
            uint16_t pio_port = Z80_GET_ADDR(pins) & 0xFF;
            if (pio_port >= 0xFC) {
                is_pio_io = true;
                pio_pins |= Z80PIO_CE;
                // BASEL: addr bit 0 → port B select
                if (pio_port & 0x01) pio_pins |= Z80PIO_BASEL;
                // CDSEL: MZ-800 addr bit 1 inverted (bit1=0 → ctrl, bit1=1 → data)
                if (!(pio_port & 0x02)) pio_pins |= Z80PIO_CDSEL;
            }
        }

        // Port A inputs: bits 0-1 always high, bit 4 = ~CTC0, bit 5 = VBLANK
        uint8_t pa = 0x03;
        if (!sys->pit.channels[0].out) pa |= (1 << 4);
        if (sys->vblank_active) pa |= (1 << 5);
        Z80PIO_SET_PA(pio_pins, pa);
        Z80PIO_SET_PB(pio_pins, 0xFF);  // Port B: all high (pullups)

        // IEI: PIO is top of daisy chain
        pio_pins |= Z80PIO_IEIO;

        // Save interrupt state for acknowledge detection
        uint8_t pio_int_state_a = sys->pio.port[0].int_state;
        uint8_t pio_int_state_b = sys->pio.port[1].int_state;

        pio_pins = z80pio_tick(&sys->pio, pio_pins);

        // PIO read: extract data from PIO to Z80 data bus
        if (is_pio_io && (pins & Z80_RD)) {
            Z80_SET_DATA(pins, Z80PIO_GET_DATA(pio_pins));
        }

        // PIO INT → Z80 INT
        if (pio_pins & Z80PIO_INT) {
            pins |= Z80_INT;
        }

        // Interrupt acknowledge: detect PIO just transitioned to SERVICED
        if ((pins & Z80_M1) && (pins & Z80_IORQ)) {
            for (int i = 0; i < 2; i++) {
                uint8_t prev = (i == 0) ? pio_int_state_a : pio_int_state_b;
                if (!(prev & Z80PIO_INT_SERVICED) &&
                    (sys->pio.port[i].int_state & Z80PIO_INT_SERVICED)) {
                    Z80_SET_DATA(pins, Z80PIO_GET_DATA(pio_pins));
                    break;
                }
            }
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
    // GDG state reset
    mz800_gdg_reset(sys);

    // PSG reset
    sn76489an_reset(&sys->psg);

    // CMT: keep loaded file, reset load state
    sys->cmt.header_loaded = false;

    // Z80 PIO reset (chips library)
    z80pio_reset(&sys->pio);
    sys->vblank_active = false;
    sys->ctc1_prev_out = 0;
    sys->ctc0_prev_out = 0;
    // CTC0 gate starts LOW (i8253_init sets it); CTC1/CTC2 already HIGH from init

    // Joystick: all released
    sys->joy[0].state = 0xFF;
    sys->joy[1].state = 0xFF;

    // --- Hardware signal emulation fields ---
    sys->vras = false;
    sys->vcas = false;
    sys->voe = false;
    sys->vod = 0;
    sys->vrwr = false;
    sys->hbln = false;
    sys->cpu_wr = false;
    sys->vram_wr = false;
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



