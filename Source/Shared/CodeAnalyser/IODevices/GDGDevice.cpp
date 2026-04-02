#include "GDGDevice.h"
#include <Misc/EmuBase.h>
#include <imgui.h>

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
}

bool FGDGDevice::Init(const char* pName, FEmuBase* pEmulator, gdg_whid65040_t* pGDG)
{
    Name = pName;
    pGDGState = pGDG;

    SetAnalyser(&pEmulator->GetCodeAnalysis());
    pEmulator->GetCodeAnalysis().IOAnalyser.AddDevice(this);

    memset(WriteBuffer, 0, sizeof(WriteBuffer));
    return true;
}

void FGDGDevice::WriteGDG(FAddressRef pc, uint16_t port, uint8_t data)
{
    FGDGWrite& w = WriteBuffer[WriteBufferWriteIndex];
    w.PC = pc;
    w.FrameNo = FrameNo;
    w.Port = port;
    w.Data = data;
    WriteBufferWriteIndex = (WriteBufferWriteIndex + 1) % kWriteBufferSize;
}

void FGDGDevice::OnFrameTick()
{
}

void FGDGDevice::OnMachineFrameEnd()
{
    FrameNo++;
}

void FGDGDevice::DrawDetailsUI()
{
    if (pGDGState == nullptr)
        return;

    if (ImGui::BeginTabBar("GDGTabs"))
    {
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
        if (ImGui::BeginTabItem("Log"))
        {
            DrawWriteLogUI();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

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

void FGDGDevice::DrawWriteLogUI()
{
    ImGui::BeginChild("##gdg_log", ImVec2(0, 0), true);

    ImGui::Columns(5, "##gdg_log_cols");
    ImGui::SetColumnWidth(0, 50);
    ImGui::SetColumnWidth(1, 60);
    ImGui::SetColumnWidth(2, 80);
    ImGui::SetColumnWidth(3, 50);
    ImGui::Text("Frame"); ImGui::NextColumn();
    ImGui::Text("PC");    ImGui::NextColumn();
    ImGui::Text("Port");  ImGui::NextColumn();
    ImGui::Text("Data");  ImGui::NextColumn();
    ImGui::Text("Name");  ImGui::NextColumn();
    ImGui::Separator();

    /* Show last N writes (newest first) */
    for (int i = 0; i < kWriteBufferSize; i++) {
        int idx = (WriteBufferWriteIndex - 1 - i + kWriteBufferSize) % kWriteBufferSize;
        const FGDGWrite& w = WriteBuffer[idx];
        if (w.FrameNo == 0 && i > 0) break; /* empty slot */

        ImGui::Text("%d", w.FrameNo);       ImGui::NextColumn();
        ImGui::Text("%04X", w.PC.Address);   ImGui::NextColumn();
        ImGui::Text("%04X", w.Port);         ImGui::NextColumn();
        ImGui::Text("%02X", w.Data);         ImGui::NextColumn();
        ImGui::Text("%s", GDGPortName(w.Port)); ImGui::NextColumn();
    }
    ImGui::Columns();
    ImGui::EndChild();
}
