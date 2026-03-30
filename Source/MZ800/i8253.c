#include "i8253.h"

void i8253_init(i8253_t* pit) {
    for (int i = 0; i < 3; i++) {
        i8253_channel_t* ch = &pit->channels[i];
        ch->state = I8253_STATE_INIT_DONE;
        ch->load_done = 0;
        ch->mode = I8253_MODE0;
        ch->out = 0;
        ch->value = 0;
        ch->preset_value = 0xFFFF;
        ch->preset_latch = 0;
        ch->read_latch = 0;
        ch->rl_byte = 0;
        ch->latch_op = 0;
        ch->rlf = I8253_RLF_LSBMSB;
        ch->bcd = 0;
        ch->mode3_half_value = 0;
        ch->mode3_destination_value = 0;
    }
    // MZ-800: CTC0 gate starts LOW (E008 controls it), CTC1/CTC2 gates HIGH
    pit->channels[0].gate = 0;
    pit->channels[1].gate = 1;
    pit->channels[2].gate = 1;
}

void i8253_reset(i8253_t* pit) {
    i8253_init(pit);
}

// --- Output change helper (only fires callback-style if value actually changed) ---
// In our architecture MZ800ChipsImpl.c polls the output, so we just set it.
static inline void i8253_set_out(i8253_channel_t* ch, uint8_t value) {
    ch->out = value;
}

// --- Write ---
void i8253_write(i8253_t* pit, uint16_t port, uint8_t data) {
    uint8_t addr = port & 0x03;

    if (addr == 3) {
        // Control Word Register
        uint8_t cs = data >> 6;
        if (cs == 3) return; // illegal

        i8253_channel_t* ch = &pit->channels[cs];
        uint8_t rlf = (data >> 4) & 0x03;

        ch->rl_byte = 0;

        if (rlf == 0) {
            // Latch command
            ch->latch_op = 1;
            ch->read_latch = ch->value;
            return;
        }

        ch->latch_op = 0;
        ch->rlf = (i8253_rlf_t)rlf;

        i8253_mode_t mode = (i8253_mode_t)((data >> 1) & 0x07);
        if (mode > I8253_MODE5) mode = (i8253_mode_t)(mode - 2); // 6→4, 7→5

        ch->mode = mode;
        ch->bcd = data & 0x01;
        ch->state = I8253_STATE_INIT;
        ch->load_done = 0;

        // Mode 0: output goes LOW on CW write. All others: output goes HIGH.
        i8253_set_out(ch, (ch->mode == I8253_MODE0) ? 0 : 1);
    } else {
        // Counter write (addr 0-2)
        i8253_channel_t* ch = &pit->channels[addr];

        ch->latch_op = 0;

        // Mode 0: if we begin a LOAD while already counting, output goes LOW
        if (ch->mode == I8253_MODE0) {
            if (ch->state > I8253_STATE_INIT_DONE) {
                ch->state = I8253_STATE_LOAD;
                i8253_set_out(ch, 0);
            }
        }

        switch (ch->rlf) {
            case I8253_RLF_LSB:
                ch->preset_latch = data;
                break;
            case I8253_RLF_MSB:
                ch->preset_latch = (uint32_t)data << 8;
                break;
            case I8253_RLF_LSBMSB:
                if (ch->rl_byte == 0) {
                    ch->preset_latch = data;
                    ch->rl_byte = 1;
                    return; // wait for MSB
                } else {
                    ch->preset_latch |= (uint32_t)data << 8;
                    ch->rl_byte = 0;
                }
                break;
        }

        // Convert 0 → 0x10000
        ch->preset_value = (ch->preset_latch == 0) ? 0x10000 : ch->preset_latch;

        // Mode 3: compute half-value
        if (ch->mode == I8253_MODE3) {
            if (ch->preset_value == 1) ch->preset_value = 0x10001;
            ch->mode3_half_value = ch->preset_value;
            if (ch->mode3_half_value & 1) ch->mode3_half_value++;
            ch->mode3_half_value >>= 1;
        }

        // LOAD completion
        if (ch->state < I8253_STATE_LOAD_DONE) {
            if (ch->state == I8253_STATE_INIT) {
                ch->load_done = 1; // deferred until next CLK fall
            } else {
                ch->state = I8253_STATE_LOAD_DONE;
            }
        } else if (ch->state == I8253_STATE_MODE1_TRIGGER_ERR) {
            ch->state = I8253_STATE_PRESET32;
        }
    }
}

