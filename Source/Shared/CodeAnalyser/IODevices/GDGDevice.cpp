#include "GDGDevice.h"
#include <Misc/EmuBase.h>
#include <imgui.h>
#include <string.h>
#include <float.h>

#ifndef CHIPS_ASSERT
#include <cassert>
#define CHIPS_ASSERT(c) assert(c)
#endif

/* ============================================================ */
/* Pin table for GDG WHID 65040-032 chip diagram                */
/* ============================================================ */
static const ui_chip_pin_t _ui_gdg_pins[] = {
    { "D0",    0,  GDG_D0 },
    { "D1",    1,  GDG_D1 },
    { "D2",    2,  GDG_D2 },
    { "D3",    3,  GDG_D3 },
    { "D4",    4,  GDG_D4 },
    { "D5",    5,  GDG_D5 },
    { "D6",    6,  GDG_D6 },
    { "D7",    7,  GDG_D7 },
    { "/WAIT", 9,  GDG_WAIT },
    { "/VRAS", 16, GDG_VRAS },
    { "/VCAS", 17, GDG_VCAS },
    { "/VOE",  18, GDG_VOE },
    { "/VRWR", 19, GDG_VRWR },
    { "/HBLN", 20, GDG_HBLN },
};

static void ui_gdg_mz_init(ui_gdg_mz_t* win, gdg_whid65040_t* gdg, const char* title)
{
    memset(win, 0, sizeof(*win));
    win->title = title;
    win->gdg = gdg;
    win->valid = true;
    ui_chip_desc_t chip_desc;
    UI_CHIP_INIT_DESC(&chip_desc, "GDG\nWHID\n65040\n-032", 24, _ui_gdg_pins);
    ui_chip_init(&win->chip, &chip_desc);
}

/* --- Port name helper --- */
static const char* GDGPortName(uint16_t port)
{
    uint8_t lo = port & 0xFF;
    uint8_t hi = (port >> 8) & 0xFF;
    switch (lo) {
        case 0xCC: return "WF";
        case 0xCD: return "RF";
        case 0xCE: return "DMD";
        case 0xCF:
            switch (hi) {
                case 1: return "SOF_LO";
                case 2: return "SOF_HI";
                case 3: return "SW";
                case 4: return "SSA";
                case 5: return "SEA";
                case 6: return "BORDER";
                case 7: return "SUPER";
                default: return "CF?";
            }
        case 0xE0: return "BANK_E0";
        case 0xE1: return "BANK_E1";
        case 0xE2: return "BANK_E2";
        case 0xE3: return "BANK_E3";
        case 0xE4: return "BANK_E4";
        case 0xF0: return "PALETTE";
        default:   return "???";
    }
}

