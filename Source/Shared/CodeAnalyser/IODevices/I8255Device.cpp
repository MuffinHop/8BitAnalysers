#include "I8255Device.h"
#include <Misc/EmuBase.h>
#include <imgui.h>
#include <string.h>

static const char* g_PortNames[] = { "Port A", "Port B", "Port C", "Control" };

static const char* g_GroupAModeNames[] = {
	"Mode 0 (Basic I/O)",
	"Mode 1 (Strobed I/O)",
	"Mode 2 (Bi-directional)",
	"Mode 2 (Bi-directional)",
};

static const char* g_GroupBModeNames[] = {
	"Mode 0 (Basic I/O)",
	"Mode 1 (Strobed I/O)",
};

FI8255Device::FI8255Device()
{
	Name = "i8255 PPI";
	memset(WriteBuffer, 0, sizeof(WriteBuffer));
}

bool FI8255Device::Init(const char* pName, FEmuBase* pEmulator, i8255_t* pPPI)
{
	Name = pName;
	pPPIState = pPPI;

	SetAnalyser(&pEmulator->GetCodeAnalysis());
	pEmulator->GetCodeAnalysis().IOAnalyser.AddDevice(this);

	return true;
}

void FI8255Device::WritePPI(FAddressRef pc, uint16_t port, uint8_t data)
{
	WriteBufferDisplayIndex = WriteBufferWriteIndex;

	FI8255Write& w = WriteBuffer[WriteBufferWriteIndex];
	w.EmuState = *pPPIState;
	w.PC = pc;
	w.FrameNo = FrameNo;
	w.Port = port;
	w.Data = data;
	w.IsRead = false;

	WriteBufferWriteIndex++;
	if (WriteBufferWriteIndex == kWriteBufferSize)
		WriteBufferWriteIndex = 0;
}

void FI8255Device::ReadPPI(FAddressRef pc, uint16_t port, uint8_t data)
{
	WriteBufferDisplayIndex = WriteBufferWriteIndex;

	FI8255Write& w = WriteBuffer[WriteBufferWriteIndex];
	w.EmuState = *pPPIState;
	w.PC = pc;
	w.FrameNo = FrameNo;
	w.Port = port;
	w.Data = data;
	w.IsRead = true;

	WriteBufferWriteIndex++;
	if (WriteBufferWriteIndex == kWriteBufferSize)
		WriteBufferWriteIndex = 0;
}

void FI8255Device::OnFrameTick()
{
}

void FI8255Device::OnMachineFrameEnd()
{
	FrameNo++;
}

// --- State Tab ---

void FI8255Device::DrawStateUI()
{
	if (pPPIState == nullptr)
		return;

	ImGui::Text("Intel 8255 PPI - Programmable Peripheral Interface");
	ImGui::Separator();

	// Control word
	const uint8_t ctrl = pPPIState->control;

	ImGui::Text("Control Word: %02X", ctrl);
	if (ctrl & I8255_CTRL_CONTROL)
		ImGui::SameLine(), ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "(Mode Select)");
	ImGui::Separator();

	// Group A
	{
		int modeA = (ctrl >> 5) & 0x03;
		bool portA_input = (ctrl & I8255_CTRL_A) != 0;
		bool portChi_input = (ctrl & I8255_CTRL_CHI) != 0;

		ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Group A");
		ImGui::Text("  Mode:       %s", g_GroupAModeNames[modeA]);
		ImGui::Text("  Port A:     %s", portA_input ? "INPUT" : "OUTPUT");
		ImGui::Text("  Port C Hi:  %s", portChi_input ? "INPUT" : "OUTPUT");
	}

	ImGui::Separator();

	// Group B
	{
		int modeB = (ctrl >> 2) & 0x01;
		bool portB_input = (ctrl & I8255_CTRL_B) != 0;
		bool portClo_input = (ctrl & I8255_CTRL_CLO) != 0;

		ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Group B");
		ImGui::Text("  Mode:       %s", g_GroupBModeNames[modeB]);
		ImGui::Text("  Port B:     %s", portB_input ? "INPUT" : "OUTPUT");
		ImGui::Text("  Port C Lo:  %s", portClo_input ? "INPUT" : "OUTPUT");
	}

	ImGui::Separator();

	// Port values table
	ImGui::Text("Port Output Latches:");
	ImGui::Columns(4, "##ppi_ports", true);
	ImGui::SetColumnWidth(0, 80);
	ImGui::SetColumnWidth(1, 80);
	ImGui::SetColumnWidth(2, 80);
	ImGui::SetColumnWidth(3, 80);

	ImGui::Text(""); ImGui::NextColumn();
	ImGui::Text("Hex"); ImGui::NextColumn();
	ImGui::Text("Dec"); ImGui::NextColumn();
	ImGui::Text("Binary"); ImGui::NextColumn();
	ImGui::Separator();

	// Port A
	ImGui::Text("Port A"); ImGui::NextColumn();
	ImGui::Text("%02X", pPPIState->pa.outp); ImGui::NextColumn();
	ImGui::Text("%d", pPPIState->pa.outp); ImGui::NextColumn();
	{
		char bits[9];
		for (int b = 7; b >= 0; b--) bits[7-b] = (pPPIState->pa.outp & (1<<b)) ? '1' : '0';
		bits[8] = 0;
		ImGui::Text("%s", bits);
	}
	ImGui::NextColumn();

	// Port B
	ImGui::Text("Port B"); ImGui::NextColumn();
	ImGui::Text("%02X", pPPIState->pb.outp); ImGui::NextColumn();
	ImGui::Text("%d", pPPIState->pb.outp); ImGui::NextColumn();
	{
		char bits[9];
		for (int b = 7; b >= 0; b--) bits[7-b] = (pPPIState->pb.outp & (1<<b)) ? '1' : '0';
		bits[8] = 0;
		ImGui::Text("%s", bits);
	}
	ImGui::NextColumn();

	// Port C
	ImGui::Text("Port C"); ImGui::NextColumn();
	ImGui::Text("%02X", pPPIState->pc.outp); ImGui::NextColumn();
	ImGui::Text("%d", pPPIState->pc.outp); ImGui::NextColumn();
	{
		char bits[9];
		for (int b = 7; b >= 0; b--) bits[7-b] = (pPPIState->pc.outp & (1<<b)) ? '1' : '0';
		bits[8] = 0;
		ImGui::Text("%s", bits);
	}
	ImGui::NextColumn();

	ImGui::Columns();
	ImGui::Separator();

	// MZ-800 specific: Port A bit meanings (keyboard column select)
	ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "MZ-800 Port A Usage:");
	{
		int key_col = pPPIState->pa.outp & 0x0F;
		ImGui::Text("  Keyboard column select: %d  (PA0-PA3)", key_col);
		if (key_col >= 10)
			ImGui::SameLine(), ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "(out of range)");
	}
}

