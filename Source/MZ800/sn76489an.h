#pragma once
/*
    sn76489an.h -- SN76489AN PSG (Programmable Sound Generator)

    Header-only emulator for the SN76489AN sound chip,
    following the chips library pattern.

    Do this:
        #define CHIPS_IMPL
    before you include this file in *one* C file to create the implementation.

    ## Functions:
        void sn76489an_init(sn76489an_t* psg)
            Initialize a new SN76489AN instance.

        void sn76489an_reset(sn76489an_t* psg)
            Reset the SN76489AN to power-on state.

        void sn76489an_write(sn76489an_t* psg, uint8_t data)
            Write a data byte (active-low WE).

        void sn76489an_tick(sn76489an_t* psg)
            Advance all channels by one PSG clock.
*/

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SN76489AN_NUM_CHANNELS (4)

/* Per-channel state */
typedef struct {
    uint8_t  type;          /* 0 = tone, 1 = noise */
    uint16_t divider;       /* 10-bit tone period (or noise config) */
    uint16_t latch_div;     /* latched low nibble for 2-byte tone writes */
    uint8_t  attn;          /* 4-bit attenuation (0 = max volume, 15 = off) */
    uint16_t timer;         /* countdown timer */
    uint8_t  output;        /* current output signal (0 or 1) */
    /* noise-specific */
    uint8_t  noise_div_type; /* 0-3 (3 = use channel 2 divider) */
    uint8_t  noise_type;     /* 0 = periodic, 1 = white */
    uint16_t shift_reg;      /* 16-bit LFSR */
} sn76489an_channel_t;

/* Chip state */
typedef struct {
    uint8_t              latch_cs;   /* last latched channel (0-3) */
    uint8_t              latch_attn; /* last latch was attenuation? (0 or 1) */
    sn76489an_channel_t  chn[SN76489AN_NUM_CHANNELS];
} sn76489an_t;

/* Initialize a new SN76489AN instance */
void sn76489an_init(sn76489an_t* psg);
/* Reset to power-on state */
void sn76489an_reset(sn76489an_t* psg);
/* Write a data byte (active-low WE) */
void sn76489an_write(sn76489an_t* psg, uint8_t data);
/* Advance all channels by one PSG clock */
void sn76489an_tick(sn76489an_t* psg);

#ifdef __cplusplus
} /* extern "C" */
#endif

/*-- IMPLEMENTATION ----------------------------------------------------------*/
#ifdef CHIPS_IMPL
#include <string.h>
#ifndef CHIPS_ASSERT
    #include <assert.h>
    #define CHIPS_ASSERT(c) assert(c)
#endif

void sn76489an_init(sn76489an_t* psg) {
    CHIPS_ASSERT(psg);
    memset(psg, 0, sizeof(sn76489an_t));
    sn76489an_reset(psg);
}

void sn76489an_reset(sn76489an_t* psg) {
    CHIPS_ASSERT(psg);
    psg->latch_cs = 0;
    psg->latch_attn = 0;
    for (int i = 0; i < SN76489AN_NUM_CHANNELS; i++) {
        sn76489an_channel_t* ch = &psg->chn[i];
        memset(ch, 0, sizeof(sn76489an_channel_t));
        ch->attn = 15;       /* OFF */
        ch->output = 1;
        ch->type = (i == 3) ? 1 : 0;   /* channel 3 = noise */
    }
    psg->chn[3].shift_reg = 1u << 15;
}

void sn76489an_write(sn76489an_t* psg, uint8_t data) {
    CHIPS_ASSERT(psg);
    int latch = data & 0x80;
    int cs, attn;

    if (latch) {
        cs   = (data >> 5) & 0x03;
        attn = (data >> 4) & 0x01;
        psg->latch_cs   = (uint8_t)cs;
        psg->latch_attn = (uint8_t)attn;
    } else {
        cs   = psg->latch_cs;
        attn = psg->latch_attn;
    }

    sn76489an_channel_t* ch = &psg->chn[cs];

    if (attn) {
        ch->attn = data & 0x0F;
    } else if (latch && ch->type == 0) {
        /* Tone latch: store low nibble */
        ch->latch_div = data & 0x0F;
    } else {
        if (ch->type == 0) {
            /* Tone data: combine high bits with latched low nibble */
            ch->divider = ((uint16_t)(data & 0x3F) << 4) | ch->latch_div;
        } else {
            /* Noise: set div type and noise type */
            ch->noise_div_type = data & 0x03;
            ch->noise_type     = (data >> 2) & 0x01;
        }
    }
}

void sn76489an_tick(sn76489an_t* psg) {
    CHIPS_ASSERT(psg);
    for (int i = 0; i < SN76489AN_NUM_CHANNELS; i++) {
        sn76489an_channel_t* ch = &psg->chn[i];

        if (ch->attn == 15) continue; /* OFF */

        if (ch->type == 0) {
            /* Tone channel */
            if (ch->divider < 2) {
                ch->output = 1;
            } else if (ch->timer == 0) {
                ch->timer = ch->divider - 1;
                ch->output ^= 1;
            } else {
                ch->timer--;
            }
        } else {
            /* Noise channel */
            if (ch->noise_div_type == 3 && psg->chn[2].divider < 2) {
                ch->output = 1;
            } else if (ch->timer == 0) {
                if (ch->noise_div_type == 3) {
                    ch->timer = psg->chn[2].divider - 1;
                } else {
                    ch->timer = (0x10 << ch->noise_div_type) - 1;
                }

                uint8_t bit0 = ch->shift_reg & 0x01;

                if (ch->noise_type == 1) {
                    /* White noise: XOR bit 0 and bit 3 */
                    uint8_t bit3 = (ch->shift_reg >> 3) & 0x01;
                    ch->shift_reg = (ch->shift_reg >> 1) | ((uint16_t)(bit0 ^ bit3) << 15);
                } else {
                    /* Periodic noise */
                    ch->shift_reg = (ch->shift_reg >> 1) | ((uint16_t)bit0 << 15);
                }
                ch->output = ch->shift_reg & 0x01;
            } else {
                ch->timer--;
            }
        }
    }
}

#endif /* CHIPS_IMPL */
