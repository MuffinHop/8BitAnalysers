#include "SN76489ANDevice.h"
#include <Misc/EmuBase.h>
#include <imgui.h>
#include <string.h>

static const char* g_SN76489ChannelNames[] = {
	"Tone A",
	"Tone B",
	"Tone C",
	"Noise",
};

// SN76489AN master clock is divided by 16 internally.
// MZ-800 PSG clock = CPU clock / 16 = ~223 kHz (PAL) or ~224 kHz (NTSC).
// For frequency display we use the PSG-tick rate (after /16).
static const int kPSGTickRate = 223721;  // approximate

FSN76489ANDevice::FSN76489ANDevice()
{
	Name = "SN76489AN";
	memset(ChanAValues, 0, sizeof(ChanAValues));
	memset(ChanBValues, 0, sizeof(ChanBValues));
	memset(ChanCValues, 0, sizeof(ChanCValues));
	memset(NoiseValues, 0, sizeof(NoiseValues));
	memset(WriteBuffer, 0, sizeof(WriteBuffer));
}

bool FSN76489ANDevice::Init(const char* pName, FEmuBase* pEmulator, sn76489an_t* pPSG)
{
	Name = pName;
	pPSGState = pPSG;

	SetAnalyser(&pEmulator->GetCodeAnalysis());
	pEmulator->GetCodeAnalysis().IOAnalyser.AddDevice(this);

	return true;
}

void FSN76489ANDevice::WritePSG(FAddressRef pc, uint8_t data)
{
	WriteBufferDisplayIndex = WriteBufferWriteIndex;

	FSN76489ANWrite& w = WriteBuffer[WriteBufferWriteIndex];
	w.EmuState = *pPSGState;
	w.PC = pc;
	w.FrameNo = FrameNo;
	w.Data = data;

	WriteBufferWriteIndex++;
	if (WriteBufferWriteIndex == kWriteBufferSize)
		WriteBufferWriteIndex = 0;
}

void FSN76489ANDevice::OnFrameTick()
{
}

void FSN76489ANDevice::OnMachineFrameEnd()
{
	FrameNo++;
}