// --- Read ---
uint8_t i8253_read(i8253_t* pit, uint16_t port) {
    uint8_t addr = port & 0x03;
    if (addr == 3) return 0xFF; // CW is write-only

    i8253_channel_t* ch = &pit->channels[addr];
    uint32_t v = (ch->latch_op == 1) ? ch->read_latch : ch->value;
    uint8_t res = 0;

    switch (ch->rlf) {
        case I8253_RLF_LSB:
            res = v & 0xFF;
            ch->latch_op = 0;
            break;
        case I8253_RLF_MSB:
            res = (v >> 8) & 0xFF;
            ch->latch_op = 0;
            break;
        case I8253_RLF_LSBMSB:
            if (ch->rl_byte == 0) {
                res = v & 0xFF;
                ch->rl_byte = 1;
            } else {
                res = (v >> 8) & 0xFF;
                ch->rl_byte = 0;
                ch->latch_op = 0;
            }
            break;
    }
    return res;
}

// --- Clock fall (tick) — the main counting engine ---
void i8253_tick(i8253_t* pit, uint8_t channel) {
    i8253_channel_t* ch = &pit->channels[channel];

    switch (ch->mode) {

    case I8253_MODE0:
        if (ch->state >= I8253_STATE_COUNTDOWN) {
            ch->value--;
            if (ch->value == 0x0000) {
                i8253_set_out(ch, 1);
                ch->state = I8253_STATE_BLIND_COUNT;
                ch->value = 0xFFFF;
            }
            return;
        } else if (ch->state == I8253_STATE_LOAD_DONE) {
            ch->value = ch->preset_value;
            ch->state = (ch->gate == 1) ? I8253_STATE_COUNTDOWN : I8253_STATE_WAIT_GATE1;
            return;
        }
        break;

    case I8253_MODE1:
        if (ch->state == I8253_STATE_BLIND_COUNT) {
            ch->value--;
            if (ch->value == 0x0000) ch->value = 0xFFFF;
            return;
        } else if (ch->state == I8253_STATE_COUNTDOWN) {
            ch->value--;
            if (ch->value == 0x0000) {
                i8253_set_out(ch, 1);
                ch->state = (ch->gate == 1) ? I8253_STATE_BLIND_COUNT : I8253_STATE_WAIT_GATE1;
            }
            return;
        } else if (ch->state == I8253_STATE_LOAD_DONE) {
            ch->state = (ch->gate == 1) ? I8253_STATE_BLIND_COUNT : I8253_STATE_WAIT_GATE1;
            return;
        } else if (ch->state == I8253_STATE_PRESET) {
            ch->value = ch->preset_value;
            i8253_set_out(ch, 0);
            ch->state = I8253_STATE_COUNTDOWN;
            return;
        } else if (ch->state == I8253_STATE_PRESET32) {
            ch->value = 32;
            ch->state = I8253_STATE_COUNTDOWN;
            return;
        }
        break;

    case I8253_MODE2:
        if (ch->state == I8253_STATE_COUNTDOWN) {
            ch->value--;
            if (ch->value == 0x0001) {
                i8253_set_out(ch, 0);
                ch->state = I8253_STATE_PRESET;
            }
            return;
        } else if (ch->state == I8253_STATE_PRESET || ch->state == I8253_STATE_LOAD_DONE) {
            i8253_set_out(ch, 1);
            ch->value = ch->preset_value;
            if (ch->value == 0x0001) {
                ch->state = I8253_STATE_PRESET_ERROR;
            } else {
                ch->state = (ch->gate == 1) ? I8253_STATE_COUNTDOWN : I8253_STATE_WAIT_GATE1;
            }
            return;
        }
        break;

    case I8253_MODE3:
        if (ch->state == I8253_STATE_COUNTDOWN) {
            ch->value--;
            if (ch->value == ch->mode3_destination_value) {
                if (ch->out == 1) {
                    i8253_set_out(ch, 0);
                    ch->value = ch->mode3_half_value;
                    ch->mode3_destination_value = 0;
                } else {
                    i8253_set_out(ch, 1);
                    ch->value = ch->preset_value;
                    ch->mode3_destination_value = ch->mode3_half_value;
                    ch->state = (ch->gate == 1) ? I8253_STATE_COUNTDOWN : I8253_STATE_WAIT_GATE1;
                }
            }
            return;
        } else if (ch->state == I8253_STATE_PRESET || ch->state == I8253_STATE_LOAD_DONE) {
            i8253_set_out(ch, 1);
            ch->value = ch->preset_value;
            ch->mode3_destination_value = ch->mode3_half_value;
            ch->state = (ch->gate == 1) ? I8253_STATE_COUNTDOWN : I8253_STATE_WAIT_GATE1;
            return;
        }
        break;

    case I8253_MODE4:
    case I8253_MODE5:
        return; // unsupported
    }

    // Fallthrough: handle INIT → INIT_DONE / LOAD_DONE transition
    if (ch->state == I8253_STATE_INIT) {
        if (ch->load_done == 1) {
            ch->state = I8253_STATE_LOAD_DONE;
            ch->load_done = 0;
        } else {
            ch->state = I8253_STATE_INIT_DONE;
        }
    }
}

