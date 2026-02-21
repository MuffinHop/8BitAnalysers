#include "PCEGameConfig.h"

#include <json.hpp>
#include <iomanip>
#include <fstream>
#include <sstream>

#include <Util/FileUtil.h>
#include "PCEConfig.h"
#include "PCEEmu.h"

// PCE specific

void FPCEGameConfig::LoadFromJson(const nlohmann::json& jsonConfigFile)
{
	FProjectConfig::LoadFromJson(jsonConfigFile);

	if(EmulatorFile.ListName.empty())
		EmulatorFile.ListName = "Snapshot File";

	MprBankId[0] = jsonConfigFile["Mpr0BankId"];
	MprBankId[1] = jsonConfigFile["Mpr1BankId"];
	MprBankId[2] = jsonConfigFile["Mpr2BankId"];
	MprBankId[3] = jsonConfigFile["Mpr3BankId"];
	MprBankId[4] = jsonConfigFile["Mpr4BankId"];
	MprBankId[5] = jsonConfigFile["Mpr5BankId"];
	MprBankId[6] = jsonConfigFile["Mpr6BankId"];
	MprBankId[7] = jsonConfigFile["Mpr7BankId"];
}

void FPCEGameConfig::SaveToJson(nlohmann::json& jsonConfigFile) const
{
	FProjectConfig::SaveToJson(jsonConfigFile);

	jsonConfigFile["Mpr0BankId"] = MprBankId[0];
	jsonConfigFile["Mpr1BankId"] = MprBankId[1];
	jsonConfigFile["Mpr2BankId"] = MprBankId[2];
	jsonConfigFile["Mpr3BankId"] = MprBankId[3];
	jsonConfigFile["Mpr4BankId"] = MprBankId[4];
	jsonConfigFile["Mpr5BankId"] = MprBankId[5];
	jsonConfigFile["Mpr6BankId"] = MprBankId[6];
	jsonConfigFile["Mpr7BankId"] = MprBankId[7];
}

FPCEGameConfig* CreateNewPCEGameConfigFromSnapshot(const FEmulatorFile& snapshot)
{
	FPCEGameConfig* pNewConfig = new FPCEGameConfig;

	pNewConfig->Name = RemoveFileExtension(snapshot.DisplayName.c_str());
	pNewConfig->EmulatorFile = snapshot;

	return pNewConfig;
}

FPCEGameConfig* CreateNewEmptyConfig(void)
{
	FPCEGameConfig* pNewConfig = new FPCEGameConfig;

	pNewConfig->Name = "No Project";

	return pNewConfig;
}

bool LoadPCEGameConfigs(FPCEEmu* pEmu)
{
	FDirFileList listing;
	
	const std::string root = pEmu->GetGlobalConfig()->WorkspaceRoot;

	// Search through each game directory
	if (EnumerateDirectory(root.c_str(), listing) == false)
		return false;

	for (const auto& file : listing)
	{
		if (file.FileType == FDirEntry::Directory)
		{
			if(file.FileName == "." || file.FileName == "..")
				continue;

			FDirFileList directoryListing;
			std::string gameDir = root + file.FileName;
			if (EnumerateDirectory(gameDir.c_str(), directoryListing))
			{
				for (const auto& gameFile : directoryListing)
				{
					if (gameFile.FileName == "Config.json")
					{
						const std::string& configFileName = gameDir + "/" + gameFile.FileName;
						FPCEGameConfig* pNewConfig = new FPCEGameConfig;
						if (LoadGameConfigFromFile(*pNewConfig, configFileName.c_str()))
						{
							AddGameConfig(pNewConfig);
						}
						else
						{
							delete pNewConfig;
						}

					}
				}
			}
		}
	}

	return true;
}
