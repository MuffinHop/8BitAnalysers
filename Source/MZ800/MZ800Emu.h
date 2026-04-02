#pragma once
#include "Misc/EmuBase.h"
#include <CodeAnalyser/IODevices/I8253Device.h>
#include <CodeAnalyser/IODevices/SN76489ANDevice.h>
#include <CodeAnalyser/IODevices/Z80PIODevice.h>
#include <CodeAnalyser/IODevices/GDGDevice.h>
#include <ImGuiSupport/ImGuiTexture.h>
#include <vector>
#include <cstdint>

struct FMZ800GlobalConfig;

class FMZ800Emu : public FEmuBase
{
public:
    bool Init(const FEmulatorLaunchConfig& launchConfig) override;
    void Shutdown() override;
    void Tick() override;
    void Reset() override;

    bool LoadEmulatorFile(const FEmulatorFile* pSnapshot) override;
    bool NewProjectFromEmulatorFile(const FEmulatorFile& gameSnapshot) override;
    bool LoadProject(FProjectConfig* pConfig, bool bLoadGame) override;
    bool SaveProject() override;

    void DrawEmulatorUI() override;

    // ICPUInterface
    uint8_t ReadByte(uint16_t address) const override;
    uint16_t ReadWord(uint16_t address) const override;
    const uint8_t* GetMemPtr(uint16_t address) const override;
    void WriteByte(uint16_t address, uint8_t value) override;
    FAddressRef GetPC(void) override;
    uint16_t GetSP(void) override;
    void* GetCPUEmulator(void) const override;

private:
    bool LoadROM();
    bool LoadMZF(const char* path);

    int16_t MainRAMBankId = -1;
    int16_t ROM0000BankId = -1;  // MZ-700 monitor ROM (0x0000-0x0FFF, 4KB)
    int16_t ROM1000BankId = -1;  // CGROM (0x1000-0x1FFF, 4KB)
    int16_t ROME000BankId = -1;  // MZ-800 monitor ROM (0xE000-0xFFFF, 8KB)
    bool    bRomLoaded    = false;

    // MZF file buffer — must outlive sys->cmt.body pointer
    std::vector<uint8_t> MzfBuffer;

    // Display
    uint32_t*   FrameBuffer = nullptr;   // RGBA pixel buffer
    ImTextureID ScreenTexture = nullptr; // ImGui GPU texture

    FI8253Device     PITDevice;
    FSN76489ANDevice PSGDevice;
    FZ80PIODevice    PIODevice;
    FGDGDevice       GDGDevice;

    // Code analysis integration
    uint16_t PreviousPC = 0;
    int      InstructionsTicks = 0;

    void OnInstructionExecuted(int ticks, uint64_t pins);
    static void CPUTickHook(uint64_t pins, void* user_data);
};
