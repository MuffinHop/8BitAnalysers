// mz800_pioz80.c — Z80 PIO (PIOZ80) at ports 0xFC-0xFF
//
// Address mapping (bit0=port_id, bit1=addr_type):
//   0xFC = CTRL_A,  0xFD = CTRL_B
//   0xFE = DATA_A,  0xFF = DATA_B
//
// Port A input pins:
//   bits 0-1 : 0x03 always high (unused LPT inputs)
//   bit  4   : ~CTC0_output (inverted)
//   bit  5   : VBLN signal from GDG
//   bits 2,3,6,7 : floating/output
// Port B : open bus (reads as 0xFF)

#include "MZ800ChipsImpl.h"
#include <string.h>

// ---- Port raw input ----
static uint8_t pioz80_port_get_raw_input(pioz80_port_t* port, int port_id, mz800_sys_t* sys) {
    if (port_id == 0) { // Port A
        uint8_t pins = 0x03;
        // bit 4: inverted CTC0 output
        if (!sys->pit.channels[0].out) pins |= (1 << 4);
        // bit 5: VBLANK
        if (sys->vblank_active) pins |= (1 << 5);
        return pins;
    }
    return 0xFF; // Port B: all high (pullups via inverters)
}

// ---- iomask input: merge raw pins (input bits) with data_output (output bits) ----
static uint8_t pioz80_port_get_iomask_input(pioz80_port_t* port, int port_id, mz800_sys_t* sys) {
    uint8_t raw = pioz80_port_get_raw_input(port, port_id, sys);
    return (raw & port->io_mask) | (port->data_output & ~port->io_mask);
}

// ---- intmask result: apply iclvl inversion and icmask ----
static uint8_t pioz80_port_get_intmask_result(pioz80_port_t* port, int port_id, mz800_sys_t* sys) {
    uint8_t data = pioz80_port_get_iomask_input(port, port_id, sys);
    uint8_t result = (port->iclvl == 0) ? (uint8_t)~data : data; // LOW: invert, HIGH: as-is
    result &= ~port->icmask; // only monitored bits
    return result;
}

// ---- intfnc result (OR/AND) ----
static uint8_t pioz80_port_get_intfnc_result(pioz80_port_t* port) {
    if (port->icfnc == 0) { // OR
        return (port->masked_input != 0) ? 1 : 0;
    } else { // AND
        uint8_t mask = ~port->icmask;
        return ((port->masked_input & mask) == mask) ? 1 : 0;
    }
}

// ---- Save input state snapshot ----
static void pioz80_port_save_input_state(pioz80_port_t* port, int port_id, mz800_sys_t* sys) {
    port->masked_input = pioz80_port_get_intmask_result(port, port_id, sys);
    port->last_intfnc_result = pioz80_port_get_intfnc_result(port);
}

// ---- Forward declaration ----
static void pioz80_interrupt_manager(pioz80_t* pio, mz800_sys_t* sys);

// ---- Interrupt activate ----
static void pioz80_port_interrupt_activate(pioz80_port_t* port, pioz80_t* pio, mz800_sys_t* sys) {
    if (port->port_int & 0x01) return; // already PENDING

    if (port->port_int == PIOZ80_PORT_INT_NONE) {
        port->port_int = PIOZ80_PORT_INT_PENDING;
    } else {
        port->port_int = PIOZ80_PORT_INT_REPENDING;
    }
    pioz80_interrupt_manager(pio, sys);
}

// ---- Interrupt deactivate (from REPENDING while waiting for RETI) ----
static void pioz80_port_interrupt_deactivate(pioz80_port_t* port) {
    port->port_int = (pioz80_port_int_t)(port->port_int & ~0x01); // clear PENDING bit
}

// ---- Interrupt reset (triggered by ICMSK bit set in ICW) ----
static void pioz80_port_interrupt_reset(pioz80_port_t* port, pioz80_t* pio, mz800_sys_t* sys) {
    port->port_int = (pioz80_port_int_t)(port->port_int & ~0x01); // PENDING→NONE, REPENDING→RECEIVED
    pioz80_interrupt_manager(pio, sys);
}

// ---- Interrupt manager: escalate port-level state to chip-level INT ----
static void pioz80_interrupt_manager(pioz80_t* pio, mz800_sys_t* sys) {
    (void)sys;

    // If already in RECEIVED, only RESET or RETI can free us
    if (pio->interrupt == PIOZ80_INT_RECEIVED) return;

    int port_id_pending = -1;

    for (int pid = 0; pid < 2; pid++) {
        pioz80_port_t* p = &pio->port[pid];
        if (p->port_int == PIOZ80_PORT_INT_RECEIVED) {
            pio->interrupt = PIOZ80_INT_RECEIVED;
            pio->interrupt_port_id = (int8_t)pid;
            return;
        }
        if ((p->port_int == PIOZ80_PORT_INT_PENDING) && (p->icena == 1)) {
            if (port_id_pending == -1) port_id_pending = pid; // Port A has priority
        }
    }

    if (port_id_pending == -1) {
        if (pio->interrupt == PIOZ80_INT_NONE) return;
        pio->interrupt = PIOZ80_INT_NONE;
        return;
    }

    if (pio->interrupt == PIOZ80_INT_PENDING) return; // already pending
    pio->interrupt = PIOZ80_INT_PENDING;
}

