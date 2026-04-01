#include "I8253Device.h"
#include <Misc/EmuBase.h>
#include <imgui.h>
#include <string.h>

static const char* g_ChannelNames[] = { "Channel 0", "Channel 1", "Channel 2" };

static const char* g_ModeNames[] = {
	"0: Interrupt on TC",
	"1: Programmable One-Shot",
	"2: Rate Generator",
	"3: Square Wave Generator",
	"4: Software Triggered Strobe",
	"5: Hardware Triggered Strobe",
};

static const char* g_StateNames[] = {
	"INIT",
	"INIT_DONE",
	"LOAD",
	"PRESET_ERROR",
	"LOAD_DONE",
	"WAIT_GATE1",
	"PRESET",
	"PRESET32",
	"COUNTDOWN",
	"MODE1_TRIGGER_ERR",
	"BLIND_COUNT",
};

static const char* g_RLFNames[] = {
	"?",    // 0 = invalid
	"LSB",
	"MSB",
	"LSB/MSB",
};

FI8253Device::FI8253Device()
{
	Name = "i8253 PIT";
	memset(Ch0Values, 0, sizeof(Ch0Values));
	memset(Ch1Values, 0, sizeof(Ch1Values));
	memset(Ch2Values, 0, sizeof(Ch2Values));
	memset(Ch0OutValues, 0, sizeof(Ch0OutValues));
	memset(Ch1OutValues, 0, sizeof(Ch1OutValues));
	memset(Ch2OutValues, 0, sizeof(Ch2OutValues));
	memset(WriteBuffer, 0, sizeof(WriteBuffer));
}

bool FI8253Device::Init(const char* pName, FEmuBase* pEmulator, i8253_t* pPIT)
{
	Name = pName;
	pPITState = pPIT;

	SetAnalyser(&pEmulator->GetCodeAnalysis());
	pEmulator->GetCodeAnalysis().IOAnalyser.AddDevice(this);

	return true;
}

void FI8253Device::WritePIT(FAddressRef pc, uint16_t port, uint8_t data)
{
	WriteBufferDisplayIndex = WriteBufferWriteIndex;

	FI8253Write& w = WriteBuffer[WriteBufferWriteIndex];
	w.EmuState = *pPITState;
	w.PC = pc;
	w.FrameNo = FrameNo;
	w.Port = port;
	w.Data = data;

	WriteBufferWriteIndex++;
	if (WriteBufferWriteIndex == kWriteBufferSize)
		WriteBufferWriteIndex = 0;
}

void FI8253Device::OnFrameTick()
{
}

void FI8253Device::OnMachineFrameEnd()
{
	FrameNo++;
}

