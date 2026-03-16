#include "GameDbViewer.h"

#include <imgui.h>

#include "../PCEEmu.h"
#include "../GameDb.h"
//#include "../DebugStats.h"
//#include "BatchGameLoadViewer.h"

//#include <geargrafx_core.h>

enum EGameDbColumns
{
	Col_GameName,
	Col_Overall,
	Col_AssemblesOk,
	Col_RomStatus,
	Col_EmulatorTestOk,
	Col_TestMethodology,
	Col_Count
};

bool GetOverallResult(const FGameDbEntry& entry)
{
	return entry.bAssemblesOk &&
		entry.bRomFileIdentical &&
		entry.bEmulatorTestOk;
}

const char* GetRomStatus(const FGameDbEntry& entry)
{
	if (entry.bRomFileIdentical)
		return "Identical";

	if (entry.bRomFilePartialMatch)
		return "Partial";

	return "Fail";
}

static void SortGameDbTable(std::vector<std::pair<std::string, FGameDbEntry*>>& entries, ImGuiTableSortSpecs* sortSpecs)
{
	if (!sortSpecs || sortSpecs->SpecsCount == 0)
		return;

	const ImGuiTableColumnSortSpecs& spec = sortSpecs->Specs[0];

	std::sort(entries.begin(), entries.end(),
		[&](const auto& A, const auto& B)
		{
			const std::string& nameA = A.first;
			const std::string& nameB = B.first;

			const FGameDbEntry& entryA = *A.second;
			const FGameDbEntry& entryB = *B.second;

			int result = 0;

			switch (spec.ColumnIndex)
			{
			case Col_GameName:
				result = nameA.compare(nameB);
				break;

			case Col_Overall:
				result = (int)GetOverallResult(entryA) - (int)GetOverallResult(entryB);
				break;

			case Col_AssemblesOk:
				result = (int)entryA.bAssemblesOk - (int)entryB.bAssemblesOk;
				break;

			case Col_RomStatus:
			{
				int a = entryA.bRomFileIdentical ? 2 : entryA.bRomFilePartialMatch ? 1 : 0;
				int b = entryB.bRomFileIdentical ? 2 : entryB.bRomFilePartialMatch ? 1 : 0;
				result = a - b;
				break;
			}

			case Col_EmulatorTestOk:
				result = (int)entryA.bEmulatorTestOk - (int)entryB.bEmulatorTestOk;
				break;

			case Col_TestMethodology:
				result = entryA.TestingMethodology - entryB.TestingMethodology;
				break;
			}

			if (spec.SortDirection == ImGuiSortDirection_Descending)
				result = -result;

			return result < 0;
		});
}

FGameDbViewer::FGameDbViewer(FEmuBase* pEmu)
: FViewerBase(pEmu) 
{ 
	Name = "Game Db";
	pPCEEmu = static_cast<FPCEEmu*>(pEmu);
}

bool FGameDbViewer::Init()
{
	return true;
}

void FGameDbViewer::DrawUI()
{
	if (ImGui::Button("Load All"))
	{
		auto findIt = pPCEEmu->GetGamesLists().find("Snapshot File");
		if (findIt != pPCEEmu->GetGamesLists().end())
		{
			const FGamesList& gamesList = findIt->second;
			for (int i = 0; i < gamesList.GetNoGames(); i++)
			{
				const FEmulatorFile& emuFile = gamesList.GetGame(i);
				const std::string fname = "GameDb/" + emuFile.DisplayName + ".json";
				LoadGameDbEntry(emuFile.DisplayName, fname);
			}
		}
	}

	//FCodeAnalysisState& state = pPCEEmu->GetCodeAnalysis();
	TGameDb& gameDb = GetGameDb();

	static std::vector<std::pair<std::string, FGameDbEntry*>> SortedEntries;
	static size_t LastDbSize = 0;

	ImGuiTableFlags flags =
		ImGuiTableFlags_Borders |
		//ImGuiTableFlags_RowBg |
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_Reorderable |
		ImGuiTableFlags_Sortable |
		ImGuiTableFlags_ScrollY;

	if (ImGui::BeginTable("GameDbTable", Col_Count,	flags))
	{
		ImGui::TableSetupScrollFreeze(0, 1);

		ImGui::TableSetupColumn("Game", ImGuiTableColumnFlags_DefaultSort);
		ImGui::TableSetupColumn("Overall");
		ImGui::TableSetupColumn("Assembles");
		ImGui::TableSetupColumn("ROM");
		ImGui::TableSetupColumn("Emu Test");
		ImGui::TableSetupColumn("Test Method");
		ImGui::TableHeadersRow();

		ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs();

		bool rebuild = false;

		if (gameDb.size() != LastDbSize)
		{
			LastDbSize = gameDb.size();
			rebuild = true;
		}

		if (rebuild)
		{
			SortedEntries.clear();
			SortedEntries.reserve(gameDb.size());

			for (auto& it : gameDb)
				SortedEntries.push_back({ it.first, &it.second });
		}

		if (sortSpecs && sortSpecs->SpecsDirty)
		{
			SortGameDbTable(SortedEntries, sortSpecs);
			sortSpecs->SpecsDirty = false;
		}

		for (auto& pair : SortedEntries)
		{
			const std::string& gameName = pair.first;
			const FGameDbEntry& entry = *pair.second;

			bool overall = GetOverallResult(entry);

			ImGui::TableNextRow();

			// ----- Row colouring -----
			ImVec4 rowColor;

			if (overall)
				rowColor = ImVec4(0.f, 0.5f, 0.f, 1.0f);      // green
			else if (entry.bRomFilePartialMatch)
				rowColor = ImVec4(0.5f, 0.5f, 0.f, 1.0f);      // yellow
			else
				rowColor = ImVec4(0.5f, 0.0f, 0.0f, 1.0f);      // red

			ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,	ImGui::ColorConvertFloat4ToU32(rowColor));

			// ----- Columns -----

			ImGui::TableSetColumnIndex(Col_GameName);
			ImGui::TextUnformatted(gameName.c_str());

			ImGui::TableSetColumnIndex(Col_Overall);
			ImGui::TextUnformatted(overall ? "PASS" : "FAIL");

			ImGui::TableSetColumnIndex(Col_AssemblesOk);
			ImGui::TextUnformatted(entry.bAssemblesOk ? "OK" : "Fail");

			ImGui::TableSetColumnIndex(Col_RomStatus);
			ImGui::TextUnformatted(GetRomStatus(entry));

			ImGui::TableSetColumnIndex(Col_EmulatorTestOk);
			ImGui::TextUnformatted(entry.bEmulatorTestOk ? "OK" : "Fail");

			ImGui::TableSetColumnIndex(Col_TestMethodology);

			if (entry.TestingMethodology == -1)
				ImGui::TextUnformatted("-");
			else
				ImGui::Text("%d", entry.TestingMethodology);
		}

		ImGui::EndTable();
	}
}
