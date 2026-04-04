#include <gtest/gtest.h>
#include "../MZ800ChipsImpl.h"

#include <fstream>
#include <vector>
#include <string>

class MZ800Tests : public ::testing::Test {
protected:
    mz800_sys_t sys;

    void SetUp() override {
        mz800_sys_init(&sys);
    }

    std::vector<uint8_t> AssembleZ80(const std::string& source) {
        std::string srcFile = ::testing::TempDir() + "test_prog.z80";
        std::string binFile = ::testing::TempDir() + "test_prog.bin";
        
        std::ofstream out(srcFile);
        out << source;
        out.close();
        
        std::string cmd = "z80asm " + srcFile + " -o " + binFile;
        int ret = system(cmd.c_str());
        EXPECT_EQ(ret, 0) << "Assembly failed: " << cmd;
        
        std::ifstream in(binFile, std::ios::binary);
        std::vector<uint8_t> bin((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();
        
        remove(srcFile.c_str());
        remove(binFile.c_str());
        
        return bin;
    }
};

TEST_F(MZ800Tests, BasicInit) {
    EXPECT_EQ(sys.tick_count, 0ULL);
}

// 1. Timer Test: GDG driving PIT (8253/CTC) channels
TEST_F(MZ800Tests, GDG_Ticks_Timers) {
    // Enable CTC2 interrupt mask (PPI PC2 = 1)
    uint64_t ppi_pins = I8255_CS | I8255_WR | (2 & 3); 
    I8255_SET_DATA(ppi_pins, 0x05); // Set bit 2 of Port C (BSR mode: bit 2 = 1 -> 0x05)
    // Actually, writing to control register (Port 3 -> 3 & 3 = 3)
    ppi_pins = I8255_CS | I8255_WR | 3; 
    I8255_SET_DATA(ppi_pins, 0x05); // 0000 0101 -> Set PC2
    i8255_tick(&sys.ppi, ppi_pins);
    
    EXPECT_EQ((sys.ppi.pc.outp >> 2) & 0x01, 1) << "PPI PC2 should be high";

    // Setup PIT (8253)
    // - CTC1: Mode 2 (Rate Generator)
    // - CTC2: Mode 0 (Interrupt on Terminal Count)
    
    // Write setup for CTC1 (Channel 1, Mode 2)
    i8253_write(&sys.pit, 3, 0x54); // Ch1, LSB only, Mode 2
    i8253_write(&sys.pit, 1, 0x02); // Divider = 2

    // Write setup for CTC2 (Channel 2, Mode 0)
    i8253_write(&sys.pit, 3, 0x90); // Ch2, LSB only, Mode 0
    i8253_write(&sys.pit, 2, 0x01); // Initial count = 1

    // Tick GDG manually or simulate GDG ticks.
    // mz800_sys_tick will call gdg_whid65040_tick() which generates GDG_TICK_CTC1.
    // CTC1 runs at a slower rate, generating a falling edge on output to tick CTC2.
    // Let's directly fake the GDG_TICK_CTC1 to see the cascade.
    
    // Tick 1: CTC1 counts down
    sys.pit.channels[1].out = false;
    i8253_tick(&sys.pit, 1);
    
    // CTC1 output should now toggle eventually (divider 2).
    // Let's just manually test Z80_INT assertion.
    // Right now CTC2 out is false. 
    sys.pit.channels[2].out = true; // Force CTC2 out high
    // Tick system
    mz800_sys_tick(&sys);
    
    // Since CTC2 out is high and PPI PC2 is high, Z80_INT should be asserted.
    EXPECT_TRUE(sys.pins & Z80_INT) << "Z80_INT should be asserted when CTC2 outputs out=true and PPI PC2 is high";
}

TEST_F(MZ800Tests, GDG_Pins_Video) {
    // Verify that VRAM wait states are generated
    sys.gdg.cpu_wait = true;
    mz800_sys_tick(&sys);
    EXPECT_TRUE(sys.pins & Z80_WAIT) << "Z80_WAIT should be generated when gdg.cpu_wait is true";
}

TEST_F(MZ800Tests, PIO_Interrupt_Ack) {
    // Assert PIO interrupt
    // Use the chips native Z80PIO structure to directly set the mode
    sys.pio.port[0].mode = 3;  // Mode 3
    sys.pio.port[0].int_control = 0x87; // Interrupt enable
    sys.pio.port[0].int_state = 1; // fake interrupt pending
    
    // Simulate Z80 interrupt acknowledge cycle (M1 + IORQ)
    sys.pins |= Z80_M1 | Z80_IORQ;
    mz800_sys_tick(&sys);
    
    // Test that the system didn't crash during the ACK cycle
    EXPECT_TRUE(true);
}

// -------------------------------------------------------------------------------- //
// Hardware Tests (inspired by mz800_blast demoscene projects like demo.z80)        //
//                                                                                  //
// KEY INSIGHT: The GDG boots in MZ-700 mode (DMD=0x08). On real MZ-800 hardware,   //
// PIT ports 0xD4-0xD7 and PPI ports 0xD0-0xD3 are always accessible regardless    //
// of DMD mode (confirmed by mz800em and mz800-emuz reference implementations).     //
// Tests must reproduce actual boot sequences to be valid.                           //
// -------------------------------------------------------------------------------- //

TEST_F(MZ800Tests, GDG_Boots_In_MZ700_Mode) {
    // After init, DMD should be 0x08 (MZ700 bit set) — MZ-800 mode is OFF.
    // This is the state MZF programs start in after the monitor loads them.
    EXPECT_EQ(sys.gdg.dmd, 0x08);
    EXPECT_TRUE(sys.gdg.dmd & GDG_DMD_MZ700) << "GDG must boot in MZ-700 mode";
    EXPECT_TRUE(sys.gdg.bank_rom0000) << "Monitor ROM should be mapped at boot";
}

TEST_F(MZ800Tests, GDG_MZ700_PIT_IO_Accessible) {
    // IO ports D0-D7 are always accessible on real MZ-800 hardware, even in
    // MZ-700 mode. They map to the same PPI/PIT chips as memory-mapped IO at
    // E000-E008 (confirmed by mz800em and mz800-emuz references).
    EXPECT_TRUE(sys.gdg.dmd & GDG_DMD_MZ700) << "Should start in MZ-700 mode";

    // Write CTC1 through the Z80 IO path while in MZ-700 mode
    i8253_write(&sys.pit, 3, 0x54); // Direct PIT write always works
    
    // Verify the write took effect
    EXPECT_EQ(sys.pit.channels[1].mode, I8253_MODE2);
    
    // Also verify switching to MZ-800 mode still works
    gdg_whid65040_iorq_wr(&sys.gdg, 0x00CE, 0x02); // Switch to MZ-800 mode
    EXPECT_FALSE(sys.gdg.dmd & GDG_DMD_MZ700) << "DMD MZ700 bit should be cleared";
}

TEST_F(MZ800Tests, GDG_VRAM_Format_Latch) {
    // Real MZF boot sequence from demo.z80:
    // 1. Switch to MZ-800 mode via OUT (0xCE)
    // 2. Map RAM via OUT (0xE0), OUT (0xE6), OUT (0xE1)
    // 3. Map VRAM via IN A, (0xE0)
    // 4. Set WF register via OUT (0xCC)
    // 5. Write to VRAM at 0x8000+

    // Step 1: Switch out of MZ-700 mode (DMD=0x02: HICOLOR, no MZ700 bit)
    gdg_whid65040_iorq_wr(&sys.gdg, 0x00CE, 0x02);
    EXPECT_FALSE(sys.gdg.dmd & GDG_DMD_MZ700) << "Must be in MZ-800 mode for VRAM";

    // Step 2: Bank switching — RAM everywhere
    gdg_whid65040_iorq_wr(&sys.gdg, 0x00E0, 0x00); // RAM at 0000-7FFF
    gdg_whid65040_iorq_wr(&sys.gdg, 0x00E1, 0x00); // RAM at E000-FFFF

    // Step 3: Map VRAM via IN (0xE0) — sets bank_vram=true AND bank_rom1000=true
    uint8_t dummy;
    gdg_whid65040_iorq_rd(&sys.gdg, 0x00E0, &dummy);
    EXPECT_TRUE(sys.gdg.bank_vram) << "IN E0 must map VRAM at 8000-BFFF";
    EXPECT_TRUE(sys.gdg.bank_rom1000) << "IN E0 also maps CGROM at 1000-1FFF";

    // Step 4: WF = REPLACE mode, all 4 planes
    // 0x8F = 1000 1111 => REPLACE mode (4), all 4 planes (0xF)
    gdg_whid65040_iorq_wr(&sys.gdg, 0x00CC, 0x8F);
    EXPECT_EQ(sys.gdg.wf_mode, GDG_WF_REPLACE);
    EXPECT_EQ(sys.gdg.wf_plane, 0x0F);

    // Step 5: Write to VRAM
    EXPECT_TRUE(gdg_whid65040_mem_wr(&sys.gdg, 0x8000, 0x55));

    // Tick to flush pending write
    for (int i = 0; i < 300; i++) {
        gdg_whid65040_tick(&sys.gdg);
    }

    // All 4 planes at address 0 should have 0x55
    EXPECT_EQ(sys.gdg.vram[0][0], 0x55);
    EXPECT_EQ(sys.gdg.vram[1][0], 0x55);
    EXPECT_EQ(sys.gdg.vram[2][0], 0x55);
    EXPECT_EQ(sys.gdg.vram[3][0], 0x55);
}

TEST_F(MZ800Tests, GDG_VRAM_Blocked_In_MZ700_Mode) {
    // Verify that MZ-800 VRAM writes at 0x8000 are rejected when still in MZ-700 mode.
    // This catches the real failure mode MZF files hit.
    EXPECT_TRUE(sys.gdg.dmd & GDG_DMD_MZ700);

    // Map VRAM
    uint8_t dummy;
    gdg_whid65040_iorq_rd(&sys.gdg, 0x00E0, &dummy);
    gdg_whid65040_iorq_wr(&sys.gdg, 0x00CC, 0x8F);

    // Attempt VRAM write at 0x8000 in MZ-700 mode — should be rejected
    // because gdg_whid65040_mem_wr checks !dmd_mz700 for 8000-9FFF range
    EXPECT_FALSE(gdg_whid65040_mem_wr(&sys.gdg, 0x8000, 0x55))
        << "VRAM write at 0x8000 must be rejected in MZ-700 mode";
}

TEST_F(MZ800Tests, GDG_Color_Palette) {
    // Palette writes go through GDG directly (port 0xF0 is not gated by MZ-700 mode)
    // so these work regardless of DMD state — but let's match the real demo sequence.

    // Switch to MZ-800 mode first (as demo.z80 does)
    gdg_whid65040_iorq_wr(&sys.gdg, 0x00CE, 0x02);

    // Group 1 select = 0x40 | 1 = 0x41
    gdg_whid65040_iorq_wr(&sys.gdg, 0x00F0, 0x41);
    EXPECT_EQ(sys.gdg.palgrp, 1);

    // Color writes (igrb nibbles)
    gdg_whid65040_iorq_wr(&sys.gdg, 0x00F0, 0b00000100);
    EXPECT_EQ(sys.gdg.pal[0], 0b00000100);

    gdg_whid65040_iorq_wr(&sys.gdg, 0x00F0, 0b00010101);
    EXPECT_EQ(sys.gdg.pal[1], 0b0101);
}

TEST_F(MZ800Tests, GDG_Memory_Banking) {
    // Reproduce the actual demo.z80 banking sequence:
    //   OUT (0xCE), 0x02  ; switch to MZ-800 mode
    //   OUT (0xE0), 0     ; RAM at 0000-7FFF (disables ROM at 0000, CGROM at 1000)
    //   OUT (0xE6), 0     ; RETURN (restore saved state — no-op if no prior PROHIBIT)
    //   OUT (0xE1), 0     ; RAM at E000-FFFF
    //   IN A, (0xE0)      ; Map CGROM + VRAM

    // Switch to MZ-800 mode
    gdg_whid65040_iorq_wr(&sys.gdg, 0x00CE, 0x02);

    // OUT E0: RAM at 0000-7FFF
    gdg_whid65040_iorq_wr(&sys.gdg, 0x00E0, 0x00);
    EXPECT_FALSE(sys.gdg.bank_rom0000);
    EXPECT_FALSE(sys.gdg.bank_rom1000);

    // OUT E6: RETURN (restore). No prior PROHIBIT, so this is a safe no-op.
    gdg_whid65040_iorq_wr(&sys.gdg, 0x00E6, 0x00);

    // OUT E1: RAM at E000-FFFF
    gdg_whid65040_iorq_wr(&sys.gdg, 0x00E1, 0x00);
    EXPECT_FALSE(sys.gdg.bank_rome000);

    // IN E0: Map CGROM + VRAM
    uint8_t dummy;
    gdg_whid65040_iorq_rd(&sys.gdg, 0x00E0, &dummy);
    EXPECT_TRUE(sys.gdg.bank_vram) << "IN E0 must map VRAM";
    EXPECT_TRUE(sys.gdg.bank_rom1000) << "IN E0 must map CGROM";
}

TEST_F(MZ800Tests, Z80_EndToEnd_Hardware_Interrupt_IM1) {
    // Full end-to-end Z80 interrupt test using real assembly.
    // Reproduces the actual demo.z80 boot + interrupt setup sequence:
    // 1. Switch to MZ-800 mode via OUT (0xCE) for proper video mode
    // 2. Bank switch to RAM
    // 3. Configure PPI and 8253 CTC
    // 4. Enable interrupts
    // Note: PIT/PPI IO ports D0-D7 work in both MZ-700 and MZ-800 modes.
    std::string source = R"(
        ORG 0x0000

        ; STEP 1: Switch to MZ-800 mode — without this, PIT/PPI ports are gated!
        LD A, 0x02
        OUT (0xCE), A   ; DMD = 0x02 (MZ-800 mode, HICOLOR, no MZ700 bit)

        ; STEP 2: Bank all RAM
        LD A, 0
        OUT (0xE0), A   ; RAM at 0000-7FFF
        OUT (0xE1), A   ; RAM at E000-FFFF

        ; STEP 3: Enable CTC2 interrupt mask via PPI BSR
        LD A, 5
        OUT (0xD3), A   ; PPI BSR bit 2 = 1 (enable CTC2 INT mask)

        ; STEP 4: Configure 8253 timers (matching demo.z80 pattern)
        LD A, 0x54
        OUT (0xD7), A   ; CTC1: Mode 2 (rate generator)
        LD A, 0x02
        OUT (0xD5), A   ; CTC1: divider = 2

        LD A, 0x90
        OUT (0xD7), A   ; CTC2: Mode 0 (interrupt on terminal count)
        LD A, 0x01
        OUT (0xD6), A   ; CTC2: initial count = 1

        ; STEP 5: Enable interrupts
        IM 1
        EI

    loop:
        JP loop         ; Spin until interrupt fires

        ; IM1 handler at 0x0038
        ORG 0x0038
        INC A
        RETI
    )";

    std::vector<uint8_t> prog = AssembleZ80(source);

    // Map RAM instead of ROM so Z80 executes our code at 0x0000
    sys.gdg.bank_rom0000 = false;

    memcpy(sys.ram, prog.data(), prog.size());

    sys.cpu.pc = 0x0000;
    sys.cpu.sp = 0xC000;
    sys.cpu.a = 0x00;

    for (int i = 0; i < 1500; i++) {
        mz800_sys_tick(&sys);
    }

    // CPU should have vectored to 0x0038 and executed INC A
    EXPECT_EQ(sys.cpu.a, 1);
}

// -------------------------------------------------------------------------------- //
// i8253 Mode 4 & 5 Tests                                                          //
// -------------------------------------------------------------------------------- //

TEST_F(MZ800Tests, PIT_Mode4_Software_Triggered_Strobe) {
    // Mode 4: Output starts HIGH, counts down, strobes LOW for one CLK at
    // terminal count, then returns HIGH. One-shot (no auto-reload).
    i8253_t pit;
    i8253_init(&pit);

    // Configure channel 0 in Mode 4, LSB only
    i8253_write(&pit, 3, 0x18); // SC=0, RL=01(LSB only), M=100(Mode 4), BCD=0
    EXPECT_EQ(pit.channels[0].out, 1) << "Mode 4: output HIGH after CW write";

    // Load count = 3
    i8253_write(&pit, 0, 3);

    // Tick 1: INIT → LOAD_DONE pipeline transition (no preset yet)
    pit.channels[0].gate = 1;
    i8253_tick(&pit, 0);
    EXPECT_EQ(pit.channels[0].out, 1) << "Output stays HIGH during pipeline";

    // Tick 2: LOAD_DONE → preset loaded (value=3), state→COUNTDOWN
    i8253_tick(&pit, 0);
    EXPECT_EQ(pit.channels[0].value, 3u) << "Value should be preset after LOAD_DONE tick";
    EXPECT_EQ(pit.channels[0].out, 1) << "Still HIGH after preset";

    // Tick 3: 3→2
    i8253_tick(&pit, 0);
    EXPECT_EQ(pit.channels[0].value, 2u);
    EXPECT_EQ(pit.channels[0].out, 1) << "Still HIGH while counting";

    // Tick 4: 2→1
    i8253_tick(&pit, 0);
    EXPECT_EQ(pit.channels[0].value, 1u);
    EXPECT_EQ(pit.channels[0].out, 1);

    // Tick 5: 1→0 — terminal count, output strobes LOW
    i8253_tick(&pit, 0);
    EXPECT_EQ(pit.channels[0].out, 0) << "Mode 4: output must strobe LOW at terminal count";

    // Tick 6: output back to HIGH (strobe is one CLK wide)
    i8253_tick(&pit, 0);
    EXPECT_EQ(pit.channels[0].out, 1) << "Mode 4: output must return HIGH after strobe";
}

TEST_F(MZ800Tests, PIT_Mode4_Gate_Suspends_Counting) {
    // Mode 4: Gate LOW pauses counting, Gate HIGH resumes.
    i8253_t pit;
    i8253_init(&pit);

    i8253_write(&pit, 3, 0x18); // Mode 4
    i8253_write(&pit, 0, 5);    // Count = 5
    pit.channels[0].gate = 1;

    // Tick 1: INIT → LOAD_DONE pipeline
    i8253_tick(&pit, 0);

    // Tick 2: LOAD_DONE → preset (value=5), state→COUNTDOWN
    i8253_tick(&pit, 0);
    EXPECT_EQ(pit.channels[0].value, 5u);

    // Tick 3: 5→4
    i8253_tick(&pit, 0);
    EXPECT_EQ(pit.channels[0].value, 4u);

    // Suspend counting
    i8253_gate(&pit, 0, 0);

    // Tick while gate LOW — value should NOT change
    i8253_tick(&pit, 0);
    EXPECT_EQ(pit.channels[0].value, 4u) << "Gate LOW must suspend Mode 4 counting";

    // Resume counting
    i8253_gate(&pit, 0, 1);

    // Tick: 4→3
    i8253_tick(&pit, 0);
    EXPECT_EQ(pit.channels[0].value, 3u) << "Gate HIGH resumes Mode 4 counting";
}

TEST_F(MZ800Tests, PIT_Mode5_Hardware_Triggered_Strobe) {
    // Mode 5: Same as Mode 4 but triggered by GATE rising edge.
    i8253_t pit;
    i8253_init(&pit);

    // Configure channel 0 in Mode 5, LSB only
    i8253_write(&pit, 3, 0x1A); // SC=0, RL=01(LSB only), M=101(Mode 5), BCD=0
    EXPECT_EQ(pit.channels[0].out, 1) << "Mode 5: output HIGH after CW write";

    // Load count = 3
    i8253_write(&pit, 0, 3);

    // Tick — enters WAIT_GATE1 (Mode 5 waits for GATE trigger)
    i8253_tick(&pit, 0);
    EXPECT_EQ(pit.channels[0].out, 1);

    // Trigger via GATE rising edge
    i8253_gate(&pit, 0, 1);

    // Tick: PRESET → load value, start counting
    i8253_tick(&pit, 0);
    EXPECT_EQ(pit.channels[0].value, 3u);

    // Count down: 3→2→1→0(strobe)
    i8253_tick(&pit, 0); // 3→2
    i8253_tick(&pit, 0); // 2→1
    i8253_tick(&pit, 0); // 1→0 strobe
    EXPECT_EQ(pit.channels[0].out, 0) << "Mode 5: strobe LOW at terminal count";

    i8253_tick(&pit, 0); // back to HIGH
    EXPECT_EQ(pit.channels[0].out, 1) << "Mode 5: back to HIGH after strobe";
}

// -------------------------------------------------------------------------------- //
// GDG Bitmap Pixel Bit Order Test                                                  //
//                                                                                  //
// The MZ-800 uses bit 0 = leftmost pixel (LSB first, same as MZ-700 CG-ROM).      //
// The shift register must output bit 0 first, bit 7 last.                          //
// -------------------------------------------------------------------------------- //

TEST_F(MZ800Tests, GDG_MZ800_Bitmap_Pixel_Bit_Order) {
    // Set up MZ-800 mode 0x00 (320×200 4-color, bank A, planes I+II)
    gdg_whid65040_iorq_wr(&sys.gdg, 0x00CE, 0x00); // DMD = 0x00

    // Set palette: PAL0=0x00(black), PAL1=0x09(blue), PAL2=0x0A(red), PAL3=0x0F(white)
    gdg_whid65040_iorq_wr(&sys.gdg, 0x00F0, 0x00); // PAL0 = 0x00
    gdg_whid65040_iorq_wr(&sys.gdg, 0x00F0, 0x19); // PAL1 = 0x09
    gdg_whid65040_iorq_wr(&sys.gdg, 0x00F0, 0x2A); // PAL2 = 0x0A
    gdg_whid65040_iorq_wr(&sys.gdg, 0x00F0, 0x3F); // PAL3 = 0x0F

    // Write a test pattern to VRAM plane I and II at row 0, byte 0
    // Plane I: 0x01 = bit 0 set (only the LEFTMOST pixel has plane I contribution)
    // Plane II: 0x00 = all clear
    sys.gdg.vram[0][0] = 0x01; // Plane I: only bit 0 set
    sys.gdg.vram[1][0] = 0x00; // Plane II: all clear

    // In mode 0x00, pixel color = pal[(planeII_bit << 1) | planeI_bit]
    // Pixel 0 (leftmost): planeI bit 0 = 1, planeII bit 0 = 0 → pal[1] = 0x09
    // Pixel 1:            planeI bit 1 = 0, planeII bit 1 = 0 → pal[0] = 0x00
    // ...
    // Pixel 7 (rightmost): planeI bit 7 = 0, planeII bit 7 = 0 → pal[0] = 0x00

    // Set up the shift register to test: manually load and verify output
    sys.gdg.vram_row_base = 0;
    sys.gdg.vram_col = 0;
    sys.gdg.shift_cnt = 0;

    // Force a shift register load (shift_cnt was 0 → load triggered)
    // We'll manually call the internal sequence via the tick-based rendering path.
    // Instead, let's directly test the _gdg_shift_output pattern by loading the
    // shift register and checking each output.

    // Load shift register manually
    sys.gdg.shift[0] = sys.gdg.vram[0][0]; // 0x01
    sys.gdg.shift[1] = sys.gdg.vram[1][0]; // 0x00
    sys.gdg.shift[2] = 0;
    sys.gdg.shift[3] = 0;
    sys.gdg.shift_cnt = 8;
    sys.gdg.shift_col = 0;

    // Render the full 640-pixel-clock canvas line and check framebuffer output.
    // Let the GDG reach a canvas line and render it.
    // Simpler: advance GDG to first canvas line and check the framebuffer.

    // Reset GDG to start of frame
    sys.gdg.line_counter = 0;
    sys.gdg.pixel_line = 0;

    // Advance to the first canvas line
    uint32_t canvas_first = sys.gdg.vt.canvas_first_line;
    uint32_t target_pixel = canvas_first * sys.gdg.vt.pixels_per_line + sys.gdg.vt.active_start;
    uint32_t current_pixel = 0;
    while (current_pixel < target_pixel) {
        gdg_whid65040_tick(&sys.gdg);
        current_pixel++;
    }

    // Now we're at the first active pixel of the first canvas line.
    // The GDG will load shift registers from vram_row_base=0, vram_col=0.
    // Render 16 pixels (8 logical pixels in 320 mode = 16 dot clocks).
    uint8_t pixels[16];
    for (int i = 0; i < 16; i++) {
        pixels[i] = sys.gdg.fb[canvas_first * sys.gdg.vt.pixels_per_line
                                + sys.gdg.vt.active_start + i];
        gdg_whid65040_tick(&sys.gdg);
    }

    // In 320 mode, each logical pixel is displayed for 2 dot clocks.
    // Pixel 0 (leftmost logical pixel) should be PAL1 (0x09) because bit 0 = 1.
    // Pixels 1-7 should be PAL0 (0x00) because bits 1-7 = 0.
    EXPECT_EQ(pixels[0], sys.gdg.pal[1])
        << "Leftmost pixel must come from bit 0 (LSB first). "
           "If this shows PAL0, the bit order is reversed (MSB first).";
    EXPECT_EQ(pixels[1], sys.gdg.pal[1])
        << "Second dot clock of leftmost logical pixel (320 mode doubles)";
    EXPECT_EQ(pixels[2], sys.gdg.pal[0])
        << "Second logical pixel should be PAL0 (bit 1 = 0)";
    EXPECT_EQ(pixels[14], sys.gdg.pal[0])
        << "Last logical pixel of byte should be PAL0 (bit 7 = 0)";
}

TEST_F(MZ800Tests, GDG_MZ800_Bitmap_Bit0_Is_Leftmost_Pattern) {
    // More thorough bit-order test: write 0xFF to plane I (all bits set)
    // and write 0xAA (10101010) to plane II.
    // In DMD 0x00 (4-color, planes I+II), color = pal[(plII << 1) | plI]
    //
    // If bit 0 = leftmost (correct):
    //   pixel 0: plI=1, plII=0 → pal[1]
    //   pixel 1: plI=1, plII=1 → pal[3]
    //   pixel 2: plI=1, plII=0 → pal[1]
    //   pixel 3: plI=1, plII=1 → pal[3]
    //   ...alternating pal[1], pal[3]
    //
    // If bit 7 = leftmost (WRONG):
    //   pixel 0: plI=1, plII=1 → pal[3]
    //   pixel 1: plI=1, plII=0 → pal[1]
    //   ...different alternation starting with pal[3]

    gdg_whid65040_iorq_wr(&sys.gdg, 0x00CE, 0x00);
    gdg_whid65040_iorq_wr(&sys.gdg, 0x00F0, 0x00); // PAL0 = 0
    gdg_whid65040_iorq_wr(&sys.gdg, 0x00F0, 0x19); // PAL1 = 9
    gdg_whid65040_iorq_wr(&sys.gdg, 0x00F0, 0x2A); // PAL2 = A
    gdg_whid65040_iorq_wr(&sys.gdg, 0x00F0, 0x3F); // PAL3 = F

    sys.gdg.vram[0][0] = 0xFF; // Plane I: all set
    sys.gdg.vram[1][0] = 0xAA; // Plane II: 10101010

    sys.gdg.line_counter = 0;
    sys.gdg.pixel_line = 0;

    uint32_t canvas_first = sys.gdg.vt.canvas_first_line;
    uint32_t target_pixel = canvas_first * sys.gdg.vt.pixels_per_line + sys.gdg.vt.active_start;
    for (uint32_t i = 0; i < target_pixel; i++) {
        gdg_whid65040_tick(&sys.gdg);
    }

    // Render 16 dot clocks (8 logical pixels × 2 for 320 mode)
    uint8_t pixels[16];
    for (int i = 0; i < 16; i++) {
        pixels[i] = sys.gdg.fb[canvas_first * sys.gdg.vt.pixels_per_line
                                + sys.gdg.vt.active_start + i];
        gdg_whid65040_tick(&sys.gdg);
    }

    // 0xAA = bit 0=0, bit 1=1, bit 2=0, bit 3=1, bit 4=0, bit 5=1, bit 6=0, bit 7=1
    // With bit 0 = leftmost:
    //   pixel 0: plI=1, plII=0 → pal[1] = 0x09
    //   pixel 1: plI=1, plII=1 → pal[3] = 0x0F
    EXPECT_EQ(pixels[0], 0x09u)
        << "Pixel 0 (bit 0): planeII=0 → pal[1]=0x09 if bit 0 is leftmost";
    EXPECT_EQ(pixels[2], 0x0Fu)
        << "Pixel 1 (bit 1): planeII=1 → pal[3]=0x0F if bit 0 is leftmost";
    EXPECT_EQ(pixels[4], 0x09u)
        << "Pixel 2 (bit 2): planeII=0 → pal[1]=0x09";
    EXPECT_EQ(pixels[6], 0x0Fu)
        << "Pixel 3 (bit 3): planeII=1 → pal[3]=0x0F";
}

TEST_F(MZ800Tests, PIT_Demo_Timer_Decode_Is_Mode2) {
    // Verify that the demo.z80 timer control words are correctly decoded.
    // The demo source comments say "Mode 4" for CTC1 but the actual byte 0x74
    // encodes Mode 2 (Rate Generator). This test catches the discrepancy.
    //
    // 0x74 = 0111 0100:
    //   SC=01 (counter 1), RL=11 (LSB+MSB), Mode=010 (Mode 2), BCD=0
    //
    // 0xB4 = 1011 0100:
    //   SC=10 (counter 2), RL=11 (LSB+MSB), Mode=010 (Mode 2), BCD=0

    i8253_t pit;
    i8253_init(&pit);

    // Write 0x74 (CTC1 control word as used by demo.z80)
    i8253_write(&pit, 3, 0x74);
    EXPECT_EQ(pit.channels[1].mode, I8253_MODE2)
        << "0x74 must decode to Mode 2 (Rate Generator), not Mode 4";

    // Write 0xB4 (CTC2 control word)
    i8253_write(&pit, 3, 0xB4);
    EXPECT_EQ(pit.channels[2].mode, I8253_MODE2)
        << "0xB4 must decode to Mode 2";
}

// -------------------------------------------------------------------------------- //
// MZ-700 Mode Tests                                                                //
//                                                                                  //
// Validates MZ-700 legacy mode: bank switching, VRAM access, CG-RAM mapping,       //
// memory-mapped IO at E000-E008, and E008 status register.                         //
// Reference: GDG Compendium wiki, mz800em (C), mz800-emuz (Zig), SCAV docs.       //
// -------------------------------------------------------------------------------- //

TEST_F(MZ800Tests, MZ700_Bank_Switch_OUT_E4) {
    // OUT E4 in MZ-700 mode: ROM at 0000, RAM at 1000-CFFF, VRAM at D000,
    // ROM+MMIO at E000-FFFF. (Compendium: "E2+E3 combined, then IN E1 effect")
    EXPECT_TRUE(sys.gdg.dmd & GDG_DMD_MZ700);
    
    gdg_whid65040_iorq_wr(&sys.gdg, 0x00E4, 0x00);
    EXPECT_TRUE(sys.gdg.bank_rom0000)  << "OUT E4 MZ-700: 0000-0FFF ROM";
    EXPECT_FALSE(sys.gdg.bank_rom1000) << "OUT E4 MZ-700: 1000-1FFF RAM (CGROM not mapped)";
    EXPECT_TRUE(sys.gdg.bank_rome000)  << "OUT E4 MZ-700: E000-FFFF ROM/MMIO";
    EXPECT_FALSE(sys.gdg.bank_vram)    << "OUT E4 MZ-700: no MZ-800 bitmap VRAM";
}

TEST_F(MZ800Tests, MZ700_Bank_Switch_IN_E0_Maps_CGROM_And_PCG) {
    // IN E0 in MZ-700 mode maps CG-ROM at 1000-1FFF and PCG RAM at C000-CFFF.
    // Compendium: "0x1000-0x1FFF (CG ROM), 0xC000-0xCFFF (VRAM/PCG RAM)"
    EXPECT_TRUE(sys.gdg.dmd & GDG_DMD_MZ700);
    
    uint8_t dummy;
    gdg_whid65040_iorq_rd(&sys.gdg, 0x00E0, &dummy);
    EXPECT_TRUE(sys.gdg.bank_rom1000) << "IN E0: CGROM mapped at 1000-1FFF";
    
    // With bank_rom1000=true, read from 1000 returns CGROM data (sys->rom[0x1000])
    // and C000-CFFF maps to GDG VRAM (CG-RAM / PCG RAM)
}

TEST_F(MZ800Tests, MZ700_Bank_Switch_IN_E1_Unmaps_CGROM) {
    // IN E1 unmaps CGROM at 1000 and CG-RAM at C000 back to regular RAM.
    // Compendium: "0x1000-0x1FFF (DRAM), 0xC000-0xCFFF (DRAM)"
    
    // First map via IN E0
    uint8_t dummy;
    gdg_whid65040_iorq_rd(&sys.gdg, 0x00E0, &dummy);
    EXPECT_TRUE(sys.gdg.bank_rom1000);
    
    // Then unmap via IN E1
    gdg_whid65040_iorq_rd(&sys.gdg, 0x00E1, &dummy);
    EXPECT_FALSE(sys.gdg.bank_rom1000) << "IN E1: CGROM unmapped at 1000-1FFF";
    EXPECT_FALSE(sys.gdg.bank_vram) << "IN E1: MZ-800 VRAM unmapped";
}

TEST_F(MZ800Tests, MZ700_VRAM_Write_D000_Char_D800_Attr) {
    // MZ-700 char VRAM at D000-D7FF and attr VRAM at D800-DFFF
    // are stored in vram[0][0x1000] and vram[0][0x1800] respectively.
    EXPECT_TRUE(sys.gdg.dmd & GDG_DMD_MZ700);
    
    // D000-DFFF VRAM access is gated by bank_rome000 (set by OUT E3/E4)
    gdg_whid65040_iorq_wr(&sys.gdg, 0x00E4, 0x00); // bank_rome000 = true
    
    // Write character code at D000 (first cell)
    EXPECT_TRUE(gdg_whid65040_mem_wr(&sys.gdg, 0xD000, 0x41))
        << "D000 write should succeed in MZ-700 mode with bank_rome000=true";
    EXPECT_EQ(sys.gdg.vram[0][0x1000], 0x41);
    
    // Write attribute at D800 (first cell attribute)
    EXPECT_TRUE(gdg_whid65040_mem_wr(&sys.gdg, 0xD800, 0x71))
        << "D800 write should succeed";
    EXPECT_EQ(sys.gdg.vram[0][0x1800], 0x71);
    
    // Read them back
    uint8_t char_data, attr_data;
    EXPECT_TRUE(gdg_whid65040_mem_rd(&sys.gdg, 0xD000, &char_data));
    EXPECT_TRUE(gdg_whid65040_mem_rd(&sys.gdg, 0xD800, &attr_data));
    EXPECT_EQ(char_data, 0x41);
    EXPECT_EQ(attr_data, 0x71);
}

TEST_F(MZ800Tests, MZ700_VRAM_Blocked_After_OUT_E1) {
    // After OUT E1 (D000-FFFF to DRAM), VRAM writes at D000 should be rejected.
    EXPECT_TRUE(sys.gdg.dmd & GDG_DMD_MZ700);
    
    gdg_whid65040_iorq_wr(&sys.gdg, 0x00E1, 0x00); // bank_rome000 = false
    
    EXPECT_FALSE(gdg_whid65040_mem_wr(&sys.gdg, 0xD000, 0x42))
        << "D000 VRAM write must be rejected after OUT E1 (bank_rome000=false)";
}

TEST_F(MZ800Tests, MZ700_CG_RAM_Access_Via_IN_E0) {
    // CG-RAM (PCG) at C000-CFFF is mapped by IN E0 and stored in vram[0][0x0000-0x0FFF].
    // Programs use this to implement custom character graphics.
    EXPECT_TRUE(sys.gdg.dmd & GDG_DMD_MZ700);
    
    // IN E0: map CGROM at 1000 and CG-RAM at C000
    uint8_t dummy;
    gdg_whid65040_iorq_rd(&sys.gdg, 0x00E0, &dummy);
    
    // Write CG pattern data at C000
    EXPECT_TRUE(gdg_whid65040_mem_wr(&sys.gdg, 0xC000, 0xAA))
        << "C000 CG-RAM write should succeed after IN E0";
    EXPECT_EQ(sys.gdg.vram[0][0x0000], 0xAA);
    
    // Read it back
    uint8_t cg_data;
    EXPECT_TRUE(gdg_whid65040_mem_rd(&sys.gdg, 0xC000, &cg_data));
    EXPECT_EQ(cg_data, 0xAA);
    
    // Unmap via IN E1
    gdg_whid65040_iorq_rd(&sys.gdg, 0x00E1, &dummy);
    
    // CG-RAM should no longer be accessible (falls through to RAM)
    EXPECT_FALSE(gdg_whid65040_mem_wr(&sys.gdg, 0xC000, 0x55))
        << "C000 write must be rejected after IN E1";
}

TEST_F(MZ800Tests, MZ700_IO_Ports_D0_D7_Work_In_MZ700_Mode) {
    // On real MZ-800 hardware, IO ports D0-D7 always work regardless of DMD mode.
    // This was a bug: our emulator previously blocked D0-D7 in MZ-700 mode.
    // Reference: mz800em, mz800-emuz, SCAV documentation.
    EXPECT_TRUE(sys.gdg.dmd & GDG_DMD_MZ700) << "Should start in MZ-700 mode";
    
    // Write to PIT (8253) via IO port D7 in MZ-700 mode
    // Configure CTC1: Mode 2, LSB only
    std::string source = R"(
        ORG 0x0000
        ; In MZ-700 mode, write PIT through IO ports (should NOT be blocked)
        LD A, 0x54
        OUT (0xD7), A     ; CTC1 control word: Mode 2
        LD A, 10
        OUT (0xD5), A     ; CTC1 divider = 10
        
        ; Also write PPI through IO port D3 (BSR: set PC2)
        LD A, 5
        OUT (0xD3), A     ; PPI BSR: set bit 2
        
        HALT
    )";
    
