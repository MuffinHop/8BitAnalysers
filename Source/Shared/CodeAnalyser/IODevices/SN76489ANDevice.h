#pragma once

#include "../IOAnalyser.h"
#include <chips/sn76489an.h>

class FEmuBase;

struct FSN76489ANWrite
{
	sn76489an_t	EmuState;
	FAddressRef	PC;
	int			FrameNo;
	uint8_t		Data;
};

class FSN76489ANDevice : public FIODevice
{
public:
	FSN76489ANDevice();
	bool	Init(const char* pName, FEmuBase* pEmulator, sn76489an_t* pPSG);

	void	WritePSG(FAddressRef pc, uint8_t data);

	void	OnFrameTick() override;
	void	OnMachineFrameEnd() override;
	void	DrawDetailsUI() override;

private:
	void	DrawPSGStateUI();
	void	DrawPSGLogUI();
	void	DrawPSGGraphUI();

	const sn76489an_t* pPSGState = nullptr;

	int			FrameNo = 0;

	// Write history ring buffer
	static const int kWriteBufferSize = 256;
	FSN76489ANWrite	WriteBuffer[kWriteBufferSize];
	int				WriteBufferWriteIndex = 0;
	int				WriteBufferDisplayIndex = 0;

	// Per-channel frequency graphs
	static const int kNoValues = 100;
	float	ChanAValues[kNoValues];
	float	ChanBValues[kNoValues];
	float	ChanCValues[kNoValues];
	float	NoiseValues[kNoValues];
	int		GraphOffset = 0;
};
