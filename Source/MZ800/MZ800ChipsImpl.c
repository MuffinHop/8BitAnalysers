#define CHIPS_IMPL
#include "MZ800ChipsImpl.h"

#define CHIPS_UTIL_IMPL
#include "util/z80dasm.h"
#include "util/m6502dasm.h"

mz800_sys_t g_mz800_sys;

void mz800_sys_init(mz800_sys_t* sys)
{
    sys->pins = z80_init(&sys->cpu);
    sys->tick_count = 0;
    
    // Initial memory state
    sys->rom_0000_on = true;
    sys->rom_1000_on = true;
    sys->vram_on = false;
}

void mz800_sys_tick(mz800_sys_t* sys)
{
    uint64_t pins = z80_tick(&sys->cpu, sys->pins);

    if (pins & Z80_MREQ) {
        const uint16_t addr = Z80_GET_ADDR(pins);
        if (pins & Z80_RD) {
            uint8_t data = 0xFF;
            
            // Read handling based on memory map
            if (addr < 0x1000 && sys->rom_0000_on) {
                data = sys->rom[addr]; // 0000-0FFF Monitor ROM
            } else if (addr >= 0x1000 && addr < 0x2000 && sys->rom_1000_on) {
                data = sys->rom[addr]; // 1000-1FFF CGROM (starts at 1000 in our buffer)
            } else if (addr >= 0x8000 && addr < 0xC000 && sys->vram_on) {
                data = sys->vram[addr & 0x3FFF]; // VRAM bank
            } else {
                data = sys->ram[addr];
            }
            
            Z80_SET_DATA(pins, data);
        } else if (pins & Z80_WR) {
            uint8_t data = Z80_GET_DATA(pins);
            
            // Write handling
            if (addr >= 0x8000 && addr < 0xC000 && sys->vram_on) {
                sys->vram[addr & 0x3FFF] = data;
            } else {
                // By default writes always go to RAM even if ROM is mapped over it
                // (Though some MZ-800 hardware might have restrictions, simple emulation maps RAM underneath).
                sys->ram[addr] = data;
            }
        }
    } else if (pins & Z80_IORQ) {
        const uint16_t port = Z80_GET_ADDR(pins) & 0xFF; // typically lower 8 bits used
        
        if (pins & Z80_WR) {
            switch (port) {
                case 0xE0: // memory_mmap_rom_bottom_off
                    sys->rom_0000_on = false;
                    break;
                case 0xE1: // memory_mmap_rom_upper_off
                    sys->rom_1000_on = false;
                    break;
                case 0xE2: // memory_mmap_rom_0000_on
                    sys->rom_0000_on = true;
                    break;
                case 0xE3: // memory_mmap_rom_upper_on
                    sys->rom_1000_on = true;
                    break;
                case 0xE4: // memory_mmap_all_on
                    sys->rom_0000_on = true;
                    sys->rom_1000_on = true;
                    break;
            }
        } else if (pins & Z80_RD) {
            uint8_t data = 0xFF; // default open bus
            switch (port) {
                case 0xE0: // IN $E0 -> VRAM ON
                    sys->vram_on = true;
                    break;
                case 0xE1: // IN $E1 -> VRAM OFF
                    sys->vram_on = false;
                    break;
            }
            Z80_SET_DATA(pins, data);
        }
    }

    sys->pins = pins;
    sys->tick_count++;
}

void mz800_sys_reset(mz800_sys_t* sys)
{
    sys->pins = z80_reset(&sys->cpu);
    sys->rom_0000_on = true;
    sys->rom_1000_on = true;
    sys->vram_on = false;
}
