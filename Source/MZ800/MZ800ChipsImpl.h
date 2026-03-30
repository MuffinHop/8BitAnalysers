#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "chips/z80.h"
#include "chips/i8255.h"

typedef struct {
    z80_t cpu;
    uint64_t pins;
    i8255_t ppi;

    uint32_t tick_count;
    uint8_t ram[64 * 1024];
    uint8_t rom[16 * 1024];
    uint8_t vram[16 * 1024];
    
    bool rom_0000_on;
    bool rom_1000_on;
    bool vram_on;
} mz800_sys_t;

extern mz800_sys_t g_mz800_sys;

void mz800_sys_init(mz800_sys_t* sys);
void mz800_sys_tick(mz800_sys_t* sys);
void mz800_sys_reset(mz800_sys_t* sys);

#ifdef __cplusplus
}
#endif
