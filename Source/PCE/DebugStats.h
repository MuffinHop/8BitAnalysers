
#pragma once
#include <string>
#include <map>

class FPCEEmu;

struct FGameDebugStats
{
	int NumDupeBanks = 0;
	int NumBanks = 0;
	int NumBanksMapped = 0;
	int MaxBankSwitches = 0;
};

struct FEmuDebugStats
{
	void Reset();
	void InitForGame(FPCEEmu* pEmu, const std::string& gameName);
	FGameDebugStats* GetDebugStatsForGame(const std::string& gameName);

	// Debug stats for each game. Uses project name as key
	std::map<std::string, FGameDebugStats> GameDebugStats;

	int NumBankSwitchesThisFrame = 0;
};
