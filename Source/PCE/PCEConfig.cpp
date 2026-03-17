#include "PCEConfig.h"

#include "json.hpp"
#include "Util/FileUtil.h"

bool FPCEConfig::Init(void)
{
	if(FGlobalConfig::Init() == false)
		return false;

	//LuaBaseFiles.push_back("Lua/PCEBase.lua");
	SnapshotFolder = GetDocumentsPath("PCEGames");
	WorkspaceRoot = GetDocumentsPath("PCEAnalyserProjects");

	FixupPaths();

	return true;
}

void FPCEConfig::ReadFromJson(const nlohmann::json& jsonConfigFile)
{
	FGlobalConfig::ReadFromJson(jsonConfigFile);

	if (jsonConfigFile.contains("BiosPath"))
		BiosPath = jsonConfigFile["BiosPath"];

	if (jsonConfigFile.contains("BiosFilename"))
		BiosFilename = jsonConfigFile["BiosFilename"];

	if (jsonConfigFile.contains("GameDbPath"))
		GameDbPath = jsonConfigFile["GameDbPath"];
	
	if (jsonConfigFile.contains("AsmExportValidator"))
		bUseAsmExportValidator = jsonConfigFile["AsmExportValidator"];

	FixupPaths();
}

void FPCEConfig::WriteToJson(nlohmann::json& jsonConfigFile) const
{
	FGlobalConfig::WriteToJson(jsonConfigFile);

	jsonConfigFile["BiosPath"] = BiosPath;
	jsonConfigFile["BiosFilename"] = BiosFilename;
	jsonConfigFile["GameDbPath"] = GameDbPath;

#ifndef NDEBUG
	jsonConfigFile["AsmExportValidator"] = bUseAsmExportValidator;
#endif
}

void FPCEConfig::FixupPaths(void)
{
	FGlobalConfig::FixupPaths();

	if (BiosPath.back() != '/')
		BiosPath += "/";
}
