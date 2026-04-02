#include "WD2793Device.h"
#include <Misc/EmuBase.h>
#include <imgui.h>
#include <string.h>
#include <stdio.h>

static const char* g_PortNames[] = {
	"CMD/STATUS", "TRACK", "SECTOR", "DATA",
	"MOTOR", "SIDE", "DENSITY", "EINT"
};

static const char* _DecodeCommand(uint8_t cmd) {
	if (cmd & 0x80) {
		if ((cmd >> 4) == 0x0F) return "RESTORE";
		if ((cmd >> 4) == 0x0E) return "SEEK";
		if ((cmd >> 5) == 0x05) return "STEP IN";
		if ((cmd >> 5) == 0x04) return "STEP OUT";
		return "TYPE I (?)";
	}
	if ((cmd >> 6) == 0x01) {
		if ((cmd >> 5) == 0x03) return "READ SECTOR";
		if ((cmd >> 5) == 0x02) return "WRITE SECTOR";
		return "TYPE II (?)";
	}
	if ((cmd >> 4) == 0x03) return "READ ADDRESS";
	if (cmd == 0x0F || cmd == 0x0B) return "WRITE TRACK";
	if (cmd == 0x27 || cmd == 0x2F) return "FORCE INT";
	return "UNKNOWN";
}

static const char* _DecodeStatus(uint8_t status) {
	static char buf[128];
	buf[0] = 0;
	if (status & WD2793_STATUS_BUSY)      strcat(buf, "BUSY ");
	if (status & WD2793_STATUS_DRQ)       strcat(buf, "DRQ ");
	if (status & WD2793_STATUS_TRK00)     strcat(buf, "TRK00 ");
	if (status & WD2793_STATUS_CRC_ERR)   strcat(buf, "CRC ");
	if (status & WD2793_STATUS_SEEK_ERR)  strcat(buf, "SEEK ");
	if (status & WD2793_STATUS_NOT_READY) strcat(buf, "!RDY ");
	if (buf[0] == 0) strcpy(buf, "OK");
	return buf;
}

FWD2793Device::FWD2793Device()
{
	Name = "WD2793 FDC";
	memset(WriteBuffer, 0, sizeof(WriteBuffer));
}

bool FWD2793Device::Init(const char* pName, FEmuBase* pEmulator, wd2793_t* pFDC)
{
	Name = pName;
	pFDCState = pFDC;

	SetAnalyser(&pEmulator->GetCodeAnalysis());
	pEmulator->GetCodeAnalysis().IOAnalyser.AddDevice(this);

	return true;
}

void FWD2793Device::WriteFDC(FAddressRef pc, uint16_t port, uint8_t data)
{
	FWD2793Write& w = WriteBuffer[WriteBufferWriteIndex];
	w.PC = pc;
	w.FrameNo = FrameNo;
	w.Port = port;
	w.Data = data;
	w.RegTrack = pFDCState->reg_track;
	w.RegSector = pFDCState->reg_sector;
	w.RegStatus = pFDCState->reg_status;
	w.Motor = pFDCState->motor;
	w.Side = pFDCState->side;
	WriteBufferWriteIndex = (WriteBufferWriteIndex + 1) % kWriteBufferSize;
}

void FWD2793Device::OnFrameTick()
{
}

void FWD2793Device::OnMachineFrameEnd()
{
	FrameNo++;
}

void FWD2793Device::DrawStateUI()
{
	if (!pFDCState) return;
	const wd2793_t& fdc = *pFDCState;

	ImGui::Text("Connected: %s", fdc.connected ? "Yes" : "No");
	ImGui::Separator();

	ImGui::Text("Command:    0x%02X (%s)", fdc.reg_command, _DecodeCommand(fdc.reg_command));
	ImGui::Text("Status:     0x%02X (%s)", fdc.reg_status, _DecodeStatus(fdc.reg_status));
	ImGui::Text("Track:      %d (0x%02X)", fdc.reg_track, fdc.reg_track);
	ImGui::Text("Sector:     %d (0x%02X)", fdc.reg_sector, fdc.reg_sector);
	ImGui::Text("Data:       0x%02X", fdc.reg_data);
	ImGui::Separator();

	int drv = fdc.motor & 0x03;
	ImGui::Text("Drive:      %d", drv);
	ImGui::Text("Motor:      %s (0x%02X)", (fdc.motor & 0x80) ? "ON" : "OFF", fdc.motor);
	ImGui::Text("Side:       %d", fdc.side);
	ImGui::Text("Density:    %s", fdc.density ? "Double" : "Single");
	ImGui::Text("EINT:       %s", fdc.eint ? "Enabled" : "Disabled");
	ImGui::Separator();

	ImGui::Text("Data Counter: %d", fdc.data_counter);
	ImGui::Text("Buffer Pos:   %d", fdc.buffer_pos);
	ImGui::Text("Multiblock:   %s", fdc.multiblock ? "Yes" : "No");
	ImGui::Text("Status Script: %d", fdc.status_script);
	ImGui::Text("INT Pending:   %s", fdc.int_pending ? "Yes" : "No");
}

