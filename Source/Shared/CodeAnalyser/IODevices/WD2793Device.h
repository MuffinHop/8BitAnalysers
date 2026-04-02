#pragma once

#include "../IOAnalyser.h"
#include "chips/wd2793.h"

class FEmuBase;

struct FWD2793Write
{
	FAddressRef	PC;
	int			FrameNo;
	uint16_t	Port;
	uint8_t		Data;
	uint8_t		RegTrack;
	uint8_t		RegSector;
	uint8_t		RegStatus;
	uint8_t		Motor;
	uint8_t		Side;
};

class FWD2793Device : public FIODevice
{
public:
	FWD2793Device();
	bool	Init(const char* pName, FEmuBase* pEmulator, wd2793_t* pFDC);

	void	WriteFDC(FAddressRef pc, uint16_t port, uint8_t data);

	void	OnFrameTick() override;
	void	OnMachineFrameEnd() override;
	void	DrawDetailsUI() override;

private:
	void	DrawStateUI();
	void	DrawLogUI();
	void	DrawDrivesUI();

	const wd2793_t* pFDCState = nullptr;

	int		FrameNo = 0;

	// Write history ring buffer
	static const int kWriteBufferSize = 256;
	FWD2793Write	WriteBuffer[kWriteBufferSize];
	int				WriteBufferWriteIndex = 0;
	int				WriteBufferDisplayIndex = 0;
};