/* --- Decode a write into a human-readable string --- */
static void GDGDecodeWrite(uint16_t port, uint8_t data, char* buf, int buflen)
{
    uint8_t lo = port & 0xFF;
    uint8_t hi = (port >> 8) & 0xFF;
    switch (lo) {
        case 0xCC: { /* WF register */
            static const char* wf_names[] = { "SINGLE","EXOR","OR","RESET","REPLACE","???","PSET","???" };
            uint8_t mode = (data >> 5) & 7;
            uint8_t planes = data & 0x0F;
            uint8_t vbank = (data >> 4) & 1;
            snprintf(buf, buflen, "WF: %s planes=%X vbank=%d", wf_names[mode], planes, vbank);
        } break;
        case 0xCD: { /* RF register */
            uint8_t planes = data & 0x0F;
            bool search = (data >> 7) & 1;
            snprintf(buf, buflen, "RF: %s planes=%X", search ? "SEARCH" : "SINGLE", planes);
        } break;
        case 0xCE: { /* DMD register */
            bool mz700 = (data >> 3) & 1;
            if (mz700) {
                snprintf(buf, buflen, "DMD -> MZ-700 mode");
            } else {
                bool w640 = (data >> 2) & 1;
                bool hicol = (data >> 1) & 1;
                int w = w640 ? 640 : 320;
                int colors = hicol ? (w640 ? 4 : 16) : (w640 ? 2 : 4);
                snprintf(buf, buflen, "DMD -> %dx200 @%d vb=%d", w, colors, data & 1);
            }
        } break;
        case 0xCF: {
            switch (hi) {
                case 1: snprintf(buf, buflen, "SOF lo = %02X", data); break;
                case 2: snprintf(buf, buflen, "SOF hi = %02X", data); break;
                case 3: snprintf(buf, buflen, "SW = %02X", data); break;
                case 4: snprintf(buf, buflen, "SSA = %02X", data); break;
                case 5: snprintf(buf, buflen, "SEA = %02X", data); break;
                case 6: {
                    static const char* igrb[] = {
                        "Black","Blue","Red","Magenta","Green","Cyan","Yellow","White",
                        "DkGray","LtBlue","LtRed","LtMag","LtGreen","LtCyan","LtYellow","BrWhite"
                    };
                    snprintf(buf, buflen, "BORDER = %X (%s)", data & 0xF, igrb[data & 0xF]);
                } break;
                case 7: snprintf(buf, buflen, "Superimpose = %02X", data); break;
                default: snprintf(buf, buflen, "CF? hi=%02X d=%02X", hi, data); break;
            }
        } break;
        case 0xE0: snprintf(buf, buflen, "BANK: ROM 0000-0FFF %s, VRAM %s", (data & 1) ? "ON" : "off", "ON"); break;
        case 0xE1: snprintf(buf, buflen, "BANK: ROM D000-FFFF %s, VRAM %s", "ON", "ON"); break;
        case 0xE2: snprintf(buf, buflen, "BANK: ROM off, VRAM off"); break;
        case 0xE3: snprintf(buf, buflen, "BANK: ROM D000+ ON, VRAM off"); break;
        case 0xE4: snprintf(buf, buflen, "BANK E4: ROM 0000=%s 1000=%s VRAM=%s E000=%s",
                        (data & 1) ? "on" : "OFF",
                        (data & 2) ? "on" : "OFF",
                        (data & 4) ? "on" : "OFF",
                        (data & 8) ? "on" : "OFF"); break;
        case 0xF0: {
            uint8_t palIdx = (data >> 4) & 3;
            uint8_t color = data & 0x0F;
            static const char* igrb[] = {
                "Black","Blue","Red","Magenta","Green","Cyan","Yellow","White",
                "DkGray","LtBlue","LtRed","LtMag","LtGreen","LtCyan","LtYellow","BrWhite"
            };
            snprintf(buf, buflen, "PAL%d = %X (%s)", palIdx, color, igrb[color]);
        } break;
        default: snprintf(buf, buflen, "port %04X = %02X", port, data); break;
    }
}

/* --- WF mode name helper --- */
static const char* WFModeName(uint8_t mode)
{
    switch (mode) {
        case 0: return "SINGLE";
        case 1: return "EXOR";
        case 2: return "OR";
        case 3: return "RESET";
        case 4: return "REPLACE";
        case 6: return "PSET";
        default: return "???";
    }
}

/* --- DMD mode name helper --- */
static const char* DMDModeName(uint8_t dmd)
{
    uint8_t mode = dmd & 0x07;
    bool mz700 = (dmd & 0x08) != 0;
    if (mz700) return "MZ-700";
    switch (mode) {
        case 0: return "320x200 @4/A";
        case 1: return "320x200 @4/B";
        case 2: return "320x200 @16";
        case 3: return "320x200 @16 (undoc)";
        case 4: return "640x200 @2/A";
        case 5: return "640x200 @2/B";
        case 6: return "640x200 @4";
        case 7: return "640x200 @4 (undoc)";
        default: return "???";
    }
}

FGDGDevice::FGDGDevice()
{
    Name = "GDG WHID 65040-032";
    memset(WaitStallValues, 0, sizeof(WaitStallValues));
    memset(ModeValues, 0, sizeof(ModeValues));
}

bool FGDGDevice::Init(const char* pName, FEmuBase* pEmulator, gdg_whid65040_t* pGDG)
{
    Name = pName;
    pGDGState = pGDG;

    SetAnalyser(&pEmulator->GetCodeAnalysis());
    pEmulator->GetCodeAnalysis().IOAnalyser.AddDevice(this);

    memset(WriteBuffer, 0, sizeof(WriteBuffer));

    /* Initialize pin diagram */
    ui_gdg_mz_init(&UIState, pGDG, pName);

    return true;
}

