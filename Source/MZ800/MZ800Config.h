#pragma once
#include "Misc/GlobalConfig.h"
#include "Misc/GameConfig.h"

struct FMZ800GlobalConfig : public FGlobalConfig
{
    bool bPAL = true;

    bool Init(void) override { return true; }

protected:
    void ReadFromJson(const nlohmann::json& jsonConfigFile) override {}
    void WriteToJson(nlohmann::json& jsonConfigFile) const override {}
};

struct FMZ800ProjectConfig : public FProjectConfig
{
};
