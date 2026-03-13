#include "DebugStats.h"
#include "PCEEmu.h"

FGameDebugStats* FEmuDebugStats::GetDebugStatsForGame(const std::string& gameName)
{
	auto it = GameDebugStats.find(gameName);
	if (it != GameDebugStats.end())
	{
		return &it->second;
	}
	return nullptr;
}

void FEmuDebugStats::InitForGame(FPCEEmu* pEmu, const std::string& gameName)
{
	GameDebugStats[gameName].NumBanks = pEmu->GetBankCount();
}

void FEmuDebugStats::Reset()
{
	GameDebugStats.clear();
}