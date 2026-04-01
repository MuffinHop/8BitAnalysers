#include "Z80PIODevice.h"

#include <chips/z80.h>
#include <ui/ui_chip.h>
#include <Misc/EmuBase.h>

#include <imgui.h>
#include <cassert>

#ifndef CHIPS_ASSERT
#define CHIPS_ASSERT(c) assert(c)
#endif

typedef struct {
	const char* title;
	z80pio_t* pio;
	int x, y;
	int w, h;
	bool open;
	ui_chip_desc_t chip_desc;
} ui_z80pio_mz_desc_t;

static void ui_z80pio_mz_init(ui_z80pio_mz_t* win, const ui_z80pio_mz_desc_t* desc);

static const ui_chip_pin_t _ui_z80pio_pins[] = {
	{ "D0",    0,  Z80_D0 },
	{ "D1",    1,  Z80_D1 },
	{ "D2",    2,  Z80_D2 },
	{ "D3",    3,  Z80_D3 },
	{ "D4",    4,  Z80_D4 },
	{ "D5",    5,  Z80_D5 },
	{ "D6",    6,  Z80_D6 },
	{ "D7",    7,  Z80_D7 },
	{ "CE",    9,  Z80PIO_CE },
	{ "BASEL",10,  Z80PIO_BASEL },
	{ "CDSEL",11,  Z80PIO_CDSEL },
	{ "M1",   12,  Z80PIO_M1 },
	{ "IORQ", 13,  Z80PIO_IORQ },
	{ "RD",   14,  Z80PIO_RD },
	{ "INT",  15,  Z80PIO_INT },
	{ "PA0",  20,  Z80PIO_PA0 },
	{ "PA1",  21,  Z80PIO_PA1 },
	{ "PA2",  22,  Z80PIO_PA2 },
	{ "PA3",  23,  Z80PIO_PA3 },
	{ "PA4",  24,  Z80PIO_PA4 },
	{ "PA5",  25,  Z80PIO_PA5 },
	{ "PA6",  26,  Z80PIO_PA6 },
	{ "PA7",  27,  Z80PIO_PA7 },
	{ "PB0",  30,  Z80PIO_PB0 },
	{ "PB1",  31,  Z80PIO_PB1 },
	{ "PB2",  32,  Z80PIO_PB2 },
	{ "PB3",  33,  Z80PIO_PB3 },
	{ "PB4",  34,  Z80PIO_PB4 },
	{ "PB5",  35,  Z80PIO_PB5 },
	{ "PB6",  36,  Z80PIO_PB6 },
	{ "PB7",  37,  Z80PIO_PB7 },
};

FZ80PIODevice::FZ80PIODevice()
{
	Name = "Z80 PIO";
}

bool FZ80PIODevice::Init(const char* pName, FEmuBase* pEmulator, z80pio_t* pPIO)
{
	Name = pName;
	pPIOState = pPIO;

	SetAnalyser(&pEmulator->GetCodeAnalysis());
	pEmulator->GetCodeAnalysis().IOAnalyser.AddDevice(this);

	ui_z80pio_mz_desc_t desc = { 0 };
	desc.title = Name.c_str();
	desc.pio = pPIOState;
	desc.x = 0;
	desc.y = 0;
	UI_CHIP_INIT_DESC(&desc.chip_desc, "Z80 PIO", 40, _ui_z80pio_pins);
	ui_z80pio_mz_init(&UIState, &desc);

	return true;
}

void FZ80PIODevice::OnFrameTick()
{
}

void FZ80PIODevice::OnMachineFrameEnd()
{
}

static const char* _z80pio_mode_str(uint8_t mode)
{
	switch (mode) {
		case 0: return "OUT";
		case 1: return "INP";
		case 2: return "BDIR";
		case 3: return "BITC";
		default: return "???";
	}
}