void FWD2793Device::DrawDrivesUI()
{
	if (!pFDCState) return;

	for (int i = 0; i < WD2793_NUM_DRIVES; i++) {
		const wd2793_drive_t& d = pFDCState->drive[i];
		char label[32];
		snprintf(label, sizeof(label), "FD%d", i);

		if (ImGui::TreeNode(label)) {
			if (d.dsk_present) {
				ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Disk inserted");
				ImGui::Text("Image size:  %u bytes", d.dsk_size);
				ImGui::Text("Track:       %d", d.current_track);
				ImGui::Text("Side:        %d", d.current_side);
				ImGui::Text("Sector:      %d", d.current_sector);
				ImGui::Text("Sector size: %d", d.sector_size);
				ImGui::Text("Track offset: 0x%06X", d.track_offset);
			} else {
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Empty");
			}
			ImGui::TreePop();
		}
	}
}

void FWD2793Device::DrawLogUI()
{
	if (ImGui::Button("Clear Log")) {
		memset(WriteBuffer, 0, sizeof(WriteBuffer));
		WriteBufferWriteIndex = 0;
		WriteBufferDisplayIndex = 0;
	}

	ImGui::BeginChild("##fdc_log", ImVec2(0, 0), true);
	if (ImGui::BeginTable("FDCLog", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
		ImGui::TableSetupColumn("Frame", ImGuiTableColumnFlags_WidthFixed, 50.0f);
		ImGui::TableSetupColumn("PC", ImGuiTableColumnFlags_WidthFixed, 50.0f);
		ImGui::TableSetupColumn("Port", ImGuiTableColumnFlags_WidthFixed, 80.0f);
		ImGui::TableSetupColumn("Data", ImGuiTableColumnFlags_WidthFixed, 40.0f);
		ImGui::TableSetupColumn("Trk", ImGuiTableColumnFlags_WidthFixed, 30.0f);
		ImGui::TableSetupColumn("Sec", ImGuiTableColumnFlags_WidthFixed, 30.0f);
		ImGui::TableSetupColumn("Decoded");
		ImGui::TableHeadersRow();

		for (int i = 0; i < kWriteBufferSize; i++) {
			int idx = (WriteBufferWriteIndex - 1 - i + kWriteBufferSize) % kWriteBufferSize;
			const FWD2793Write& w = WriteBuffer[idx];
			if (w.FrameNo == 0 && w.Port == 0) continue;

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%d", w.FrameNo);
			ImGui::TableNextColumn();
			ImGui::Text("%04X", w.PC.Address);
			ImGui::TableNextColumn();

			uint8_t portOff = w.Port & 0x07;
			ImGui::Text("D%X %s", 8 + portOff, g_PortNames[portOff]);
			ImGui::TableNextColumn();
			ImGui::Text("%02X", w.Data);
			ImGui::TableNextColumn();
			ImGui::Text("%02X", w.RegTrack);
			ImGui::TableNextColumn();
			ImGui::Text("%02X", w.RegSector);
			ImGui::TableNextColumn();

			if (portOff == WD2793_PORT_CMD_STATUS) {
				ImGui::Text("%s", _DecodeCommand(w.Data));
			} else if (portOff == WD2793_PORT_MOTOR) {
				ImGui::Text("DRV%d %s", w.Data & 3, (w.Data & 0x80) ? "ON" : "OFF");
			} else if (portOff == WD2793_PORT_SIDE) {
				ImGui::Text("Side %d", w.Data & 1);
			} else if (portOff == WD2793_PORT_EINT) {
				ImGui::Text("INT %s", (w.Data & 1) ? "ENA" : "DIS");
			} else if (portOff == WD2793_PORT_TRACK) {
				ImGui::Text("Track=%d", (uint8_t)~w.Data);
			} else if (portOff == WD2793_PORT_SECTOR) {
				ImGui::Text("Sector=%d", (uint8_t)~w.Data);
			} else if (portOff == WD2793_PORT_DATA) {
				ImGui::Text("Data=~%02X", (uint8_t)~w.Data);
			}
		}
		ImGui::EndTable();
	}
	ImGui::EndChild();
}

void FWD2793Device::DrawDetailsUI()
{
	if (!pFDCState) {
		ImGui::Text("FDC not initialized");
		return;
	}

	if (ImGui::BeginTabBar("FDCTabs")) {
		if (ImGui::BeginTabItem("State")) {
			DrawStateUI();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Drives")) {
			DrawDrivesUI();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Log")) {
			DrawLogUI();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}
