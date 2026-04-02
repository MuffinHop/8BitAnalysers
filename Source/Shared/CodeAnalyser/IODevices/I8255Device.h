#pragma once

#include "../IOAnalyser.h"
#include "chips/i8255.h"

class FEmuBase;

struct FI8255Write
{
	i8255_t		EmuState;
	FAddressRef	PC;
	int			FrameNo;
	uint16_t	Port;		// port address (low 2 bits: 0=A, 1=B, 2=C, 3=control)
	uint8_t		Data;
	bool		IsRead;		// true = read, false = write
};

class FI8255Device : public FIODevice
{
public:
	FI8255Device();
	bool	Init(const char* pName, FEmuBase* pEmulator, i8255_t* pPPI);

	void	WritePPI(FAddressRef pc, uint16_t port, uint8_t data);
	void	ReadPPI(FAddressRef pc, uint16_t port, uint8_t data);

	void	OnFrameTick() override;
	void	OnMachineFrameEnd() override;
	void	DrawDetailsUI() override;

private:
	void	DrawStateUI();
	void	DrawLogUI();

	const i8255_t* pPPIState = nullptr;

	int		FrameNo = 0;

	// Access history ring buffer
	static const int kWriteBufferSize = 256;
	FI8255Write	WriteBuffer[kWriteBufferSize];
	int			WriteBufferWriteIndex = 0;
	int			WriteBufferDisplayIndex = 0;
};