// ---- Port event: evaluates masked input change → possibly fires interrupt ----
void pioz80_port_event(pioz80_t* pio, int port_id, mz800_sys_t* sys) {
    pioz80_port_t* port = &pio->port[port_id];

    // If waiting for INTMCW, ignore events
    if (port->ctrl_expect == PIOZ80_EXPECT_INTMCW) return;
    // If already PENDING, ignore
    if (port->port_int == PIOZ80_PORT_INT_PENDING) return;

    uint8_t old_masked = port->masked_input;
    port->masked_input = pioz80_port_get_intmask_result(port, port_id, sys);

    if (old_masked == port->masked_input) return; // no change

    uint8_t last_result = port->last_intfnc_result;
    port->last_intfnc_result = pioz80_port_get_intfnc_result(port);

    if (last_result == port->last_intfnc_result) return; // same result

    if (port->last_intfnc_result == 1) {
        pioz80_port_interrupt_activate(port, pio, sys);
    } else if (port->port_int == PIOZ80_PORT_INT_REPENDING) {
        pioz80_port_interrupt_deactivate(port);
    }
}

// ---- Init ----
void pioz80_init(pioz80_t* pio) {
    memset(pio, 0, sizeof(*pio));
    for (int i = 0; i < 2; i++) {
        pio->port[i].io_mask = 0xFF;     // all inputs
        pio->port[i].icmask = 0xFF;      // all masked
        pio->port[i].mode = 1;           // MODE1_INPUT
        pio->port[i].icena = 0;          // disabled
        pio->port[i].icfnc = 0;          // OR
        pio->port[i].iclvl = 0;          // LOW
    }
    pio->interrupt = PIOZ80_INT_NONE;
    pio->interrupt_port_id = -1;
}

// ---- Read ----
uint8_t pioz80_read(pioz80_t* pio, uint8_t addr, mz800_sys_t* sys) {
    int port_id  = addr & 0x01;       // bit 0 = port A/B
    int is_data  = (addr >> 1) & 0x01; // bit 1 = data/ctrl

    if (is_data) {
        return pioz80_port_get_iomask_input(&pio->port[port_id], port_id, sys);
    }
    return 0xFF; // ctrl read returns 0xFF
}

// ---- Control word dispatch ----

static void pioz80_port_wr_ctrl_ivw(pioz80_port_t* port, uint8_t value) {
    port->interrupt_vector = value & 0xFE;
}

static void pioz80_port_wr_ctrl_mcw(pioz80_port_t* port, uint8_t mode_bits, pioz80_t* pio, mz800_sys_t* sys) {
    int port_id = (port == &pio->port[0]) ? 0 : 1;

    switch (mode_bits) {
    case 0: // OUTPUT
        port->io_mask = 0x00;
        break;
    case 1: // INPUT
        port->io_mask = 0xFF;
        break;
    case 2: // BIDIR (only port A, but behaves like input for MZ-800)
        port->io_mask = 0xFF;
        break;
    case 3: // USER — next byte is IO mask
        port->ctrl_expect = PIOZ80_EXPECT_IOMCW;
        return;
    }

    // Leaving MODE3 with OR function: activate interrupt immediately
    if (port->mode == 3 && port->icfnc == 0) {
        pioz80_port_interrupt_activate(port, pio, sys);
    }
    port->mode = mode_bits;
}

static void pioz80_port_wr_ctrl_icw(pioz80_port_t* port, uint8_t ic_bits, pioz80_t* pio, mz800_sys_t* sys) {
    int port_id = (port == &pio->port[0]) ? 0 : 1;

    port->iclvl = (ic_bits & 0x02) ? 1 : 0; // bit 1
    port->icfnc = (ic_bits & 0x04) ? 1 : 0; // bit 2
    uint8_t icena = (ic_bits & 0x08) ? 1 : 0; // bit 3
    uint8_t icmsk = ic_bits & 0x01; // bit 0

    if (icmsk) {
        // Mask follows in next byte
        port->icena = icena;
        port->ctrl_expect = PIOZ80_EXPECT_INTMCW;
        pioz80_port_interrupt_reset(port, pio, sys);
    } else {
        // No mask follows
        uint8_t old_icena = port->icena;
        port->icena = icena;
        if (port->mode == 3) {
            pioz80_port_event(pio, port_id, sys);
        }
        if (old_icena != icena) {
            pioz80_interrupt_manager(pio, sys);
        }
    }
}

