

#include "MZ800ChipsImpl.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* Boot trace: targeted events only */
static FILE* _bt_file = NULL;
static int _bt_count = 0;
static bool _bt_done = false;

static void _bt_log(const char* fmt, ...) {
    if (_bt_done) return;
    if (!_bt_file) {
        _bt_file = fopen("/tmp/mz800_boot_trace2.txt", "w");
        if (!_bt_file) { _bt_done = true; return; }
    }
    va_list ap;
    va_start(ap, fmt);
    vfprintf(_bt_file, fmt, ap);
    va_end(ap);
    _bt_count++;
    if (_bt_count % 50 == 0) fflush(_bt_file);
    if (_bt_count >= 2000) { fflush(_bt_file); fclose(_bt_file); _bt_file = NULL; _bt_done = true; }
}

#define CHIPS_UTIL_IMPL
#include "util/z80dasm.h"
#include "util/m6502dasm.h"

/* SN76489AN volume table: 2dB steps, 0=max, 15=off.
   Values are linear amplitude scaled to 0..1 range (per channel, summed). */
static const float _sn76489_vol[16] = {
    1.0f, 0.7943f, 0.6310f, 0.5012f, 0.3981f, 0.3162f, 0.2512f, 0.1995f,
    0.1585f, 0.1259f, 0.1000f, 0.0794f, 0.0631f, 0.0501f, 0.0398f, 0.0f,
};

/* Mix all 4 PSG channels + CTC0 beep into a single float sample */
static float _mz800_psg_sample(sn76489an_t* psg, uint8_t ctc0_audio) {
    float s = 0.0f;
    for (int i = 0; i < SN76489AN_NUM_CHANNELS; i++) {
        if (psg->chn[i].output) {
            s += _sn76489_vol[psg->chn[i].attn];
        }
    }
    /* CTC0 beep: treated as a 5th channel at full volume */
    if (ctc0_audio) {
        s += 1.0f;
    }
    return s * 0.2f;  /* normalize: 4 PSG channels + 1 CTC0 */
}

/* DC adjustment filter */
#define _MZ800_DCADJ_BUFLEN (512)

/* Compute PPI Port C upper nibble inputs (PC4-PC7 are INPUT on MZ-800).
   PC4: CMT motor sense (toggled by rising edge of PC3 output)
   PC5: CMT read data (0 = no tape)
   PC6: 556 cursor timer (toggles every 25 frames, ~0.5s at PAL 50Hz)
   PC7: VBLANK
   Returns the upper nibble value (bits 4-7 set appropriately, bits 0-3 zero). */
static uint8_t _mz800_ppi_pc_inputs(mz800_sys_t* sys) {
    uint8_t pc_hi = 0;
    if (sys->gdg.vblank_active) pc_hi |= 0x80;                    // PC7: VBLANK
    if ((sys->cursor_timer / 25) & 1) pc_hi |= 0x40;             // PC6: cursor blink
    /* PC5: CMT read data */
    if (cmt_read_data(&sys->cmt)) pc_hi |= 0x20;
    /* PC4: CMT motor sense (1 = motor off, 0 = motor on) */
    if (!cmt_motor_state(&sys->cmt)) pc_hi |= 0x10;
    return pc_hi;
}
static struct {
    float sum;
    int pos;
    float buf[_MZ800_DCADJ_BUFLEN];
} _mz800_dcadj;

static float _mz800_dcadjust(float s) {
    _mz800_dcadj.sum -= _mz800_dcadj.buf[_mz800_dcadj.pos];
    _mz800_dcadj.sum += s;
    _mz800_dcadj.buf[_mz800_dcadj.pos] = s;
    _mz800_dcadj.pos = (_mz800_dcadj.pos + 1) & (_MZ800_DCADJ_BUFLEN - 1);
    return s - (_mz800_dcadj.sum / _MZ800_DCADJ_BUFLEN);
}