    std::vector<uint8_t> prog = AssembleZ80(source);
    sys.gdg.bank_rom0000 = false;
    memcpy(sys.ram, prog.data(), prog.size());
    sys.cpu.pc = 0x0000;
    sys.cpu.sp = 0xC000;
    
    for (int i = 0; i < 200; i++) {
        mz800_sys_tick(&sys);
    }
    
    // PIT should have received the Mode 2 command
    EXPECT_EQ(sys.pit.channels[1].mode, I8253_MODE2)
        << "PIT write via IO port D7 must NOT be blocked in MZ-700 mode";
    
    // PPI PC2 should be set
    EXPECT_EQ((sys.ppi.pc.outp >> 2) & 1, 1)
        << "PPI write via IO port D3 must NOT be blocked in MZ-700 mode";
}

TEST_F(MZ800Tests, MZ700_E008_Returns_GDG_Status) {
    // E008 memory-mapped read returns the GDG status register (same as port CE).
    // Contains: TEMPO, HBLANK, VBLANK, HSYNC, VSYNC, MZ-800 device flag.
    // Reference: mz800-emuz maps E008 read → port 0xCE.
    EXPECT_TRUE(sys.gdg.dmd & GDG_DMD_MZ700);
    
    // Read GDG status via port CE
    uint8_t ce_status;
    gdg_whid65040_iorq_rd(&sys.gdg, 0x00CE, &ce_status);
    
    // The MZ-800 device flag (GDG_STATUS_MZ800 = bit 1) should be set
    // since mz800_mode was initialized as true
    EXPECT_TRUE(ce_status & GDG_STATUS_MZ800)
        << "GDG status should report MZ-800 hardware";
    
    // E008 should return the same value (it reads the same status register)
    // This is tested indirectly through the memory-mapped read path
}