// --- Log Tab ---

static const char* DecodePPIWrite(uint8_t addr, uint8_t data, bool isRead)
{
	static char buf[128];

	if (isRead)
	{
		switch (addr & 3)
		{
			case 0: snprintf(buf, sizeof(buf), "Read Port A = %02X", data); break;
			case 1: snprintf(buf, sizeof(buf), "Read Port B = %02X (keyboard lines)", data); break;
			case 2: snprintf(buf, sizeof(buf), "Read Port C = %02X", data); break;
			case 3: snprintf(buf, sizeof(buf), "Read Control = %02X", data); break;
		}
		return buf;
	}

	switch (addr & 3)
	{
		case 0: snprintf(buf, sizeof(buf), "Port A = %02X (key col %d)", data, data & 0x0F); break;
		case 1: snprintf(buf, sizeof(buf), "Port B = %02X", data); break;
		case 2: snprintf(buf, sizeof(buf), "Port C = %02X", data); break;
		case 3:
			if (data & 0x80)
			{
				// Mode select
				int modeA = (data >> 5) & 0x03;
				bool paIn = (data & 0x10) != 0;
				bool chiIn = (data & 0x08) != 0;
				int modeB = (data >> 2) & 0x01;
				bool pbIn = (data & 0x02) != 0;
				bool cloIn = (data & 0x01) != 0;
				snprintf(buf, sizeof(buf), "MODE SET: A=m%d/%s Chi=%s B=m%d/%s Clo=%s",
					modeA, paIn ? "in" : "out", chiIn ? "in" : "out",
					modeB, pbIn ? "in" : "out", cloIn ? "in" : "out");
			}
			else
			{
				// Bit set/reset
				int bit = (data >> 1) & 7;
				bool set = (data & 1) != 0;
				snprintf(buf, sizeof(buf), "PC%d %s", bit, set ? "SET" : "RESET");
			}
			break;
	}
	return buf;
}

void FI8255Device::DrawLogUI()
{
	if (pPPIState == nullptr)
		return;

	ImGui::Text("Access Log (last %d accesses):", kWriteBufferSize);
	ImGui::Separator();

	ImGui::Columns(6, "##ppi_log", true);
	ImGui::SetColumnWidth(0, 60);
	ImGui::SetColumnWidth(1, 80);
	ImGui::SetColumnWidth(2, 40);
	ImGui::SetColumnWidth(3, 50);
	ImGui::SetColumnWidth(4, 50);
	ImGui::SetColumnWidth(5, 280);
	ImGui::Text("Frame"); ImGui::NextColumn();
	ImGui::Text("PC"); ImGui::NextColumn();
	ImGui::Text("R/W"); ImGui::NextColumn();
	ImGui::Text("Port"); ImGui::NextColumn();
	ImGui::Text("Data"); ImGui::NextColumn();
	ImGui::Text("Decoded"); ImGui::NextColumn();
	ImGui::Separator();

	int idx = WriteBufferDisplayIndex;
	for (int n = 0; n < kWriteBufferSize; n++)
	{
		const FI8255Write& w = WriteBuffer[idx];
		if (w.FrameNo == 0 && w.Data == 0 && idx != 0)
		{
			idx--;
			if (idx < 0) idx = kWriteBufferSize - 1;
			continue;
		}

		ImGui::Text("%d", w.FrameNo); ImGui::NextColumn();
		ImGui::Text("%04X", w.PC.Address); ImGui::NextColumn();

		if (w.IsRead)
			ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "RD");
		else
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "WR");
		ImGui::NextColumn();

		uint8_t addr = w.Port & 0x03;
		ImGui::Text("%s", g_PortNames[addr]); ImGui::NextColumn();
		ImGui::Text("%02X", w.Data); ImGui::NextColumn();

		ImGui::Text("%s", DecodePPIWrite(addr, w.Data, w.IsRead));
		ImGui::NextColumn();

		idx--;
		if (idx < 0) idx = kWriteBufferSize - 1;
	}
	ImGui::Columns();
}

// --- Main DrawDetailsUI ---

void FI8255Device::DrawDetailsUI()
{
	if (pPPIState == nullptr)
		return;

	if (ImGui::BeginTabBar("PPIDetailsTabBar"))
	{
		if (ImGui::BeginTabItem("State"))
		{
			DrawStateUI();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Log"))
		{
			DrawLogUI();
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}
}
