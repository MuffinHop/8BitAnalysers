#include "MZ800Emu.h"
#include "MZ800ChipsImpl.h"
#include "MZ800Config.h"
#include <imGui/imgui.h>

extern void SetWindowTitle(const char* pTitle);

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
