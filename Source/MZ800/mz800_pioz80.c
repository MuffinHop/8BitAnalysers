// mz800_pioz80.c — Z80 PIO (PIOZ80) at ports 0xFC-0xFF
//
// Port A input pins:
//   bits 0-1  : 0x03 always high (unused inputs)
//   bit  4    : ~CTC0_output (inverted CTC0 square wave)
//   bit  5    : VBLN signal from GDG (vertical blank)
//   bits 2,3,6,7 : floating / output (driven by data_output latch if configured as outputs)
// Port B : general-purpose (open bus, reads as 0xFF)
//
// Register layout (addr = port & 0x03):
//   0x00 (0xFC) : Port A data
//   0x01 (0xFD) : Port B data
//   0x02 (0xFE) : Port A control
//   0x03 (0xFF) : Port B control / interrupt vector

#include "MZ800ChipsImpl.h"

void pioz80_init(pioz80_t* pio) {
    pio->port_a_out  = 0x00;
    pio->port_b_out  = 0x00;
    pio->io_mask_a   = 0xFF; // all inputs by default
    pio->io_mask_b   = 0xFF;
    pio->int_vector  = 0x00;
    pio->ctrl_word_a = 0x4F; // mode 0 input
    pio->ctrl_word_b = 0x4F;
    pio->int_pending = false;
}

// Build port A byte from live hardware signals + output latch.
// Input pins override output latch in bit positions where io_mask says 1 (input).
static uint8_t pioz80_read_port_a(pioz80_t* pio, mz800_sys_t* sys) {
    // Assemble the raw hardware input for port A
    uint8_t pins = 0x03;  // bits 0-1 always high
    // bit 4: CTC0 output inverted
    if (!sys->pit.channels[0].output) pins |= (1 << 4);
    // bit 5: vertical blank signal
    if (sys->vblank_active) pins |= (1 << 5);
    // Merge: input pins where io_mask=1, output latch where io_mask=0
    return (pins & pio->io_mask_a) | (pio->port_a_out & ~pio->io_mask_a);
}

uint8_t pioz80_read(pioz80_t* pio, uint8_t addr, mz800_sys_t* sys) {
    switch (addr & 0x03) {
        case 0x00: return pioz80_read_port_a(pio, sys);
        case 0x01: return 0xFF; // Port B open bus
        case 0x02: return pio->ctrl_word_a;
        case 0x03: return pio->ctrl_word_b;
        default:   return 0xFF;
    }
}

void pioz80_write(pioz80_t* pio, uint8_t addr, uint8_t value) {
    switch (addr & 0x03) {
        case 0x00:
            pio->port_a_out = value;
            break;
        case 0x01:
            pio->port_b_out = value;
            break;
        case 0x02:
            pio->ctrl_word_a = value;
            if (value & 0x01) {
                // Interrupt control word
                // bit 7: interrupt enable
            } else if ((value & 0x0F) == 0x0F) {
                // IO mask follows
                pio->io_mask_a = value; // simplified: next write sets io_mask
            }
            // If bit 7=0 and bit 3=1 and bit 0=0: interrupt vector
            if (!(value & 0x01) && (value & 0x08) == 0) {
                // mode word: bits 7-6 select mode. Simplified: just store.
            }
            break;
        case 0x03:
            pio->ctrl_word_b = value;
            // Interrupt vector (mode 2): lower bit 0 must be 0
            if (!(value & 0x01)) {
                pio->int_vector = value;
            }
            break;
    }
}