static void pioz80_port_wr_ctrl_idw(pioz80_port_t* port, uint8_t ena, pioz80_t* pio, mz800_sys_t* sys) {
    uint8_t old = port->icena;
    port->icena = ena;
    if (old != ena) pioz80_interrupt_manager(pio, sys);
}

static void pioz80_port_wr_ctrl_intmcw(pioz80_port_t* port, uint8_t value, pioz80_t* pio, mz800_sys_t* sys) {
    int port_id = (port == &pio->port[0]) ? 0 : 1;
    port->icmask = value;
    port->ctrl_expect = PIOZ80_EXPECT_COMMAND;
    // Previous ctrl byte reset interrupt; save starting state now
    pioz80_port_save_input_state(port, port_id, sys);
}

static void pioz80_port_wr_ctrl_iomcw(pioz80_port_t* port, uint8_t value, pioz80_t* pio, mz800_sys_t* sys) {
    int port_id = (port == &pio->port[0]) ? 0 : 1;
    uint8_t old_mode = port->mode;
    port->mode = 3; // MODE3_USER
    port->io_mask = value;
    port->ctrl_expect = PIOZ80_EXPECT_COMMAND;

    if (old_mode == 3) {
        // IO mask changed in mode 3 — may trigger immediate INT
        pioz80_port_event(pio, port_id, sys);
    } else {
        pioz80_port_save_input_state(port, port_id, sys);
    }
}

static void pioz80_port_wr_ctrl(pioz80_port_t* port, uint8_t value, pioz80_t* pio, mz800_sys_t* sys) {
    switch (port->ctrl_expect) {
    case PIOZ80_EXPECT_COMMAND:
        if ((value & 0x01) == 0) {
            // IVW: bit 0 == 0
            pioz80_port_wr_ctrl_ivw(port, value);
        } else {
            uint8_t cmd = value & 0x0F;
            if (cmd == 0x0F) {
                // MCW
                pioz80_port_wr_ctrl_mcw(port, (value >> 6) & 0x03, pio, sys);
            } else if (cmd == 0x07) {
                // ICW
                pioz80_port_wr_ctrl_icw(port, (value >> 4) & 0x0F, pio, sys);
            } else if (cmd == 0x03) {
                // IDW
                pioz80_port_wr_ctrl_idw(port, (value >> 7) & 0x01, pio, sys);
            }
        }
        break;
    case PIOZ80_EXPECT_INTMCW:
        pioz80_port_wr_ctrl_intmcw(port, value, pio, sys);
        break;
    case PIOZ80_EXPECT_IOMCW:
        pioz80_port_wr_ctrl_iomcw(port, value, pio, sys);
        break;
    }
}

// ---- Write ----
void pioz80_write(pioz80_t* pio, uint8_t addr, uint8_t value, mz800_sys_t* sys) {
    int port_id  = addr & 0x01;
    int is_data  = (addr >> 1) & 0x01;

    pioz80_port_t* port = &pio->port[port_id];

    if (is_data) {
        port->data_output = value;
        // Data write may trigger immediate interrupt in mode 3
        pioz80_port_event(pio, port_id, sys);
    } else {
        pioz80_port_wr_ctrl(port, value, pio, sys);
    }
}

// ---- IM2 interrupt acknowledge: returns vector byte ----
uint8_t pioz80_interrupt_ack_im2(pioz80_t* pio, mz800_sys_t* sys) {
    if (pio->interrupt != PIOZ80_INT_PENDING) return 0x00;

    pioz80_port_t* port = NULL;
    int port_id = -1;

    // Port A has priority
    for (int pid = 0; pid < 2; pid++) {
        pioz80_port_t* p = &pio->port[pid];
        if (p->port_int == PIOZ80_PORT_INT_PENDING && p->icena == 1) {
            port = p;
            port_id = pid;
            break;
        }
    }

    if (!port) return 0x00;

    port->port_int = PIOZ80_PORT_INT_RECEIVED;
    pioz80_interrupt_manager(pio, sys);
    pioz80_port_save_input_state(port, port_id, sys);

    return port->interrupt_vector;
}

// ---- RETI callback ----
void pioz80_interrupt_reti(pioz80_t* pio, mz800_sys_t* sys) {
    if (pio->interrupt != PIOZ80_INT_RECEIVED) return;

    pioz80_port_t* port = &pio->port[pio->interrupt_port_id];
    port->port_int = (pioz80_port_int_t)(port->port_int & ~0x02); // clear RECEIVED bit

    pio->interrupt = PIOZ80_INT_NONE;
    pio->interrupt_port_id = -1;

    pioz80_interrupt_manager(pio, sys);
}
