// mz800_psg.c — SN76489AN PSG implementation
#include "MZ800ChipsImpl.h"

// --- PSG write byte (SN76489AN register protocol) ---
void mz800_psg_write_byte(psg_t* psg, uint8_t value) {
    int latch = value & 0x80;
    int cs, attn;

    if (latch) {
        cs   = (value >> 5) & 0x03;
        attn = (value >> 4) & 0x01;
        psg->latch_cs   = cs;
        psg->latch_attn = attn;
    } else {
        cs   = psg->latch_cs;
        attn = psg->latch_attn;
    }

    psg_channel_t* ch = &psg->ch[cs];

    if (attn) {
        ch->attn = value & 0x0F;
    } else if (latch && ch->type == 0) {
        // Tone latch: store low nibble
        ch->latch_div = value & 0x0F;
    } else {
        if (ch->type == 0) {
            // Tone data: combine high bits with latched low nibble
            ch->divider = ((uint16_t)(value & 0x3F) << 4) | ch->latch_div;
        } else {
            // Noise: set div type and noise type
            ch->noise_div_type = value & 0x03;
            ch->noise_type     = (value >> 2) & 0x01;
        }
    }
}

// --- PSG step: advance all channels by one PSG clock ---
void psg_step(psg_t* psg) {
    for (int cs = 0; cs < PSG_CHANNELS; cs++) {
        psg_channel_t* ch = &psg->ch[cs];

        if (ch->attn == 15) continue; // OFF

        if (ch->type == 0) {
            // Tone channel
            if (ch->divider < 2) {
                ch->output = 1;
            } else if (ch->timer == 0) {
                ch->timer = ch->divider - 1;
                ch->output ^= 1;
            } else {
                ch->timer--;
            }
        } else {
            // Noise channel
            if (ch->noise_div_type == 3 && psg->ch[2].divider < 2) {
                ch->output = 1;
            } else if (ch->timer == 0) {
                if (ch->noise_div_type == 3) {
                    ch->timer = psg->ch[2].divider - 1;
                } else {
                    ch->timer = (0x10 << ch->noise_div_type) - 1;
                }

                uint8_t bit0 = ch->shift_reg & 0x01;

                if (ch->noise_type == 1) {
                    // White noise: XOR bit 0 and bit 3
                    uint8_t bit3 = (ch->shift_reg >> 3) & 0x01;
                    ch->shift_reg = (ch->shift_reg >> 1) | ((uint16_t)(bit0 ^ bit3) << 15);
                } else {
                    // Periodic noise
                    ch->shift_reg = (ch->shift_reg >> 1) | ((uint16_t)bit0 << 15);
                }
                ch->output = ch->shift_reg & 0x01;
            } else {
                ch->timer--;
            }
        }
    }
}