void FSN76489ANDevice::DrawPSGStateUI()
{
	if (pPSGState == nullptr)
		return;

	ImGui::Text("Latched Channel: %d (%s)", pPSGState->latch_cs,
		(pPSGState->latch_cs < 4) ? g_SN76489ChannelNames[pPSGState->latch_cs] : "?");
	ImGui::Text("Latched Type: %s", pPSGState->latch_attn ? "Attenuation" : "Tone/Noise");
	ImGui::Separator();

	ImGui::Columns(5, "##psg_channels", false);
	ImGui::SetColumnWidth(0, 100);
	ImGui::SetColumnWidth(1, 80);
	ImGui::SetColumnWidth(2, 80);
	ImGui::SetColumnWidth(3, 80);
	ImGui::SetColumnWidth(4, 100);

	ImGui::Text(""); ImGui::NextColumn();
	for (int i = 0; i < SN76489AN_NUM_CHANNELS; i++)
	{
		ImGui::Text("%s", g_SN76489ChannelNames[i]); ImGui::NextColumn();
	}
	ImGui::Separator();

	// Attenuation with dB
	ImGui::Text("Attn"); ImGui::NextColumn();
	for (int i = 0; i < SN76489AN_NUM_CHANNELS; i++)
	{
		const sn76489an_channel_t* ch = &pPSGState->chn[i];
		if (ch->attn == 15)
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "OFF");
		else
			ImGui::Text("%d (-%ddB)", ch->attn, ch->attn * 2);
		ImGui::NextColumn();
	}

	// Volume bars
	ImGui::Text("Volume"); ImGui::NextColumn();
	for (int i = 0; i < SN76489AN_NUM_CHANNELS; i++)
	{
		const sn76489an_channel_t* ch = &pPSGState->chn[i];
		float vol = (ch->attn == 15) ? 0.0f : (15.0f - (float)ch->attn) / 15.0f;
		ImGui::ProgressBar(vol, ImVec2(-1, 14), "");
		ImGui::NextColumn();
	}

	// Divider / noise type
	ImGui::Text("Divider"); ImGui::NextColumn();
	for (int i = 0; i < SN76489AN_NUM_CHANNELS; i++)
	{
		const sn76489an_channel_t* ch = &pPSGState->chn[i];
		if (ch->type == 0)
		{
			ImGui::Text("%03X (%d)", ch->divider, ch->divider);
		}
		else
		{
			static const char* noise_src[] = { "N/512", "N/1024", "N/2048", "Ch2" };
			ImGui::Text("%s %s", noise_src[ch->noise_div_type],
				ch->noise_type ? "White" : "Periodic");
		}
		ImGui::NextColumn();
	}

	// Frequency (tone channels only)
	ImGui::Text("Frequency"); ImGui::NextColumn();
	for (int i = 0; i < SN76489AN_NUM_CHANNELS; i++)
	{
		const sn76489an_channel_t* ch = &pPSGState->chn[i];
		if (ch->type == 0 && ch->divider >= 2 && ch->attn < 15)
		{
			int freqHz = kPSGTickRate / (2 * ch->divider);
			ImGui::Text("%d Hz", freqHz);
		}
		else
		{
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "-");
		}
		ImGui::NextColumn();
	}

	// Timer
	ImGui::Text("Timer"); ImGui::NextColumn();
	for (int i = 0; i < SN76489AN_NUM_CHANNELS; i++)
	{
		ImGui::Text("%04X", pPSGState->chn[i].timer);
		ImGui::NextColumn();
	}

	// Output
	ImGui::Text("Output"); ImGui::NextColumn();
	for (int i = 0; i < SN76489AN_NUM_CHANNELS; i++)
	{
		const sn76489an_channel_t* ch = &pPSGState->chn[i];
		if (ch->attn == 15)
		{
			ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "-");
		}
		else
		{
			ImGui::TextColored(
				ch->output ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
				"%s", ch->output ? "HIGH" : "LOW");
		}
		ImGui::NextColumn();
	}

	ImGui::Columns();
	ImGui::Separator();

	// Noise LFSR
	const sn76489an_channel_t* noise = &pPSGState->chn[3];
	ImGui::Text("Noise LFSR: %04X", noise->shift_reg);

	// Draw LFSR bits as colored blocks
	ImGui::Text("LFSR Bits:  ");
	ImGui::SameLine();
	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	ImVec2 pos = ImGui::GetCursorScreenPos();
	const float bw = 10.0f, bh = 14.0f, gap = 1.0f;
	for (int b = 15; b >= 0; b--)
	{
		bool bit = (noise->shift_reg >> b) & 1;
		ImU32 col = bit ? IM_COL32(80, 220, 80, 255) : IM_COL32(60, 60, 60, 255);
		draw_list->AddRectFilled(pos, ImVec2(pos.x + bw, pos.y + bh), col);
		pos.x += bw + gap;
	}
	ImGui::Dummy(ImVec2(16 * (bw + gap), bh + 4));
}

void FSN76489ANDevice::DrawPSGLogUI()
{
	if (pPSGState == nullptr)
		return;

	ImGui::Text("Write Log (last %d writes):", kWriteBufferSize);
	ImGui::Separator();

	ImGui::Columns(4, "##psg_log", true);
	ImGui::SetColumnWidth(0, 60);
	ImGui::SetColumnWidth(1, 80);
	ImGui::SetColumnWidth(2, 60);
	ImGui::SetColumnWidth(3, 200);
	ImGui::Text("Frame"); ImGui::NextColumn();
	ImGui::Text("PC"); ImGui::NextColumn();
	ImGui::Text("Data"); ImGui::NextColumn();
	ImGui::Text("Decoded"); ImGui::NextColumn();
	ImGui::Separator();

	// Display most recent entries first
	int idx = WriteBufferDisplayIndex;
	for (int n = 0; n < kWriteBufferSize; n++)
	{
		const FSN76489ANWrite& w = WriteBuffer[idx];
		if (w.FrameNo == 0 && w.Data == 0 && idx != 0)
		{
			// Skip empty entries
			idx--;
			if (idx < 0) idx = kWriteBufferSize - 1;
			continue;
		}

		ImGui::Text("%d", w.FrameNo); ImGui::NextColumn();
		ImGui::Text("%04X", w.PC.Address); ImGui::NextColumn();
		ImGui::Text("%02X", w.Data); ImGui::NextColumn();

		// Decode the write
		if (w.Data & 0x80)
		{
			int ch = (w.Data >> 5) & 0x03;
			bool isAttn = (w.Data >> 4) & 0x01;
			if (isAttn)
			{
				int attn = w.Data & 0x0F;
				if (attn == 15)
					ImGui::Text("%s: Attn OFF", g_SN76489ChannelNames[ch]);
				else
					ImGui::Text("%s: Attn %d (-%ddB)", g_SN76489ChannelNames[ch], attn, attn * 2);
			}
			else
			{
				if (ch == 3)
					ImGui::Text("Noise: type=%d div=%d", (w.Data >> 2) & 1, w.Data & 0x03);
				else
					ImGui::Text("%s: Tone low=%X", g_SN76489ChannelNames[ch], w.Data & 0x0F);
			}
		}
		else
		{
			ImGui::Text("Data: %02X (hi bits)", w.Data & 0x3F);
		}
		ImGui::NextColumn();

		idx--;
		if (idx < 0) idx = kWriteBufferSize - 1;
	}
	ImGui::Columns();
}