// --- Gate change ---
void i8253_gate(i8253_t* pit, uint8_t channel, uint8_t gate_value) {
    i8253_channel_t* ch = &pit->channels[channel];
    gate_value &= 0x01;

    if (ch->gate == gate_value) return; // no change
    ch->gate = gate_value;

    if (ch->state == I8253_STATE_INIT) return;

    switch (ch->mode) {

    case I8253_MODE0:
        if (ch->gate == 0) {
            ch->state = I8253_STATE_WAIT_GATE1;
        } else {
            ch->state = (ch->out == 0) ? I8253_STATE_COUNTDOWN : I8253_STATE_BLIND_COUNT;
        }
        break;

    case I8253_MODE1:
        if (ch->gate == 1) {
            // Rising edge triggers
            if (ch->state == I8253_STATE_LOAD_DONE ||
                ch->state == I8253_STATE_WAIT_GATE1 ||
                ch->state == I8253_STATE_COUNTDOWN) {
                ch->state = I8253_STATE_PRESET;
            } else if (ch->state == I8253_STATE_INIT_DONE) {
                // Trigger before LOAD done
                i8253_set_out(ch, 0);
                ch->state = I8253_STATE_MODE1_TRIGGER_ERR;
            }
        } else if (ch->state == I8253_STATE_BLIND_COUNT) {
            ch->state = I8253_STATE_WAIT_GATE1;
        }
        break;

    case I8253_MODE2:
    case I8253_MODE3:
        if (ch->gate == 0) {
            if (ch->state == I8253_STATE_COUNTDOWN || ch->state == I8253_STATE_PRESET) {
                i8253_set_out(ch, 1);
                ch->state = I8253_STATE_WAIT_GATE1;
            }
        } else if (ch->state == I8253_STATE_WAIT_GATE1 && ch->gate == 1) {
            ch->state = I8253_STATE_PRESET;
        }
        break;

    case I8253_MODE4:
    case I8253_MODE5:
        break; // unsupported
    }
}
