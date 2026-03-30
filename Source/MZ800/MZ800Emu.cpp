#include "MZ800Emu.h"
#include "MZ800ChipsImpl.h"
#include "MZ800Config.h"
#include <imGui/imgui.h>

extern void SetWindowTitle(const char* pTitle);

int MZ800KeyFromImGuiKey(ImGuiKey key) {
    int mzKey = 0;
    if (key >= ImGuiKey_0 && key <= ImGuiKey_9) mzKey = '0' + (key - ImGuiKey_0);
    else if (key >= ImGuiKey_A && key <= ImGuiKey_Z) mzKey = 'A' + (key - ImGuiKey_A);
    else if (key == ImGuiKey_Space) mzKey = 32;
    else if (key == ImGuiKey_Enter) mzKey = 13;
    else if (key == ImGuiKey_Tab) mzKey = 9;
    else if (key == ImGuiKey_Escape) mzKey = 27;
    else if (key == ImGuiKey_LeftCtrl) mzKey = 140;
    else if (key == ImGuiKey_RightCtrl) mzKey = 140;
    else if (key == ImGuiKey_LeftShift) mzKey = 141;
    else if (key == ImGuiKey_RightShift) mzKey = 141;
    else if (key == ImGuiKey_Insert) mzKey = 134;
    else if (key == ImGuiKey_Delete) mzKey = 135;
    else if (key == ImGuiKey_Backspace) mzKey = 135;
    else if (key == ImGuiKey_UpArrow) mzKey = 136;
    else if (key == ImGuiKey_DownArrow) mzKey = 137;
    else if (key == ImGuiKey_RightArrow) mzKey = 138;
    else if (key == ImGuiKey_LeftArrow) mzKey = 139;
    else if (key == ImGuiKey_F1) mzKey = 142;
    else if (key == ImGuiKey_F2) mzKey = 143;
    else if (key == ImGuiKey_F3) mzKey = 144;
    else if (key == ImGuiKey_F4) mzKey = 145;
    else if (key == ImGuiKey_F5) mzKey = 146;
    else if (key == ImGuiKey_Semicolon) mzKey = ';';
    else if (key == ImGuiKey_Apostrophe) mzKey = ':';
    else if (key == ImGuiKey_Comma) mzKey = ',';
    else if (key == ImGuiKey_Period) mzKey = '.';
    else if (key == ImGuiKey_Slash) mzKey = '/';
    else if (key == ImGuiKey_Backslash) mzKey = '\\';
    else if (key == ImGuiKey_Minus) mzKey = '-';
    else if (key == ImGuiKey_Equal) mzKey = '+';
    else if (key == ImGuiKey_LeftBracket) mzKey = '[';
    else if (key == ImGuiKey_RightBracket) mzKey = ']';
    else if (key == ImGuiKey_GraveAccent) mzKey = '~';
    return mzKey;
}

bool FMZ800Emu::Init(const FEmulatorLaunchConfig& launchConfig)
{
    printf("MZ800 Init called!\n");
    if (!FEmuBase::Init(launchConfig)) {
        printf("FEmuBase::Init failed!\n");
        return false;
    }
    printf("FEmuBase::Init succeeded!\n");
    
    // Create Global Config
    pGlobalConfig = new FMZ800GlobalConfig();
    pGlobalConfig->Init(); pGlobalConfig->bBuiltInFont = true;
    CodeAnalysis.SetGlobalConfig(pGlobalConfig);
    
    SetWindowTitle("MZ-800 Analyser");
    
    mz800_sys_init(&g_mz800_sys);

    LoadFont();
    
    CPUType = ECPUType::Z80;
    
    // Initialize standard memory
    MainRAMBankId = CodeAnalysis.CreateBank("MainRAM", 64, g_mz800_sys.ram, false, 0x0000, true);
    CodeAnalysis.MapBank(MainRAMBankId, 0, EBankAccess::ReadWrite);
    CodeAnalysis.Init(this);
    CodeAnalysis.ViewState[0].Enabled = true;

    printf("FMZ800Emu::Init almost finished returning true\n");
    return true;
}

void FMZ800Emu::Shutdown()
{
    FEmuBase::Shutdown();
    delete pGlobalConfig;
}

void FMZ800Emu::Tick()
{
    FEmuBase::Tick();
    mz800_sys_tick(&g_mz800_sys);
    
    // Update Floooh keyboard matrix
    const uint32_t frame_time_us = static_cast<FMZ800GlobalConfig*>(pGlobalConfig)->bPAL ? 20000 : 16667;
    kbd_update(&g_mz800_sys.kbd, frame_time_us); 

    // Draw UI
    DrawDockingView();
}

void FMZ800Emu::Reset()
{
    FEmuBase::Reset();
    mz800_sys_reset(&g_mz800_sys);
}

bool FMZ800Emu::LoadEmulatorFile(const FEmulatorFile* pSnapshot)
{
    return false;
}

bool FMZ800Emu::NewProjectFromEmulatorFile(const FEmulatorFile& gameSnapshot)
{
    return false;
}

bool FMZ800Emu::LoadProject(FProjectConfig* pConfig, bool bLoadGame)
{
    return false;
}

bool FMZ800Emu::SaveProject()
{
    return false;
}

void FMZ800Emu::DrawEmulatorUI()
{
    if (ImGui::Begin("MZ-800 Screen"))
    {
        bool bWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
        if (bWindowFocused)
        {
            for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_COUNT; key++)
            {
                if (ImGui::IsKeyPressed((ImGuiKey)key,false))
                { 
                    int mz_key = MZ800KeyFromImGuiKey((ImGuiKey)key);
                    if (mz_key != 0)
                        mz800_sys_key_down(&g_mz800_sys, mz_key);
                }
                else if (ImGui::IsKeyReleased((ImGuiKey)key))
                {
                    int mz_key = MZ800KeyFromImGuiKey((ImGuiKey)key);
                    if (mz_key != 0)
                        mz800_sys_key_up(&g_mz800_sys, mz_key);
                }
            }
        }

        ImGui::Text("MZ-800 UI - Stepping Z80");
        ImGui::Text("Tick: %d", g_mz800_sys.tick_count);
        ImGui::Text("PC: %04X", g_mz800_sys.cpu.pc);
    }
    ImGui::End();
}

uint8_t FMZ800Emu::ReadByte(uint16_t address) const { 
    return g_mz800_sys.ram[address]; 
}

uint16_t FMZ800Emu::ReadWord(uint16_t address) const { 
    return (uint16_t)g_mz800_sys.ram[address] | ((uint16_t)g_mz800_sys.ram[(address + 1) & 0xFFFF] << 8); 
}

const uint8_t* FMZ800Emu::GetMemPtr(uint16_t address) const { 
    return &g_mz800_sys.ram[address]; 
}

void FMZ800Emu::WriteByte(uint16_t address, uint8_t value) {
    g_mz800_sys.ram[address] = value;
}

FAddressRef FMZ800Emu::GetPC(void) { 
    return FAddressRef(g_mz800_sys.cpu.pc, 0); 
}

uint16_t FMZ800Emu::GetSP(void) { 
    return g_mz800_sys.cpu.sp; 
}

void* FMZ800Emu::GetCPUEmulator(void) const {
    return (void*)&g_mz800_sys.cpu;
}
