// mz800_cmt.c — CMT (tape) hack: ROM patch installation and MZF file loading
#include "MZ800ChipsImpl.h"
#include <string.h>

// --- Install ROM patches so RHEAD/RDATA calls trigger OUT 0x01/0x02 ---
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
    sys->cmt.body      = (uint8_t*)(data + MZF_HEADER_SIZE);
    sys->cmt.body_size = size - MZF_HEADER_SIZE;
    sys->cmt.has_file  = true;
    sys->cmt.header_loaded = false;

    // Patch ROM so monitor RHEAD/RDATA stubs route through our IO intercept
    cmthack_install_rom_patches(sys);

    return true;
}
