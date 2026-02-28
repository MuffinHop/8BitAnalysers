#include "DebugStatsViewer.h"

#include <imgui.h>

#include "../PCEEmu.h"
#include "../GameDb.h"
#include "BatchGameLoadViewer.h"

#include <geargrafx_core.h>


FDebugStatsViewer::FDebugStatsViewer(FEmuBase* pEmu)
: FViewerBase(pEmu) 
{ 
	Name = "Debug Stats";
	pPCEEmu = static_cast<FPCEEmu*>(pEmu);
}

bool FDebugStatsViewer::Init()
{
	return true;
}

std::string GetBankType(Memory* pMemory, int bankIndex)
{
	switch (pMemory->GetBankType(bankIndex))
	{
	case Memory::MEMORY_BANK_TYPE_ROM:
		return "ROM";
	case Memory::MEMORY_BANK_TYPE_BIOS:
		return "BIOS";
	case Memory::MEMORY_BANK_TYPE_CARD_RAM:
		return "CARD RAM";
	case Memory::MEMORY_BANK_TYPE_BACKUP_RAM:
		return "BACKUP RAM";
	case Memory::MEMORY_BANK_TYPE_WRAM:
		return "WORK RAM";
	case Memory::MEMORY_BANK_TYPE_CDROM_RAM:
		return "CD ROM RAM";
	case Memory::MEMORY_BANK_TYPE_UNUSED:
		return "UNUSED RAM";
	}
	return "UNKNOWN";
}