void FI8253Device::DrawPITStateUI()
{
	if (pPITState == nullptr)
		return;

	ImGui::Text("Intel 8253 PIT - Programmable Interval Timer");
	ImGui::Separator();

	ImGui::Columns(4, "##pit_channels", true);
	ImGui::SetColumnWidth(0, 100);
	ImGui::SetColumnWidth(1, 110);
	ImGui::SetColumnWidth(2, 110);
	ImGui::SetColumnWidth(3, 110);

	ImGui::Text(""); ImGui::NextColumn();
	for (int i = 0; i < 3; i++)
	{
		ImGui::Text("%s", g_ChannelNames[i]); ImGui::NextColumn();
	}
	ImGui::Separator();

	// Mode
	ImGui::Text("Mode"); ImGui::NextColumn();
	for (int i = 0; i < 3; i++)
	{
		int mode = (int)pPITState->channels[i].mode;
		if (mode >= 0 && mode <= 5)
			ImGui::Text("%s", g_ModeNames[mode]);
		else
			ImGui::Text("? (%d)", mode);
		ImGui::NextColumn();
	}

	// State
	ImGui::Text("State"); ImGui::NextColumn();
	for (int i = 0; i < 3; i++)
	{
		int state = (int)pPITState->channels[i].state;
		bool active = (state >= 8); // COUNTDOWN or BLIND_COUNT
		const char* name = (state >= 0 && state <= 10) ? g_StateNames[state] : "?";
		if (active)
			ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%s", name);
		else
			ImGui::Text("%s", name);
		ImGui::NextColumn();
	}

	// R/L Format
	ImGui::Text("R/L Format"); ImGui::NextColumn();
	for (int i = 0; i < 3; i++)
	{
		int rlf = (int)pPITState->channels[i].rlf;
		if (rlf >= 1 && rlf <= 3)
			ImGui::Text("%s", g_RLFNames[rlf]);
		else
			ImGui::Text("? (%d)", rlf);
		ImGui::NextColumn();
	}

	// BCD
	ImGui::Text("BCD"); ImGui::NextColumn();
	for (int i = 0; i < 3; i++)
	{
		ImGui::Text("%s", pPITState->channels[i].bcd ? "Yes" : "No");
		ImGui::NextColumn();
	}

	// Gate
	ImGui::Text("Gate"); ImGui::NextColumn();
	for (int i = 0; i < 3; i++)
	{
		if (pPITState->channels[i].gate)
			ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "HIGH");
		else
			ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "LOW");
		ImGui::NextColumn();
	}

	// Output
	ImGui::Text("Output"); ImGui::NextColumn();
	for (int i = 0; i < 3; i++)
	{
		if (pPITState->channels[i].out)
			ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "HIGH");
		else
			ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "LOW");
		ImGui::NextColumn();
	}

	ImGui::Separator();

	// Counter value
	ImGui::Text("Counter"); ImGui::NextColumn();
	for (int i = 0; i < 3; i++)
	{
		ImGui::Text("%05X (%d)", pPITState->channels[i].value, pPITState->channels[i].value);
		ImGui::NextColumn();
	}

	// Preset
	ImGui::Text("Preset"); ImGui::NextColumn();
	for (int i = 0; i < 3; i++)
	{
		ImGui::Text("%05X (%d)", pPITState->channels[i].preset_value, pPITState->channels[i].preset_value);
		ImGui::NextColumn();
	}

	// Preset latch
	ImGui::Text("Latch"); ImGui::NextColumn();
	for (int i = 0; i < 3; i++)
	{
		ImGui::Text("%04X", pPITState->channels[i].preset_latch & 0xFFFF);
		ImGui::NextColumn();
	}

	// Read latch
	ImGui::Text("Read Latch"); ImGui::NextColumn();
	for (int i = 0; i < 3; i++)
	{
		if (pPITState->channels[i].latch_op)
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.3f, 1.0f), "%04X (latched)",
				pPITState->channels[i].read_latch & 0xFFFF);
		else
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "-");
		ImGui::NextColumn();
	}

	// R/W byte flip-flop
	ImGui::Text("R/W Byte"); ImGui::NextColumn();
	for (int i = 0; i < 3; i++)
	{
		ImGui::Text("%s", pPITState->channels[i].rl_byte ? "MSB next" : "LSB next");
		ImGui::NextColumn();
	}

	ImGui::Columns();
	ImGui::Separator();

	// Mode 3 specific details
	bool hasMode3 = false;
	for (int i = 0; i < 3; i++)
	{
		if (pPITState->channels[i].mode == I8253_MODE3) { hasMode3 = true; break; }
	}
	if (hasMode3)
	{
		ImGui::Text("Square Wave Details:");
		for (int i = 0; i < 3; i++)
		{
			if (pPITState->channels[i].mode == I8253_MODE3)
			{
				ImGui::Text("  Ch%d: half_value=%d  dest_value=%d",
					i, pPITState->channels[i].mode3_half_value,
					pPITState->channels[i].mode3_destination_value);
			}
		}
		ImGui::Separator();
	}

	// Counter progress bars
	ImGui::Text("Counter Progress:");
	for (int i = 0; i < 3; i++)
	{
		uint32_t preset = pPITState->channels[i].preset_value;
		uint32_t val = pPITState->channels[i].value;
		float progress = 0.0f;
		if (preset > 0 && val <= preset)
			progress = 1.0f - ((float)val / (float)preset);

		char label[48];
		snprintf(label, sizeof(label), "Ch%d: %d / %d", i, preset - val, preset);
		ImGui::ProgressBar(progress, ImVec2(-1, 14), label);
	}
}

