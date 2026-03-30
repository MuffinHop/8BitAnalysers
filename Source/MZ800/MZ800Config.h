#pragma once
#include "Misc/GlobalConfig.h"
#include "Misc/GameConfig.h"

struct FMZ800GlobalConfig : public FGlobalConfig
{
    bool        bPAL     = true;
    // ROM path: absolute, or relative (resolved via GetBundlePath).
    // Supports single all-in-one 16KB file, or three separate files.
    std::string RomPath  = "Roms/mz800.rom";   // all-in-one 16KB ROM
    std::string Rom700   = "Roms/mz700.rom";   // fallback: MZ-700 monitor (4KB)
    std::string RomCG    = "Roms/cgrom.rom";   // fallback: CGROM (4KB)
    std::string Rom800   = "Roms/mz800.rom";   // fallback: MZ-800 monitor (8KB)
    std::string MzfPath;                        // last loaded MZF tape file

    bool Init(void) override { return true; }

protected:
    void ReadFromJson(const nlohmann::json& jsonConfigFile) override;
    void WriteToJson(nlohmann::json& jsonConfigFile) const override;
};

struct FMZ800ProjectConfig : public FProjectConfig
{
};
