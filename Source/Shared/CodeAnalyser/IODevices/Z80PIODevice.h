#pragma once

#include "../IOAnalyser.h"

#include <chips/z80pio.h>
#include <ui/ui_util.h>
#include <ui/ui_chip.h>

class FEmuBase;

typedef struct {
	const char* title;
	z80pio_t* pio;
	float init_x, init_y;
	float init_w, init_h;
	bool open;
	bool valid;
	ui_chip_t chip;
} ui_z80pio_mz_t;

class FZ80PIODevice : public FIODevice
{
public:
	FZ80PIODevice();
	bool	Init(const char* pName, FEmuBase* pEmulator, z80pio_t* pPIO);

	void	OnFrameTick() override;
	void	OnMachineFrameEnd() override;
	void	DrawDetailsUI() override;
	void	DrawPIOStateUI(void);

private:
	z80pio_t*		pPIOState = nullptr;
	ui_z80pio_mz_t	UIState;
};