void FI8253Device::DrawPITLogUI()
{
	if (pPITState == nullptr)
		return;

	ImGui::Text("Write Log (last %d writes):", kWriteBufferSize);
	ImGui::Separator();

	ImGui::Columns(5, "##pit_log", true);
	ImGui::SetColumnWidth(0, 60);
	ImGui::SetColumnWidth(1, 80);
	ImGui::SetColumnWidth(2, 50);
	ImGui::SetColumnWidth(3, 50);
	ImGui::SetColumnWidth(4, 220);
	ImGui::Text("Frame"); ImGui::NextColumn();
	ImGui::Text("PC"); ImGui::NextColumn();
	ImGui::Text("Port"); ImGui::NextColumn();
	ImGui::Text("Data"); ImGui::NextColumn();
	ImGui::Text("Decoded"); ImGui::NextColumn();
	ImGui::Separator();

	int idx = WriteBufferDisplayIndex;
	for (int n = 0; n < kWriteBufferSize; n++)
	{
		const FI8253Write& w = WriteBuffer[idx];
		if (w.FrameNo == 0 && w.Data == 0 && idx != 0)
		{
			idx--;
			if (idx < 0) idx = kWriteBufferSize - 1;
			continue;
		}

		ImGui::Text("%d", w.FrameNo); ImGui::NextColumn();
		ImGui::Text("%04X", w.PC.Address); ImGui::NextColumn();

		uint8_t addr = w.Port & 0x03;
		ImGui::Text("%d", addr); ImGui::NextColumn();
		ImGui::Text("%02X", w.Data); ImGui::NextColumn();

		if (addr == 3)
		{
			// Control word
			uint8_t cs = w.Data >> 6;
			uint8_t rlf = (w.Data >> 4) & 0x03;
			uint8_t mode = (w.Data >> 1) & 0x07;
			uint8_t bcd = w.Data & 0x01;
			if (cs <= 2)
			{
				if (rlf == 0)
					ImGui::Text("Ch%d: LATCH", cs);
				else
				{
					const char* rlfName = (rlf >= 1 && rlf <= 3) ? g_RLFNames[rlf] : "?";
					const char* modeName = (mode <= 5) ? g_ModeNames[mode] : "?";
					ImGui::Text("Ch%d: CW mode=%s rl=%s%s", cs, modeName, rlfName,
						bcd ? " BCD" : "");
				}
			}
			else
			{
				ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "ILLEGAL (cs=3)");
			}
		}
		else
		{
			// Counter write
			ImGui::Text("Ch%d: data=%02X (%d)", addr, w.Data, w.Data);
		}
		ImGui::NextColumn();

		idx--;
		if (idx < 0) idx = kWriteBufferSize - 1;
	}
	ImGui::Columns();
}

void FI8253Device::DrawPITGraphUI()
{
	if (pPITState == nullptr)
		return;

	// Sample current values
	Ch0Values[GraphOffset] = (float)pPITState->channels[0].value;
	Ch1Values[GraphOffset] = (float)pPITState->channels[1].value;
	Ch2Values[GraphOffset] = (float)pPITState->channels[2].value;
	Ch0OutValues[GraphOffset] = pPITState->channels[0].out ? 1.0f : 0.0f;
	Ch1OutValues[GraphOffset] = pPITState->channels[1].out ? 1.0f : 0.0f;
	Ch2OutValues[GraphOffset] = pPITState->channels[2].out ? 1.0f : 0.0f;
	GraphOffset = (GraphOffset + 1) % kNoValues;

	for (int i = 0; i < 3; i++)
	{
		float* counterVals = (i == 0) ? Ch0Values : (i == 1) ? Ch1Values : Ch2Values;
		float* outVals = (i == 0) ? Ch0OutValues : (i == 1) ? Ch1OutValues : Ch2OutValues;

		float preset = (float)pPITState->channels[i].preset_value;

		char label[32];
		snprintf(label, sizeof(label), "Ch%d Counter", i);
		ImGui::Text("Channel %d  (preset=%d  mode=%d  out=%s)",
			i, pPITState->channels[i].preset_value,
			(int)pPITState->channels[i].mode,
			pPITState->channels[i].out ? "H" : "L");

		char plotId[32];
		snprintf(plotId, sizeof(plotId), "##cnt%d", i);
		ImGui::PlotLines(plotId, counterVals, kNoValues, GraphOffset,
			label, 0.0f, preset > 0 ? preset : 65536.0f, ImVec2(-1, 50));

		snprintf(plotId, sizeof(plotId), "##out%d", i);
		snprintf(label, sizeof(label), "Ch%d Output", i);
		ImGui::PlotLines(plotId, outVals, kNoValues, GraphOffset,
			label, -0.1f, 1.1f, ImVec2(-1, 30));

		ImGui::Separator();
	}
}

void FI8253Device::DrawDetailsUI()
{
	if (pPITState == nullptr)
		return;

	if (ImGui::BeginTabBar("PITDetailsTabBar"))
	{
		if (ImGui::BeginTabItem("State"))
		{
			DrawPITStateUI();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Graphs"))
		{
			DrawPITGraphUI();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Log"))
		{
			DrawPITLogUI();
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}
}
