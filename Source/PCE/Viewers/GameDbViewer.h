#pragma once

#include "CodeAnalyser/UI/ViewerBase.h"

class FPCEEmu;

class FGameDbViewer : public FViewerBase
{
public:
	FGameDbViewer(FEmuBase* pEmu);

	virtual bool Init() override;
	virtual void Shutdown() override {}
	virtual void DrawUI() override;

protected:
	
	FPCEEmu* pPCEEmu = nullptr;
};
