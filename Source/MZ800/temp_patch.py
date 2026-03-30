import re
with open('/Users/sofiasavilampi/Repositories/8BitAnalysers/Source/MZ800/MZ800ChipsImpl.c', 'r') as f:
    text = f.read()
replacement_read = '''            // Read handling based on memory map
            if (addr < 0x1000 && sys->rom_0000_on) {
                data = sys->rom[addr];
            } else if (addr >= 0x1000 && addr < 0x2000 && sys->rom_1000_on) {