void FGDGDevice::WriteGDG(FAddressRef pc, uint16_t port, uint8_t data)
{
    FGDGWrite& w = WriteBuffer[WriteBufferWriteIndex];
    w.PC = pc;
    w.FrameNo = FrameNo;
    w.Port = port;
    w.Data = data;
    /* Capture beam position at time of write */
    if (pGDGState) {
        w.Line = (uint16_t)pGDGState->line_counter;
        w.Pixel = (uint16_t)pGDGState->pixel_line;
    }
    WriteBufferWriteIndex = (WriteBufferWriteIndex + 1) % kWriteBufferSize;
}

void FGDGDevice::OnFrameTick()
{
}

void FGDGDevice::OnMachineFrameEnd()
{
    if (pGDGState) {
        WaitStallValues[GraphOffset] = (float)pGDGState->vram_wait_stalls;
        ModeValues[GraphOffset] = (float)pGDGState->dmd;
        GraphOffset = (GraphOffset + 1) % kGraphSamples;
    }
    FrameNo++;
}

void FGDGDevice::DrawDetailsUI()
{
    if (pGDGState == nullptr)
        return;

    if (ImGui::BeginTabBar("GDGTabs"))
    {
        if (ImGui::BeginTabItem("Chip"))
        {
            DrawPinDiagramUI();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Registers"))
        {
            DrawRegistersUI();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Timing"))
        {
            DrawTimingUI();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Graphs"))
        {
            DrawGraphsUI();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Palette"))
        {
            DrawPaletteUI();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Banks"))
        {
            DrawBankSwitchUI();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("VRAM"))
        {
            DrawVRAMViewerUI();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Log"))
        {
            DrawWriteLogUI();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

/* ============================================================ */
/* [1] Chip Pin Diagram                                         */
/* ============================================================ */
void FGDGDevice::DrawPinDiagramUI()
{
    CHIPS_ASSERT(UIState.valid && UIState.gdg);
    ImGui::BeginChild("##gdg_chip", ImVec2(176, 0), true);
    ui_chip_draw(&UIState.chip, UIState.gdg->pins);
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##gdg_chipinfo", ImVec2(0, 0), true);
    ImGui::Text("GDG WHID 65040-032");
    ImGui::Text("100-pin QFP custom LSI");
    ImGui::Separator();
    ImGui::Text("D0..D7 = %02X", (uint8_t)(pGDGState->pins & 0xFF));
    ImGui::Text("VOD    = %02X", pGDGState->hw_vod);
    ImGui::Text("/WAIT  = %s", (pGDGState->pins & GDG_WAIT) ? "HIGH" : "LOW");
    ImGui::Text("/VRAS  = %s", pGDGState->hw_vras ? "LOW" : "HIGH");
    ImGui::Text("/HBLN  = %s", pGDGState->hw_hbln ? "LOW" : "HIGH");
    ImGui::EndChild();
}

/* ============================================================ */
/* [2] Registers                                                */
/* ============================================================ */
void FGDGDevice::DrawRegistersUI()
{
    const gdg_whid65040_t* g = pGDGState;

    ImGui::Text("Display Mode: %s", DMDModeName(g->dmd));
    ImGui::Text("DMD Register: %02X  [MZ700:%d SCRW640:%d HICOLOR:%d VBANK:%d]",
        g->dmd,
        (g->dmd >> 3) & 1, (g->dmd >> 2) & 1,
        (g->dmd >> 1) & 1, g->dmd & 1);
    if (g->forbidden_mode)
        ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "FORBIDDEN MODE!");

    ImGui::Separator();
    ImGui::Text("WF: mode=%s  planes=%X  vbank=%d",
        WFModeName(g->wf_mode), g->wf_plane, g->wfrf_vbank);
    ImGui::Text("RF: search=%d  planes=%X",
        g->rf_search, g->rf_plane);

    ImGui::Separator();
    ImGui::Text("Scroll: %s", g->scroll_on ? "ON" : "OFF");
    ImGui::Text("  SOF=%04X  SW=%02X  SSA=%02X  SEA=%02X",
        g->sof, g->sw, g->ssa, g->sea);

    ImGui::Separator();
    ImGui::Text("Border: %X  Superimpose: %s",
        g->border, g->superimpose ? "ON" : "OFF");
    ImGui::Text("Video Standard: %s", g->bPAL ? "PAL (50Hz)" : "NTSC (60Hz)");
}

void FGDGDevice::DrawTimingUI()
{
    gdg_whid65040_t* g = pGDGState;

    ImGui::Checkbox("Debug Visualization", &g->debug_vis);
    if (g->debug_vis)
        ImGui::TextColored(ImVec4(1,1,0,1), "Full raster visible (VBLANK=Red, HBLANK=Blue, Border=Green, WAIT=Magenta)");
    ImGui::Separator();

    ImGui::Text("Line: %3d / %d    Pixel: %4d / %d",
        g->line_counter, g->vt.lines_per_frame,
        g->pixel_line, g->vt.pixels_per_line);
    ImGui::Text("VRAS Phase: %s (%d/8)    CPU Wait: %s",
        g->vras_phase == 0 ? "DISP" : "CPU",
        g->vras_sub,
        g->cpu_wait ? "YES" : "no");
    ImGui::Text("Wait Stalls: %d (this frame)", g->vram_wait_stalls);
    ImGui::Text("Tempo: %d  Divider: %d", g->tempo, g->tempo_divider);

    ImGui::Separator();
    ImGui::Text("Hardware Signals:");
    ImGui::Columns(4, "##signals", false);
    ImGui::TextColored(g->hw_vras ? ImVec4(0,1,0,1) : ImVec4(0.5f,0.5f,0.5f,1), "VRAS"); ImGui::NextColumn();
    ImGui::TextColored(g->hw_vcas ? ImVec4(0,1,0,1) : ImVec4(0.5f,0.5f,0.5f,1), "VCAS"); ImGui::NextColumn();
    ImGui::TextColored(g->hw_voe  ? ImVec4(0,1,0,1) : ImVec4(0.5f,0.5f,0.5f,1), "VOE");  ImGui::NextColumn();
    ImGui::TextColored(g->hw_vrwr ? ImVec4(0,1,0,1) : ImVec4(0.5f,0.5f,0.5f,1), "VRWR"); ImGui::NextColumn();
    ImGui::TextColored(g->hw_hbln ? ImVec4(0,1,0,1) : ImVec4(0.5f,0.5f,0.5f,1), "HBLN"); ImGui::NextColumn();
    ImGui::TextColored(g->hw_cpu_wr ? ImVec4(0,1,0,1) : ImVec4(0.5f,0.5f,0.5f,1), "CPU.WR"); ImGui::NextColumn();
    ImGui::TextColored(g->hw_vram_wr ? ImVec4(0,1,0,1) : ImVec4(0.5f,0.5f,0.5f,1), "VRAM.WR"); ImGui::NextColumn();
    ImGui::TextColored(g->cpu_wait ? ImVec4(1,0.3f,0.3f,1) : ImVec4(0.5f,0.5f,0.5f,1), "/WAIT"); ImGui::NextColumn();
    ImGui::Columns();

    ImGui::Separator();
    ImGui::Text("VBLANK: %s    MZ700 HBLANK: %s",
        g->vblank_active ? "YES" : "no",
        g->mz700_in_hblank ? "YES" : "no");
    ImGui::Text("VOD: %02X", g->hw_vod);
}

void FGDGDevice::DrawPaletteUI()
{
    const gdg_whid65040_t* g = pGDGState;

    /* Full 16-color IGRB reference grid */
    ImGui::Text("IGRB Palette (16 colors):");
    for (int i = 0; i < 16; i++) {
        uint32_t rgba = gdg_whid65040_color(i);
        ImU32 col = IM_COL32((rgba)&0xFF, (rgba>>8)&0xFF, (rgba>>16)&0xFF, 0xFF);
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x+20, p.y+14), col);
        ImGui::Dummy(ImVec2(24, 16));
        if ((i & 7) != 7) ImGui::SameLine();
    }
    ImGui::Separator();

    ImGui::Text("Palette Group: %d", g->palgrp);
    ImGui::Separator();

    /* Standard IGRB 16-color palette table */
    static const ImU32 igrb_colors[16] = {
        IM_COL32(0x00,0x00,0x00,0xFF), IM_COL32(0x40,0x40,0xAC,0xFF),
        IM_COL32(0xD0,0x34,0x00,0xFF), IM_COL32(0xB4,0x0C,0x8C,0xFF),
        IM_COL32(0x40,0x6C,0x00,0xFF), IM_COL32(0x24,0xCC,0xFF,0xFF),
        IM_COL32(0xE8,0xD4,0x30,0xFF), IM_COL32(0xD0,0xD0,0xD0,0xFF),
        IM_COL32(0x84,0x84,0x84,0xFF), IM_COL32(0x00,0x8C,0xE8,0xFF),
        IM_COL32(0xFF,0x00,0x00,0xFF), IM_COL32(0xF0,0x54,0xCC,0xFF),
        IM_COL32(0x54,0xFF,0x54,0xFF), IM_COL32(0x80,0xFF,0xFF,0xFF),
        IM_COL32(0xFF,0xFF,0x28,0xFF), IM_COL32(0xFF,0xFF,0xFF,0xFF)
    };

    for (int i = 0; i < 4; i++) {
        uint8_t idx = g->pal[i];
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + 24, p.y + 16), igrb_colors[idx & 0x0F]);
        ImGui::Dummy(ImVec2(28, 18));
        ImGui::SameLine();
        ImGui::Text("PAL%d = %X (%s%s%s%s)", i, idx,
            (idx & 8) ? "I" : "",
            (idx & 4) ? "G" : "",
            (idx & 2) ? "R" : "",
            (idx & 1) ? "B" : "");
    }

    ImGui::Separator();
    ImGui::Text("Border:");
    {
        uint8_t idx = g->border;
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + 24, p.y + 16), igrb_colors[idx & 0x0F]);
        ImGui::Dummy(ImVec2(28, 18));
        ImGui::SameLine();
        ImGui::Text("= %X", idx);
    }
}

void FGDGDevice::DrawBankSwitchUI()
{
    const gdg_whid65040_t* g = pGDGState;

    ImGui::Text("Memory Bank Configuration:");
    ImGui::Separator();

    auto bankLine = [](const char* range, const char* name, bool active) {
        ImGui::TextColored(
            active ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
            "  %s  %s  %s", range, name, active ? "[MAPPED]" : "[OFF]");
    };

    bankLine("0000-0FFF", "Monitor ROM", g->bank_rom0000);
    bankLine("1000-1FFF", "CGROM",       g->bank_rom1000);
    bankLine("8000-9FFF", "VRAM",        g->bank_vram);
    bankLine("E000-FFFF", "Monitor ROM", g->bank_rome000);

    ImGui::Separator();
    ImGui::Text("CTC0 Gate: %s (regct53g7=%d)",
        g->ctc0_gate ? "HIGH" : "LOW", g->regct53g7);
}

/* ============================================================ */
/* [5] Graphs                                                   */
/* ============================================================ */
void FGDGDevice::DrawGraphsUI()
{
    ImGui::Text("VRAM Wait Stalls (per frame):");
    ImGui::PlotLines("##wait_stalls", WaitStallValues, kGraphSamples, GraphOffset,
        nullptr, 0.0f, FLT_MAX, ImVec2(0, 80));

    ImGui::Separator();
    ImGui::Text("DMD Register (per frame):");
    ImGui::PlotLines("##dmd_mode", ModeValues, kGraphSamples, GraphOffset,
        nullptr, 0.0f, 15.0f, ImVec2(0, 80));

    /* Legend */
    ImGui::TextDisabled("200-frame rolling history  |  Current offset: %d", GraphOffset);
}

/* ============================================================ */
/* [6] VRAM Plane Viewer                                        */
/* ============================================================ */
void FGDGDevice::DrawVRAMViewerUI()
{
    static int selectedPlane = 0;
    static int zoom = 2;
    static int baseAddr = 0;

    ImGui::SliderInt("Plane", &selectedPlane, 0, 3);
    ImGui::SliderInt("Zoom", &zoom, 1, 4);
    ImGui::SliderInt("Base Addr", &baseAddr, 0, 0x1F80, "%04X");

    /* Interpret VRAM plane as a 1bpp 320-pixel-wide image */
    const int bytesPerRow = 40;  /* 320 / 8 */
    const int visRows = 200;

    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize((float)(320 * zoom), (float)(visRows * zoom));
    ImGui::InvisibleButton("##vram_canvas", canvasSize);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const uint8_t* plane = pGDGState->vram[selectedPlane];
    ImU32 colOn  = IM_COL32(0xFF, 0xFF, 0xFF, 0xFF);
    ImU32 colOff = IM_COL32(0x10, 0x10, 0x20, 0xFF);

    for (int row = 0; row < visRows; row++) {
        for (int byteX = 0; byteX < bytesPerRow; byteX++) {
            int addr = (baseAddr + row * bytesPerRow + byteX) & 0x1FFF;
            uint8_t b = plane[addr];
            for (int bit = 0; bit < 8; bit++) {
                bool on = (b >> (7 - bit)) & 1;
                int px = byteX * 8 + bit;
                float x0 = origin.x + (float)(px * zoom);
                float y0 = origin.y + (float)(row * zoom);
                dl->AddRectFilled(
                    ImVec2(x0, y0),
                    ImVec2(x0 + (float)zoom, y0 + (float)zoom),
                    on ? colOn : colOff);
            }
        }
    }

    /* Tooltip showing byte address on hover */
    if (ImGui::IsItemHovered()) {
        ImVec2 mouse = ImGui::GetMousePos();
        int mx = (int)((mouse.x - origin.x) / zoom);
        int my = (int)((mouse.y - origin.y) / zoom);
        if (mx >= 0 && mx < 320 && my >= 0 && my < visRows) {
            int addr = (baseAddr + my * bytesPerRow + mx / 8) & 0x1FFF;
            ImGui::SetTooltip("Plane %d  Addr: %04X  Row: %d  Col: %d\nByte: %02X",
                selectedPlane, addr, my, mx, plane[addr]);
        }
    }
}

/* ============================================================ */
/* [7] Write Log (decoded)                                      */
/* ============================================================ */
void FGDGDevice::DrawWriteLogUI()
{
    ImGui::BeginChild("##gdg_log", ImVec2(0, 0), true);

    ImGui::Columns(6, "##gdg_log_cols");
    ImGui::SetColumnWidth(0, 50);
    ImGui::SetColumnWidth(1, 60);
    ImGui::SetColumnWidth(2, 80);
    ImGui::SetColumnWidth(3, 50);
    ImGui::SetColumnWidth(4, 80);
    ImGui::Text("Frame"); ImGui::NextColumn();
    ImGui::Text("PC");    ImGui::NextColumn();
    ImGui::Text("Beam");  ImGui::NextColumn();
    ImGui::Text("Data");  ImGui::NextColumn();
    ImGui::Text("Port");  ImGui::NextColumn();
    ImGui::Text("Decode"); ImGui::NextColumn();
    ImGui::Separator();

    /* Show last N writes (newest first) */
    for (int i = 0; i < kWriteBufferSize; i++) {
        int idx = (WriteBufferWriteIndex - 1 - i + kWriteBufferSize) % kWriteBufferSize;
        const FGDGWrite& w = WriteBuffer[idx];
        if (w.FrameNo == 0 && i > 0) break; /* empty slot */

        ImGui::Text("%d", w.FrameNo);                ImGui::NextColumn();
        ImGui::Text("%04X", w.PC.Address);            ImGui::NextColumn();
        ImGui::Text("%03d:%03d", w.Line, w.Pixel);   ImGui::NextColumn();
        ImGui::Text("%02X", w.Data);                  ImGui::NextColumn();
        ImGui::Text("%s", GDGPortName(w.Port));       ImGui::NextColumn();

        char decodeBuf[128];
        GDGDecodeWrite(w.Port, w.Data, decodeBuf, sizeof(decodeBuf));
        ImGui::TextUnformatted(decodeBuf);            ImGui::NextColumn();
    }
    ImGui::Columns();
    ImGui::EndChild();
}