void FDebugStatsViewer::DrawUI()
{
	FCodeAnalysisState& state = pPCEEmu->GetCodeAnalysis();

	int mappedBanks = 0;
	int banksWithPrimaryMappedPage = 0;
	int usedBanks = 0;

	for (int b = 0; b < FCodeAnalysisState::BankCount; b++)
	{
		FCodeAnalysisBank& bank = state.GetBanks()[b];
	
		if (bank.PrimaryMappedPage != -1)
			banksWithPrimaryMappedPage++;
		if (bank.IsMapped())
			mappedBanks++;
		if (bank.IsUsed())
			usedBanks++;
	}

	int pagesInUse = 0;
	for (int p = 0; p < state.GetNoPages(); p++)
	{
		FCodeAnalysisPage* pPage = state.GetPage(p);
		if (pPage->bUsed)
			pagesInUse++;
	}

	int maxDupeBanks = 0;
	std::string gameWithMaxDupes;
	for (auto pair : pPCEEmu->DebugStats.GameDebugStats)
	{
		const FGameDebugStats& debugStats = pair.second;
		if (debugStats.NumDupeBanks > maxDupeBanks)
		{
			maxDupeBanks = debugStats.NumDupeBanks;
			gameWithMaxDupes = pair.first;
		}
	}

	const int romSize = pPCEEmu->GetMedia()->IsCDROM() ? GG_BIOS_SYSCARD_SIZE : pPCEEmu->GetMedia()->GetROMSize();
	const int romBankCount = (romSize / 0x2000) + (romSize % 0x2000 ? 1 : 0);

	ImGui::SeparatorText("Banks");
	ImGui::Text("Total banks: %d", FCodeAnalysisState::BankCount);
	ImGui::Text("Mapped banks: %d", mappedBanks);
	ImGui::Text("Banks with primary mapped page: %d", banksWithPrimaryMappedPage);
	ImGui::Text("Used banks: %d", usedBanks);
	ImGui::Text("Total pages: %d", state.GetNoPages());
	ImGui::Text("Pages in use: %d", pagesInUse);
	ImGui::Text("Game with most dupe banks: %s", gameWithMaxDupes.c_str());
	ImGui::Text("Max dupe banks: %d", maxDupeBanks);
	ImGui::Text("Num bank sets: %d", pPCEEmu->kNumBankSetIds);
	ImGui::Text("Bank switches per frame: %d", pPCEEmu->DebugStats.NumBankSwitchesThisFrame);

	if (ImGui::TreeNode("Game Stats"))
	{
		if (ImGui::Button("Reset"))
		{
			pPCEEmu->DebugStats.Reset();
		}

		bool bDump = false;
		if (ImGui::Button("Dump"))
		{
			bDump = true;
		}

		for (auto pair : pPCEEmu->DebugStats.GameDebugStats)
		{
			const FGameDebugStats& gameStats = pair.second;
			const ImVec4 txtColour = gameStats.NumBanksMapped == gameStats.NumBanks ? ImVec4(0.f, 0.75f, 0.f, 1.0f) : ImVec4(1.f, 1.0f, 1.f, 1.0f);
			ImGui::Text("%s", pair.first.c_str());
			ImGui::Text("  Num Banks:        %d", gameStats.NumBanks);
			ImGui::TextColored(txtColour, "  Num Banks Mapped: %d / %d", gameStats.NumBanksMapped, gameStats.NumBanks);
			ImGui::Text("  Num Dupe Banks:   %d", gameStats.NumDupeBanks);
			ImGui::Text("  Max Bank Switches:   %d", gameStats.MaxBankSwitches);

			if (bDump)
			{
				LOGINFO("%s", pair.first.c_str());
				LOGINFO("  Num Banks:        %d", gameStats.NumBanks);
				LOGINFO("  Num Banks Mapped: %d/%d (%.2f%%)", gameStats.NumBanksMapped, gameStats.NumBanks, ((float)gameStats.NumBanksMapped / (float)gameStats.NumBanks) * 100.f);
				LOGINFO("  Num Dupe Banks:   %d", gameStats.NumDupeBanks);
			}

			if (gameStats.NumBanksMapped == gameStats.NumBanks)
			{
				std::map<std::string, float>::iterator it = MappedGames.find(pair.first);
				if (it == MappedGames.end())
				{
					MappedGames[pair.first] = pPCEEmu->GetBatchGameLoadViewer()->GetElapsedGameRunTime();
				}
			}
		}

		if (ImGui::TreeNode("Games with all banks mapped"))
		{
			if (bDump)
			{
				LOGINFO("Mapped games");
			}

			for (auto pair : MappedGames)
			{
				ImGui::Text("%s [%.2f secs]", pair.first.c_str(), pair.second);

				if (bDump)
				{
					LOGINFO("  %s [%.2f secs]", pair.first.c_str(), pair.second);
				}
			}
			ImGui::TreePop();
		}

		ImGui::TreePop();
	}
	
	if (ImGui::TreeNode("Games Bank mappings"))
	{
		TGameBankMappings& bankMappings = GetBankMappings();
		for (const auto it : bankMappings)
		{
			if (ImGui::TreeNode(it.first.c_str()))
			{
				ImGui::Text("%s", it.first.c_str());

				for (int i = 0; i < bankMappings[it.first].size(); i++)
				{
					ImGui::Text("  %02d 0x%04x", i, bankMappings[it.first][i]);
				}
				ImGui::TreePop();
			}
		}
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Bank list"))
	{
		constexpr ImVec4 redColour(1.0f, 0.0f, 0.0f, 1.0f);
		constexpr ImVec4 whiteColour(1.0f, 1.0f, 1.0f, 1.0f);
		constexpr ImVec4 yellowColour(1.0f, 1.0f, 0.0f, 1.0f);
		for (int i = 0; i < 256; i++)
		{
			if (const FCodeAnalysisBank* pBank = state.GetBank(pPCEEmu->Banks[i]->GetBankId(0)))
			{
				const uint8_t* gearGfxMem = pPCEEmu->GetMemory()->GetMemoryMap()[i];

				int index = -1;
				const Memory::MemoryBankType bankType = pPCEEmu->GetMemory()->GetBankType(i);
				if (bankType == Memory::MEMORY_BANK_TYPE_ROM || bankType == Memory::MEMORY_BANK_TYPE_BIOS)
					index = pPCEEmu->GetMedia()->GetRomBankIndex(i);
				else if (bankType == Memory::MEMORY_BANK_TYPE_CARD_RAM)
					index = i - pPCEEmu->GetMemory()->GetCardRAMStart();

				const bool bRam = pPCEEmu->GetMemory()->GetMemoryMapWrite()[i];
				
				ImVec4 colour;
				if (gearGfxMem != pBank->Memory)
					colour = redColour;
				else if ((bRam && pBank->Mapping != EBankAccess::ReadWrite) || (!bRam && pBank->Mapping != EBankAccess::Read))
					colour = yellowColour;
				else
					colour = whiteColour;

				ImGui::TextColored(colour, "%02x '%s' %02d %s '%s' %s",
					i, 
					GetBankType(pPCEEmu->GetMemory(), i).c_str(), 
					index, 
					bRam ? "RW" : "R",
					pBank->Name.c_str(),
					pBank->Mapping == EBankAccess::Read ? "R" : pBank->Mapping == EBankAccess::ReadWrite ? "RW" : "?");
			}
		}
		ImGui::TreePop();
	}

	ImGui::SeparatorText("Items");
	ImGui::Text("Itemlist size: %d", state.ItemList.size());
	ImGui::Text("Global data items size: %d", state.GlobalDataItems.size());
	ImGui::Text("Global functions items size: %d", state.GlobalFunctions.size());

	ImGui::SeparatorText("Rom/Bios");
	ImGui::Text("Type : %s", pPCEEmu->GetMedia()->IsCDROM() ? "BIOS" : "ROM");
	ImGui::Text("Bank count : %d", romBankCount);

	ImGui::SeparatorText("Debugger");
	ImGui::Text("Frame trace size: %d", state.Debugger.GetFrameTrace().size());
	ImGui::Text("Call stack size : %d", state.Debugger.GetCallstack().size());
	
	ImGui::SeparatorText("Analysis State");
	ImGui::Text("Current frame : %d", state.CurrentFrameNo);
	ImGui::Text("Execution counter : %d", state.ExecutionCounter);

	ImGui::SeparatorText("Video");
	ImGui::Text("6270 Hpos: %d", *pPCEEmu->GetCore()->GetHuC6270_1()->GetState()->HPOS);
	ImGui::Text("6270 Vpos: %d", pPCEEmu->GetVPos());
	//ImGui::Text("Raster line: %d", pPCEEmu->GetCore()->GetHuC6270_1()->m_raster_line);

	ImGui::SeparatorText("IO");
	const u8 ioReg = pPCEEmu->GetCore()->GetInput()->GetIORegister();
	ImGui::Text("Register: 0x%x %s", ioReg, NumStr(ioReg, ENumberDisplayMode::Binary));

	ImGui::SeparatorText("Save RAM");
	ImGui::Text("Enabled: %s", pPCEEmu->GetMemory()->IsBackupRamEnabled() ? "Yes" : "No");
	ImGui::Text("In Use: %s", pPCEEmu->GetMemory()->IsBackupRamUsed() ? "Yes" : "No");
}