void FZ80PIODevice::DrawPIOStateUI(void)
{
	CHIPS_ASSERT(UIState.valid && UIState.pio);

	ImGui::BeginChild("##pio_chip", ImVec2(176, 0), true);
	ui_chip_draw(&UIState.chip, UIState.pio->pins);
	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::BeginChild("##pio_vals", ImVec2(0, 0), true);

	const z80pio_t* pio = UIState.pio;

	ImGui::Columns(3, "##pio_columns", false);
	ImGui::SetColumnWidth(0, 80);
	ImGui::SetColumnWidth(1, 50);
	ImGui::SetColumnWidth(2, 50);
	ImGui::NextColumn();
	ImGui::Text("PA"); ImGui::NextColumn();
	ImGui::Text("PB"); ImGui::NextColumn();
	ImGui::Separator();

	ImGui::Text("Mode"); ImGui::NextColumn();
	for (int i = 0; i < 2; i++) {
		ImGui::Text("%s", _z80pio_mode_str(pio->port[i].mode)); ImGui::NextColumn();
	}
	ImGui::Text("Output"); ImGui::NextColumn();
	for (int i = 0; i < 2; i++) {
		ImGui::Text("%02X", pio->port[i].output); ImGui::NextColumn();
	}
	ImGui::Text("Input"); ImGui::NextColumn();
	for (int i = 0; i < 2; i++) {
		ImGui::Text("%02X", pio->port[i].input); ImGui::NextColumn();
	}
	ImGui::Text("IO Select"); ImGui::NextColumn();
	for (int i = 0; i < 2; i++) {
		ImGui::Text("%02X", pio->port[i].io_select); ImGui::NextColumn();
	}
	ImGui::Text("INT Ctrl"); ImGui::NextColumn();
	for (int i = 0; i < 2; i++) {
		ImGui::Text("%02X", pio->port[i].int_control); ImGui::NextColumn();
	}
	ImGui::Text("  ei/di"); ImGui::NextColumn();
	for (int i = 0; i < 2; i++) {
		ImGui::Text("%s", pio->port[i].int_control & Z80PIO_INTCTRL_EI ? "EI" : "DI"); ImGui::NextColumn();
	}
	ImGui::Text("  and/or"); ImGui::NextColumn();
	for (int i = 0; i < 2; i++) {
		ImGui::Text("%s", pio->port[i].int_control & Z80PIO_INTCTRL_ANDOR ? "AND" : "OR"); ImGui::NextColumn();
	}
	ImGui::Text("  hi/lo"); ImGui::NextColumn();
	for (int i = 0; i < 2; i++) {
		ImGui::Text("%s", pio->port[i].int_control & Z80PIO_INTCTRL_HILO ? "HI" : "LO"); ImGui::NextColumn();
	}
	ImGui::Text("INT Vec"); ImGui::NextColumn();
	for (int i = 0; i < 2; i++) {
		ImGui::Text("%02X", pio->port[i].int_vector); ImGui::NextColumn();
	}
	ImGui::Text("INT Mask"); ImGui::NextColumn();
	for (int i = 0; i < 2; i++) {
		ImGui::Text("%02X", pio->port[i].int_mask); ImGui::NextColumn();
	}

	ImGui::Columns();
	ImGui::EndChild();
}

void FZ80PIODevice::DrawDetailsUI()
{
	if (pPIOState == nullptr)
		return;
	DrawPIOStateUI();
}

// --- UI init/draw helpers (matching chips ui pattern) ---

static void ui_z80pio_mz_init(ui_z80pio_mz_t* win, const ui_z80pio_mz_desc_t* desc)
{
	CHIPS_ASSERT(win && desc);
	CHIPS_ASSERT(desc->title);
	CHIPS_ASSERT(desc->pio);
	memset(win, 0, sizeof(ui_z80pio_mz_t));
	win->title = desc->title;
	win->pio = desc->pio;
	win->init_x = (float)desc->x;
	win->init_y = (float)desc->y;
	win->init_w = (float)((desc->w == 0) ? 360 : desc->w);
	win->init_h = (float)((desc->h == 0) ? 364 : desc->h);
	win->open = desc->open;
	win->valid = true;
	ui_chip_init(&win->chip, &desc->chip_desc);
}