void FSN76489ANDevice::DrawPSGGraphUI()
{
	if (pPSGState == nullptr)
		return;

	// Calculate current frequencies
	float chanAFreq = 0, chanBFreq = 0, chanCFreq = 0, noiseFreq = 0;

	for (int i = 0; i < 3; i++)
	{
		const sn76489an_channel_t* ch = &pPSGState->chn[i];
		float freq = 0;
		if (ch->divider >= 2 && ch->attn < 15)
			freq = (float)kPSGTickRate / (2.0f * ch->divider);

		if (i == 0) chanAFreq = freq;
		else if (i == 1) chanBFreq = freq;
		else chanCFreq = freq;
	}

	// Noise: approximate rate
	const sn76489an_channel_t* noise = &pPSGState->chn[3];
	if (noise->attn < 15)
	{
		if (noise->noise_div_type == 3)
		{
			if (pPSGState->chn[2].divider >= 2)
				noiseFreq = (float)kPSGTickRate / (2.0f * pPSGState->chn[2].divider);
		}
		else
		{
			noiseFreq = (float)kPSGTickRate / (2.0f * (0x10 << noise->noise_div_type));
		}
	}

	const float freqMin = 0.0f;
	const float freqMax = 12000.0f;

	ImGui::Text("Tone A: %.0f Hz", chanAFreq);
	ImGui::PlotLines("##ToneA", ChanAValues, kNoValues, GraphOffset,
		"Tone A", freqMin, freqMax, ImVec2(0, 60));

	ImGui::Text("Tone B: %.0f Hz", chanBFreq);
	ImGui::PlotLines("##ToneB", ChanBValues, kNoValues, GraphOffset,
		"Tone B", freqMin, freqMax, ImVec2(0, 60));

	ImGui::Text("Tone C: %.0f Hz", chanCFreq);
	ImGui::PlotLines("##ToneC", ChanCValues, kNoValues, GraphOffset,
		"Tone C", freqMin, freqMax, ImVec2(0, 60));

	ImGui::Text("Noise: %.0f Hz", noiseFreq);
	ImGui::PlotLines("##Noise", NoiseValues, kNoValues, GraphOffset,
		"Noise", freqMin, freqMax, ImVec2(0, 60));

	// Update graph ring buffer
	ChanAValues[GraphOffset] = chanAFreq;
	ChanBValues[GraphOffset] = chanBFreq;
	ChanCValues[GraphOffset] = chanCFreq;
	NoiseValues[GraphOffset] = noiseFreq;
	GraphOffset = (GraphOffset + 1) % kNoValues;
}

void FSN76489ANDevice::DrawDetailsUI()
{
	if (pPSGState == nullptr)
		return;

	if (ImGui::BeginTabBar("PSGDetailsTabBar"))
	{
		if (ImGui::BeginTabItem("State"))
		{
			DrawPSGStateUI();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Graphs"))
		{
			DrawPSGGraphUI();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Log"))
		{
			DrawPSGLogUI();
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}
}
