#include "BanksViewer.h"

#include <imgui.h>

#include "../PCEEmu.h"
//#include <geargrafx_core.h>


static const char* BankAccessToString(EBankAccess access)
{
	switch (access)
	{
		case EBankAccess::Read:       return "R";
		case EBankAccess::Write:      return "W";
		case EBankAccess::ReadWrite:  return "RW";
		default:                      return "-";
	}
}

enum class EBankTableColumn : int
{
	Name = 0,
	Access,
	Address,
	EverMapped,
};

static void SortBankTable(const ImGuiTableSortSpecs* sortSpecs,	const std::vector<FCodeAnalysisBank*>& banks,	std::vector<int>& sortedIndices)
{
	if (!sortSpecs || sortSpecs->SpecsCount == 0)
		return;

	const ImGuiTableColumnSortSpecs& spec = sortSpecs->Specs[0];

	auto Compare = [&](int lhs, int rhs)
		{
			const FCodeAnalysisBank* A = banks[lhs];
			const FCodeAnalysisBank* B = banks[rhs];
			int delta = 0;

			switch ((EBankTableColumn)spec.ColumnIndex)
			{
			case EBankTableColumn::Name:
				delta = A->Name.compare(B->Name);
				break;

			case EBankTableColumn::Access:
				delta = int(A->Mapping) - int(B->Mapping);
				break;

			case EBankTableColumn::Address:
				delta = int(A->GetMappedAddress()) - int(B->GetMappedAddress());
				break;

			case EBankTableColumn::EverMapped:
				delta = int(A->bEverBeenMapped) - int(B->bEverBeenMapped);
				break;
			}

			if (spec.SortDirection == ImGuiSortDirection_Descending)
				delta = -delta;

			return delta < 0;
		};

	std::sort(sortedIndices.begin(), sortedIndices.end(), Compare);
}

void FBanksViewer::DrawBankTable(const std::vector<FCodeAnalysisBank*>& banks)
{
	static std::vector<int> sortedIndices;

	// Initialize index list once or if size changes
	if (sortedIndices.size() != banks.size())
	{
		sortedIndices.resize(banks.size());
		for (int i = 0; i < (int)banks.size(); ++i)
			sortedIndices[i] = i;
	}

	ImGuiTableFlags flags =
		ImGuiTableFlags_Borders |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_Reorderable |
		ImGuiTableFlags_Sortable |
		ImGuiTableFlags_SortMulti |
		ImGuiTableFlags_Hideable |
		ImGuiTableFlags_ScrollY;

	if (ImGui::BeginTable("MemoryBanksTable", 4, flags))
	{
		ImGui::TableSetupScrollFreeze(0, 1);

		ImGui::TableSetupColumn("Name",
			ImGuiTableColumnFlags_DefaultSort,
			0.0f,
			(int)EBankTableColumn::Name);

		ImGui::TableSetupColumn("Access",
			ImGuiTableColumnFlags_PreferSortDescending,
			0.0f,
			(int)EBankTableColumn::Access);

		ImGui::TableSetupColumn("Mapped Address",
			ImGuiTableColumnFlags_PreferSortDescending,
			0.0f,
			(int)EBankTableColumn::Address);

		ImGui::TableSetupColumn("Ever Mapped",
			ImGuiTableColumnFlags_PreferSortDescending,
			0.0f,
			(int)EBankTableColumn::EverMapped);

		ImGui::TableHeadersRow();

		// Handle sorting
		if (ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs())
		{
			if (sortSpecs->SpecsDirty)
			{
				SortBankTable(sortSpecs, banks, sortedIndices);
				sortSpecs->SpecsDirty = false;
			}
		}

		// Draw rows
		for (int idx : sortedIndices)
		{
			const FCodeAnalysisBank* bank = banks[idx];

			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(bank->Name.c_str());

			ImGui::TableSetColumnIndex(1);
			ImGui::TextUnformatted(BankAccessToString(bank->Mapping));

			ImGui::TableSetColumnIndex(2);
			ImGui::Text("0x%04X", bank->GetMappedAddress());

			ImGui::TableSetColumnIndex(3);
			ImGui::TextUnformatted(bank->bEverBeenMapped ? "Y" : "N");
		}

		ImGui::EndTable();
	}
}

FBanksViewer::FBanksViewer(FEmuBase* pEmu)
: FViewerBase(pEmu) 
{ 
	Name = "Banks";
	pPCEEmu = static_cast<FPCEEmu*>(pEmu);
}

bool FBanksViewer::Init()
{
	return true;
}

void FBanksViewer::DrawUI()
{
	FCodeAnalysisState& state = pPCEEmu->GetCodeAnalysis();

	std::vector<FCodeAnalysisBank*> banksToView;
	//if (!pMedia->IsCDROM())

	for (int i = 0; i < 0x80; i++)
	{
		const int16_t bankId = pPCEEmu->Banks[i]->GetBankId();
		if (FCodeAnalysisBank* pBank = state.GetBank(bankId))
		{
			if (std::find(banksToView.begin(), banksToView.end(), pBank) == banksToView.end())
			{
				banksToView.push_back(pBank);
			}
		}
	}

	DrawBankTable(banksToView);
}
