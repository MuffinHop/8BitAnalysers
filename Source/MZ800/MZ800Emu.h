#pragma once
#include "Misc/EmuBase.h"
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
    bool    bRomLoaded    = false;

    // MZF file buffer — must outlive sys->cmt.body pointer
    std::vector<uint8_t> MzfBuffer;
};
