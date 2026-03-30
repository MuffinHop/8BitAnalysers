#include "i8253.h"

void i8253_init(i8253_t* pit) {
    i8253_reset(pit);
    for (int i=0; i<3; i++) {
        pit->channels[i].gate = true; // Most MZ-800 gates are tied high or managed externally
    }
}

void i8253_reset(i8253_t* pit) {
    for (int i=0; i<3; i++) {
        pit->channels[i].initial_value = 0;
        pit->channels[i].value = 0;
        pit->channels[i].mode = 0;
        pit->channels[i].rl = 0;
        pit->channels[i].state = 0;
        pit->channels[i].latched = false;
        pit->channels[i].output = false;
        pit->channels[i].load_done = false;
        pit->channels[i].null_count = true;
    }
}

void i8253_write(i8253_t* pit, uint16_t port, uint8_t data) {
    uint8_t addr = port & 0x03;
    if (addr == 3) {
        // Control Word Register ($D7)
        uint8_t sc = (data >> 6) & 3;
        if (sc == 3) return; // Illegal
        
        i8253_channel_t* ch = &pit->channels[sc];
        uint8_t rl = (data >> 4) & 3;
        
        if (rl == 0) {
            // Latch command
            if (!ch->latched) {
                ch->latch = ch->value;
                ch->latched = true;
            }
        } else {
            ch->rl = rl;
            ch->mode = (data >> 1) & 7;
            if (ch->mode > 5) ch->mode &= 3; // Mode 6/7 mirror 2/3
            ch->bcd = data & 1;
            ch->state = 0; // Reset MSB/LSB flip-flop
            ch->null_count = true;
        }
    } else {
        // Counter 0-2 ($D4-$D6)
        i8253_channel_t* ch = &pit->channels[addr];
        
        if (ch->rl == 1) { // LSB only
            ch->initial_value = (ch->initial_value & 0xFF00) | data;
            ch->value = ch->initial_value;
            ch->load_done = true;
            ch->null_count = false;
        } else if (ch->rl == 2) { // MSB only
            ch->initial_value = (ch->initial_value & 0x00FF) | (data << 8);
            ch->value = ch->initial_value;
            ch->load_done = true;
            ch->null_count = false;
        } else if (ch->rl == 3) { // LSB then MSB
            if (ch->state == 0) {
                ch->initial_value = (ch->initial_value & 0xFF00) | data;
                ch->state = 1;
            } else {
                ch->initial_value = (ch->initial_value & 0x00FF) | (data << 8);
                ch->value = ch->initial_value;
                ch->state = 0;
                ch->load_done = true;
                ch->null_count = false;
            }
        }
    }
}

uint8_t i8253_read(i8253_t* pit, uint16_t port) {
    uint8_t addr = port & 0x03;
    if (addr == 3) return 0xFF; // CW is write-only
    
    i8253_channel_t* ch = &pit->channels[addr];
    uint8_t res = 0;
    
    uint16_t v = ch->latched ? ch->latch : ch->value;
    
    if (ch->rl == 1) { // LSB
        res = v & 0xFF;
        ch->latched = false;
    } else if (ch->rl == 2) { // MSB
        res = v >> 8;
        ch->latched = false;
    } else if (ch->rl == 3) { // LSB then MSB
        if (ch->state == 0) {
            res = v & 0xFF;
            ch->state = 1;
        } else {
            res = v >> 8;
            ch->state = 0;
            ch->latched = false;
        }
    }
    
    return res;
}

void i8253_tick(i8253_t* pit, uint8_t channel) {
    i8253_channel_t* ch = &pit->channels[channel];
    if (!ch->gate || ch->null_count) return;
    
    if (ch->value == 0) {
        if (ch->initial_value == 0) {
            ch->value = 0xFFFF; // 0 means 65536
        } else {
            ch->value = ch->initial_value;
        }
        if (ch->mode == 3) { // Square Wave
            ch->output = !ch->output;
        } else if (ch->mode == 2) { // Rate Generator
            // output pulses low for 1 tick then high
        } else if (ch->mode == 0) {
            ch->output = true;
        }
    } else {
        ch->value--;
        if (ch->value == 0 && ch->mode == 0) {
            ch->output = true;
        }
    }
}
