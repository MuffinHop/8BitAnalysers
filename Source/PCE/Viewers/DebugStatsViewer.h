#pragma once

#include "CodeAnalyser/UI/ViewerBase.h"
#include <map>

class FPCEEmu;

class FDebugStatsViewer : public FViewerBase
{
public:
	FDebugStatsViewer(FEmuBase* pEmu);

	virtual bool Init() override;
	virtual void Shutdown() override {}
	virtual void DrawUI() override;

protected:
	
	std::map<std::string, float> TimeUntilMapped;

	FPCEEmu* pPCEEmu = nullptr;
};
