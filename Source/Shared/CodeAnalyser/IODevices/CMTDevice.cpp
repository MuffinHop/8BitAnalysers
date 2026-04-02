#include "CMTDevice.h"
#include <Misc/EmuBase.h>
#include <imgui.h>
#include <string.h>
#include <stdio.h>

static const char* g_StateNames[] = {
	"Empty", "Stopped", "Playing", "Recording"
};

static const char* g_PhaseNames[] = {
	"Leader", "Sync", "Data", "Checksum", "Gap", "Done"
};

static const char* g_FileTypeNames[] = {
	"Binary", "MZ-80 Basic", "Data", "MZ-700 obj", "MZ-800 obj", "Unknown"
};

static const char* _GetFileType(uint8_t ftype) {
	if (ftype == 0x01) return g_FileTypeNames[0];
	if (ftype == 0x02 || ftype == 0x05) return g_FileTypeNames[1];
	if (ftype == 0x03) return g_FileTypeNames[2];
	if (ftype == 0x04) return g_FileTypeNames[3];
	if (ftype == 0x05) return g_FileTypeNames[4];
	return g_FileTypeNames[5];
}

FCMTDevice::FCMTDevice()
{
	Name = "CMT Tape";
	memset(SignalValues, 0, sizeof(SignalValues));
	memset(ProgressValues, 0, sizeof(ProgressValues));
}

bool FCMTDevice::Init(const char* pName, FEmuBase* pEmulator, cmt_t* pCMT)
{
	Name = pName;
	pCMTState = pCMT;

	SetAnalyser(&pEmulator->GetCodeAnalysis());
	pEmulator->GetCodeAnalysis().IOAnalyser.AddDevice(this);

	return true;
}

void FCMTDevice::OnFrameTick()
{
}

void FCMTDevice::OnMachineFrameEnd()
{
	if (!pCMTState) return;

	SignalValues[GraphOffset] = (float)pCMTState->read_data;
	ProgressValues[GraphOffset] = cmt_get_progress(pCMTState);
	GraphOffset = (GraphOffset + 1) % kSignalLen;
	FrameNo++;
}

void FCMTDevice::DrawStateUI()
{
	if (!pCMTState) return;
	const cmt_t& cmt = *pCMTState;

	ImGui::Text("State: %s", g_StateNames[cmt.state]);
	ImGui::Text("Motor: %s", cmt.motor_on ? "ON" : "OFF");
	ImGui::Separator();

	if (cmt.has_file) {
		char fname[20];
		cmt_get_filename(&cmt, fname, sizeof(fname));

		ImGui::Text("File: %s", fname);
		ImGui::Text("Type: 0x%02X (%s)", cmt.header[0], _GetFileType(cmt.header[0]));

		uint16_t fsize = cmt.header[0x12] | ((uint16_t)cmt.header[0x13] << 8);
		uint16_t fstrt = cmt.header[0x14] | ((uint16_t)cmt.header[0x15] << 8);
		uint16_t fexec = cmt.header[0x16] | ((uint16_t)cmt.header[0x17] << 8);

		ImGui::Text("Size:  %d bytes (0x%04X)", fsize, fsize);
		ImGui::Text("Start: 0x%04X", fstrt);
		ImGui::Text("Exec:  0x%04X", fexec);
		ImGui::Separator();

		// Progress bar
		float progress = cmt_get_progress(&cmt);
		ImGui::ProgressBar(progress, ImVec2(-1, 0), "");
		ImGui::Text("Block: %s", cmt.current_block == 0 ? "Header" : "Data");
		ImGui::Text("Phase: %s", g_PhaseNames[cmt.phase]);

		if (cmt.state == CMT_STATE_PLAYING) {
			ImGui::Text("Byte:  %d / %d", cmt.byte_pos, cmt.byte_total);
			ImGui::Text("Bit:   %d", cmt.bit_pos);
			ImGui::Text("Output: %d", cmt.read_data);
		}
	} else {
		ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No tape loaded");
	}

	ImGui::Separator();
	ImGui::Text("Read Data (PC5): %d", cmt.read_data);
}

void FCMTDevice::DrawSignalUI()
{
	if (!pCMTState) return;

	ImGui::Text("Output Signal (PC5)");
	ImGui::PlotLines("##signal", SignalValues, kSignalLen, GraphOffset,
		NULL, 0.0f, 1.0f, ImVec2(0, 60));

	ImGui::Text("Playback Progress");
	ImGui::PlotLines("##progress", ProgressValues, kSignalLen, GraphOffset,
		NULL, 0.0f, 1.0f, ImVec2(0, 40));
}

void FCMTDevice::DrawDetailsUI()
{
	if (!pCMTState) {
		ImGui::Text("CMT not initialized");
		return;
	}

	if (ImGui::BeginTabBar("CMTTabs")) {
		if (ImGui::BeginTabItem("State")) {
			DrawStateUI();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Signal")) {
			DrawSignalUI();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}
