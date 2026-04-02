#pragma once

#include "../IOAnalyser.h"
#include "chips/cmt.h"

class FEmuBase;

class FCMTDevice : public FIODevice
{
public:
	FCMTDevice();
	bool	Init(const char* pName, FEmuBase* pEmulator, cmt_t* pCMT);

	void	OnFrameTick() override;
	void	OnMachineFrameEnd() override;
	void	DrawDetailsUI() override;

private:
	void	DrawStateUI();
	void	DrawSignalUI();

	const cmt_t* pCMTState = nullptr;

	int		FrameNo = 0;

	// Signal graph (output level over time)
	static const int kSignalLen = 200;
	float	SignalValues[kSignalLen];
	float	ProgressValues[kSignalLen];
	int		GraphOffset = 0;
};
