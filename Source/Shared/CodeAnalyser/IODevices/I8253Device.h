#pragma once

#include "../IOAnalyser.h"
#include <chips/i8253.h>

class FEmuBase;

struct FI8253Write
{
	i8253_t		EmuState;
	FAddressRef	PC;
	int			FrameNo;
	uint16_t	Port;		// port address (low 2 bits: 0-2=channel, 3=control)
	uint8_t		Data;
};

class FI8253Device : public FIODevice
{
public:
	FI8253Device();
	bool	Init(const char* pName, FEmuBase* pEmulator, i8253_t* pPIT);

	void	WritePIT(FAddressRef pc, uint16_t port, uint8_t data);

	void	OnFrameTick() override;
	void	OnMachineFrameEnd() override;
	void	DrawDetailsUI() override;

private:
	void	DrawPITStateUI();
	void	DrawPITLogUI();
	void	DrawPITGraphUI();

	const i8253_t* pPITState = nullptr;

	int		FrameNo = 0;

	// Write history ring buffer
	static const int kWriteBufferSize = 256;
	FI8253Write	WriteBuffer[kWriteBufferSize];
	int			WriteBufferWriteIndex = 0;
	int			WriteBufferDisplayIndex = 0;

	// Per-channel counter value graphs
	static const int kNoValues = 200;
	float	Ch0Values[kNoValues];
	float	Ch1Values[kNoValues];
	float	Ch2Values[kNoValues];
	float	Ch0OutValues[kNoValues];
	float	Ch1OutValues[kNoValues];
	float	Ch2OutValues[kNoValues];
	int		GraphOffset = 0;
};
