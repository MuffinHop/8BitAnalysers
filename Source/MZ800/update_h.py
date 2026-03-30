with open('/Users/sofiasavilampi/Repositories/8BitAnalysers/Source/MZ800/MZ800ChipsImpl.h', 'r') as f:
    text = f.read()

import re
replacement = '''    uint8_t ram[64 * 1024];
    uint8_t rom[16 * 1024];
    uint8_t vram[4][16 * 1024];
    
    // Memory mapping
    bool rom_0000_on;
    bool rom_1000_on;
    bool rom_e000_on;
    bool vram_on;

    // GDG & VRAM latches
    uint8_t gdg_dmd;
    uint8_t gdg_wf;
    uint8_t gdg_rf;
    uint8_t gdg_palgrp;
'''
text = re.sub(r'    uint8_t ram\[64 \* 1024\];.*?bool vram_on;\n', replacement, text, flags=re.DOTALL)

with open('/Users/sofiasavilampi/Repositories/8BitAnalysers/Source/MZ800/MZ800ChipsImpl.h', 'w') as f:
    f.write(text)