/* Fixed-point scale for audio sample timing */
#define _MZ800_AUDIO_FP_SCALE (128)

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
    
    // Initial memory state is set by GDG init (bank_rom0000 etc.)

    // GDG state initialization
    {
        gdg_whid65040_desc_t gdg_desc;
        memset(&gdg_desc, 0, sizeof(gdg_desc));
        gdg_desc.pal = true;
        gdg_desc.mz800_mode = true;
        gdg_desc.cgrom = &sys->rom[0x1000];
        gdg_whid65040_init(&sys->gdg, &gdg_desc);
    }

    // PSG init
    sn76489an_init(&sys->psg);

    // WD2793 FDC init
    wd2793_init(&sys->fdc);

    // CMT tape init
    cmt_init(&sys->cmt, sys->gdg.bPAL);

    // Z80 PIO (chips library)
    z80pio_init(&sys->pio);
    // CTC edge detection
    sys->ctc1_prev_out  = 0;
    sys->ctc0_prev_out  = 0;
    // CTC0 gate starts LOW (controlled by E008); CTC1/CTC2 gates set in i8253_init

    // Joystick — all released (active low = 0xFF)
    sys->joy[0].state = 0xFF;
    sys->joy[1].state = 0xFF;
}

void mz800_sys_tick(mz800_sys_t* sys)
{
    // --- Z80 WAIT pin assertion for GDG VRAM stalls ---
    if (sys->gdg.cpu_wait) {
        sys->pins |= Z80_WAIT;
    } else {
        sys->pins &= ~Z80_WAIT;
    }

    uint64_t pins = z80_tick(&sys->cpu, sys->pins);

    // RETI handled by z80pio_tick via shared RETI pin

    bool dmd_mz700   = (sys->gdg.dmd & GDG_DMD_MZ700) != 0;

    if (pins & Z80_MREQ) {
        const uint16_t addr = Z80_GET_ADDR(pins);
        if (pins & Z80_RD) {
            uint8_t data = 0xFF;
            uint8_t addr_hi = addr >> 12;
            if (addr_hi == 0x00 && sys->gdg.bank_rom0000) {
                data = sys->rom[addr];
            } else if (addr_hi == 0x01 && sys->gdg.bank_rom1000) {
                data = sys->rom[addr];
                /* Trace first CGROM read */
                static int cgrd_cnt = 0;
                if (cgrd_cnt < 5) {
                    _bt_log("RD CGROM %04X=%02X PC=%04X tick=%u\n",
                        addr, data, sys->cpu.pc, sys->tick_count);
                    cgrd_cnt++;
                }
            } else if (addr_hi == 0x0E && sys->gdg.bank_rome000) {
                uint16_t addr_low = addr & 0x0FFF;
                if (addr_low <= 0x08) {
                    if (addr_low == 0x08) {
                        /* E008 Custom LSI status = GDG status register (same as port CE read).
                         * Contains: TEMPO, HBLANK, VBLANK, HSYNC, VSYNC, MZ-800 device flag.
                         * Reference: mz800-emuz maps E008 read → port 0xCE. */
                        data = gdg_whid65040_status(&sys->gdg);
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
                        I8255_SET_PC(ppi_pins, _mz800_ppi_pc_inputs(sys));
                        ppi_pins = i8255_tick(&sys->ppi, ppi_pins);
                        data = I8255_GET_DATA(ppi_pins);
                        if (sys->ppi_write_hook) sys->ppi_write_hook(sys->hook_user, sys->cpu.pc, addr_low & 3, data);
                    }
                } else {
                    data = sys->rom[addr & 0x3FFF];
                }
            } else if (addr_hi == 0x0F && sys->gdg.bank_rome000) {
                data = sys->rom[addr & 0x3FFF];
            } else if (
                (addr_hi == 0x08 || addr_hi == 0x09 || addr_hi == 0x0A || addr_hi == 0x0B || addr_hi == 0x0C || addr_hi == 0x0D)
            ) {
                uint8_t vdata;
                if (gdg_whid65040_mem_rd(&sys->gdg, addr, &vdata)) {
                    data = vdata;
                } else {
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
            if (addr_hi == 0x00 && sys->gdg.bank_rom0000) {
                // discard write under ROM
            } else if (addr_hi == 0x01 && sys->gdg.bank_rom1000) {
                // discard write under CGROM
            } else if (addr_hi == 0x0E && sys->gdg.bank_rome000) {
                uint16_t addr_low = addr & 0x0FFF;
                if (addr_low <= 0x08) {
                    if (addr_low == 0x08) {
                        bool prev_gate = sys->gdg.ctc0_gate;
                        gdg_whid65040_write_e008(&sys->gdg, data);
                        if (sys->gdg.ctc0_gate != prev_gate) {
                            i8253_gate(&sys->pit, 0, sys->gdg.ctc0_gate);
                        }
                    } else if (addr_low & 0x04) {
                        i8253_write(&sys->pit, addr_low & 0x03, data);
                        if (sys->pit_write_hook) sys->pit_write_hook(sys->hook_user, sys->cpu.pc, addr_low & 0x03, data);
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
                        /* PA7 active-low resets cursor blink timer */
                        if (!(sys->ppi.pa.outp & 0x80)) sys->cursor_timer = 0;
                        if (sys->ppi_write_hook) sys->ppi_write_hook(sys->hook_user, sys->cpu.pc, addr_low & 3, data);
                    }
                }
            } else if (addr_hi == 0x0F && sys->gdg.bank_rome000) {
                // discard write under ROM
            } else if (
                (addr_hi == 0x08 || addr_hi == 0x09 || addr_hi == 0x0A || addr_hi == 0x0B || addr_hi == 0x0C || addr_hi == 0x0D)
            ) {
                /* Trace VRAM writes */
                static int vwr_cnt = 0;
                if (addr_hi == 0x0C && vwr_cnt < 20) {
                    _bt_log("WR C %04X=%02X dmd=%02X vram=%d PC=%04X tick=%u\n",
                        addr, data, sys->gdg.dmd, sys->gdg.bank_vram, sys->cpu.pc, sys->tick_count);
                    vwr_cnt++;
                }
                if (addr_hi == 0x0D && vwr_cnt < 20) {
                    _bt_log("WR D %04X=%02X dmd=%02X rome=%d PC=%04X tick=%u\n",
                        addr, data, sys->gdg.dmd, sys->gdg.bank_rome000, sys->cpu.pc, sys->tick_count);
                    vwr_cnt++;
                }
                if (!gdg_whid65040_mem_wr(&sys->gdg, addr, data)) {
                    sys->ram[addr] = data;  /* VRAM not mapped — write to RAM */
                }
            } else {
                sys->ram[addr] = data;
            }
        }
    } else if (pins & Z80_IORQ) {
        const uint16_t port_full = Z80_GET_ADDR(pins); // Full 16-bit port address
        const uint16_t port = port_full & 0xFF;
        const uint8_t port_hi = (port_full >> 8) & 0xFF;
        
        if (pins & Z80_WR) {
            if (port >= 0xD4 && port <= 0xD7) {
                /* PIT ports always accessible (both MZ-700 and MZ-800 modes) */
                uint8_t pit_data = Z80_GET_DATA(pins);
                i8253_write(&sys->pit, port, pit_data);
                if (sys->pit_write_hook) sys->pit_write_hook(sys->hook_user, sys->cpu.pc, port & 0x03, pit_data);
            } else if (port >= 0xD0 && port <= 0xD3) {
                /* PPI ports always accessible (both MZ-700 and MZ-800 modes) */
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
                /* PA7 active-low resets cursor blink timer */
                if (!(sys->ppi.pa.outp & 0x80)) sys->cursor_timer = 0;
                if (sys->ppi_write_hook) sys->ppi_write_hook(sys->hook_user, sys->cpu.pc, port & 3, Z80_GET_DATA(pins));
            } else {
                uint8_t data_io = Z80_GET_DATA(pins);
                // Try GDG first (handles CC,CD,CE,CF,E0-E4,F0)
                if (gdg_whid65040_iorq_wr(&sys->gdg, port_full, data_io)) {
                    if (port >= 0xE0 && port <= 0xE4) {
                        _bt_log("OUT %02X=%02X: rom0=%d rom1=%d romE=%d vram=%d PC=%04X tick=%u\n",
                            port, data_io, sys->gdg.bank_rom0000, sys->gdg.bank_rom1000,
                            sys->gdg.bank_rome000, sys->gdg.bank_vram, sys->cpu.pc, sys->tick_count);
                    }
                    if (sys->gdg_write_hook) sys->gdg_write_hook(sys->hook_user, sys->cpu.pc, port_full, data_io);
                } else switch (port) {
                    
                    case 0xF2: // PSG SN76489AN write
                        sn76489an_write(&sys->psg, data_io);
                        if (sys->psg_write_hook) sys->psg_write_hook(sys->hook_user, sys->cpu.pc, data_io);
                        break;
                    // Z80 PIO (0xFC-0xFF) handled by z80pio_tick below

                    // WD2793 FDC (ports D8-DF)
                    case 0xD8: case 0xD9: case 0xDA: case 0xDB:
                    case 0xDC: case 0xDD: case 0xDE: case 0xDF:
                        if (sys->fdc.connected) {
                            wd2793_iorq_wr(&sys->fdc, port, data_io);
                            if (sys->fdc_write_hook) sys->fdc_write_hook(sys->hook_user, sys->cpu.pc, port, data_io);
                        }
                        break;
                    
                    case 0x01: { // CMT hack RHEAD — copy pre-loaded header to Z80 HL
                        if (sys->cmt.has_file) {
                            uint16_t dest = sys->cpu.hl;
                            for (int i = 0; i < CMT_MZF_HEADER_SIZE; i++) {
                                sys->ram[(dest + i) & 0xFFFF] = sys->cmt.header[i];
                            }
                            sys->cpu.a = 0;
                            sys->cpu.f &= ~0x01; // clear carry
                        } else {
                            sys->cpu.a = 2; // BREAK
                            sys->cpu.f |= 0x01; // set carry
                        }
                        break;
                    }
                    case 0x02: { // CMT hack RDATA — copy pre-loaded body to Z80 HL for BC bytes
                        if (sys->cmt.has_file) {
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
            uint8_t data = sys->dbus_latch; // default: last bus value
            // Try GDG first (handles CE status, E0/E1 bank switching)
            uint8_t gdg_data;
            if (gdg_whid65040_iorq_rd(&sys->gdg, port_full, &gdg_data)) {
                data = gdg_data;
                if (port == 0xE0 || port == 0xE1) {
                    _bt_log("IN %02X: rom1=%d vram=%d PC=%04X tick=%u\n",
                        port, sys->gdg.bank_rom1000, sys->gdg.bank_vram, sys->cpu.pc, sys->tick_count);
                }
            } else if (port >= 0xD4 && port <= 0xD7) {
                /* PIT ports always accessible (both MZ-700 and MZ-800 modes) */
                if ((port & 0x03) == 3) {
                    data = sys->dbus_latch; // D7 (CW register) is write-only
                } else {
                    data = i8253_read(&sys->pit, port);
                }
            } else if (port >= 0xD0 && port <= 0xD3) {
                /* PPI ports always accessible (both MZ-700 and MZ-800 modes) */
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
                I8255_SET_PC(ppi_pins, _mz800_ppi_pc_inputs(sys));
                ppi_pins = i8255_tick(&sys->ppi, ppi_pins);
                data = I8255_GET_DATA(ppi_pins);
                if (sys->ppi_write_hook) sys->ppi_write_hook(sys->hook_user, sys->cpu.pc, port & 3, data);
            } else {
                switch (port) {
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
                    // WD2793 FDC (ports D8-DF)
                    case 0xD8: case 0xD9: case 0xDA: case 0xDB:
                    case 0xDC: case 0xDD: case 0xDE: case 0xDF:
                        if (sys->fdc.connected) {
                            uint8_t fdc_data;
                            if (wd2793_iorq_rd(&sys->fdc, port, &fdc_data)) {
                                data = fdc_data;
                            }
                        }
                        break;
                    case 0xFC: case 0xFD: case 0xFE: case 0xFF: // Z80 PIO handled by z80pio_tick below
                        break;
                }
            }
            Z80_SET_DATA(pins, data);
        }
    }

    // --- CPU tick hook for code analysis framework integration ---
    if (sys->cpu_tick_hook) {
        sys->cpu_tick_hook(pins, sys->hook_user);
    }

    // === Timing: advance GDG pixel clocks (cpu_divider per CPU tick) ===
    // The GDG tick handles: hardware signals, VRAM arbitration, display pixel
    // rendering, CTC0/PSG clock division, scanline/frame management, and VBLANK.
    // It returns flags telling us when to clock external chips.
    
    // Feed CPU write signal to GDG before ticking
    sys->gdg.hw_cpu_wr = (pins & Z80_MREQ) && (pins & Z80_WR);

    const uint32_t cpu_div = sys->gdg.vt.cpu_divider;
    for (uint32_t px = 0; px < cpu_div; px++) {
        uint8_t tick_flags = gdg_whid65040_tick(&sys->gdg);

        /* Tick CMT tape every master clock */
        cmt_tick(&sys->cmt);
        
        if (tick_flags & GDG_TICK_CTC0) {
            i8253_tick(&sys->pit, 0);
            /* Update CTC0 audio: CTC0 output gated by PPI PC0 */
            sys->ctc0_audio_out = sys->pit.channels[0].out & (sys->ppi.pc.outp & 0x01);
        }
        if (tick_flags & GDG_TICK_PSG) {
            sn76489an_tick(&sys->psg);
            /* Audio sample decimation: count PSG ticks, emit sample when ready */
            if (sys->audio.sample_period > 0) {
                sys->audio.sample_counter -= _MZ800_AUDIO_FP_SCALE;
                if (sys->audio.sample_counter <= 0) {
                    sys->audio.sample_counter += sys->audio.sample_period;
                    float sample = _mz800_dcadjust(_mz800_psg_sample(&sys->psg, sys->ctc0_audio_out));
                    sys->audio.sample_buffer[sys->audio.sample_pos++] = sample;
                    if (sys->audio.sample_pos >= sys->audio.num_samples) {
                        if (sys->audio.callback.func) {
                            sys->audio.callback.func(sys->audio.sample_buffer, sys->audio.num_samples, sys->audio.callback.user_data);
                        }
                        sys->audio.sample_pos = 0;
                    }
                }
            }
        }
        if (tick_flags & GDG_TICK_NEWLINE) {
            // CTC1 clocked by HSYNC (once per line); CTC2 clocked by CTC1 falling edge
            sys->ctc1_prev_out = sys->pit.channels[1].out;
            i8253_tick(&sys->pit, 1);
            if (sys->ctc1_prev_out && !sys->pit.channels[1].out) {
                i8253_tick(&sys->pit, 2);
            }
        }
        if (tick_flags & GDG_TICK_NEWFRAME) {
            sys->cursor_timer++;
        }
    }

    // VRAM access WAIT — stall CPU during VRAM access (handled by GDG)
    if (sys->gdg.cpu_wait) {
        pins |= Z80_WAIT;
    }

    // CTC2 output drives Z80 INT (masked by PPI PC2)
    uint8_t pc2_mask = (sys->ppi.pc.outp >> 2) & 0x01;
    if (sys->pit.channels[2].out && pc2_mask) {
        pins |= Z80_INT;
    }

    // WD2793 FDC interrupt
    if (sys->fdc.connected && wd2793_check_interrupt(&sys->fdc)) {
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
        if (sys->gdg.vblank_active) pa |= (1 << 5);
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

    // GDG reset (handles bank switching, video timing, VRAM, hardware signals)
    gdg_whid65040_reset(&sys->gdg);

    // PSG reset
    sn76489an_reset(&sys->psg);

    // WD2793 FDC reset
    wd2793_reset(&sys->fdc);

    // CMT tape reset (preserves loaded file)
    cmt_reset(&sys->cmt);

    // Z80 PIO reset (chips library)
    z80pio_reset(&sys->pio);
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

void mz800_sys_setup_audio(mz800_sys_t* sys, chips_audio_callback_t callback, int sample_rate, int num_samples) {
    sys->audio.callback = callback;
    sys->audio.num_samples = (num_samples > 0 && num_samples <= 1024) ? num_samples : 128;
    sys->audio.sample_pos = 0;
    /* PSG clock rate: PAL = 17734475 / 80 = 221680.9 Hz, NTSC = 14318180 / 64 = 223721.6 Hz
       Audio sample period = psg_hz / sample_rate (in fixed-point) */
    int psg_hz = sys->gdg.bPAL ? (17734475 / 80) : (14318180 / 64);
    if (sample_rate <= 0) sample_rate = 44100;
    sys->audio.sample_period = (psg_hz * _MZ800_AUDIO_FP_SCALE) / sample_rate;
    sys->audio.sample_counter = sys->audio.sample_period;
    memset(&_mz800_dcadj, 0, sizeof(_mz800_dcadj));
}

uint32_t mz800_sys_exec(mz800_sys_t* sys, uint32_t micro_seconds) {
    /* Calculate number of CPU ticks to run.
       PAL: 17734475 Hz master / cpu_divider(5) = 3546895 CPU Hz
       NTSC: 14318180 Hz / 4 = 3579545 CPU Hz */
    uint32_t cpu_hz = sys->gdg.bPAL ? (17734475 / 5) : (14318180 / 4);
    uint32_t ticks_to_run = (uint32_t)(((uint64_t)cpu_hz * micro_seconds) / 1000000ULL);
    if (ticks_to_run < 1) ticks_to_run = 1;

    for (uint32_t t = 0; t < ticks_to_run; t++) {
        mz800_sys_tick(sys);
    }
    return ticks_to_run;
}



