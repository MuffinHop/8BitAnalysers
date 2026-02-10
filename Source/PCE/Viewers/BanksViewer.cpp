#include "BanksViewer.h"

#include <imgui.h>

#include "../PCEEmu.h"
#include "CodeAnalyser/UI/CodeAnalyserUI.h"

enum class EBankTableColumn : int
{
	Name = 0,
	Access,
	Address,
	EverMapped,
	Content,
};

enum class EBankContent : int
{
	Data = 0,
	Code,
	Mixed,
	Unknown,
};

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

// todo: get ratio of code/data?
EBankContent GetBankContent(std::vector<FCodeAnalysisItem> items)
{
	bool bFoundCode = false;
	bool bFoundData = false;

	for (const FCodeAnalysisItem& item : items)
	{
		if (item.Item->Type == EItemType::Code)
			bFoundCode = true;
		if (item.Item->Type == EItemType::Data)
			bFoundData = true;
	}

	if (bFoundCode || bFoundData)
	{
		if (bFoundCode)
		{
			return EBankContent::Code;
		}
		return EBankContent::Data;
	}
	return EBankContent::Unknown;
}

static const char* BankContentToString(EBankContent content)
{
	switch (content)
	{
		case EBankContent::Data:		return "Data";
		case EBankContent::Code:		return "Code";
		case EBankContent::Mixed:		return "Mixed";

		case EBankContent::Unknown:
		default:								return "Unknown";
	}
}

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
	FCodeAnalysisState& state = pPCEEmu->GetCodeAnalysis();

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

	if (ImGui::BeginTable("MemoryBanksTable", 5, flags))
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

		ImGui::TableSetupColumn("Content",
			ImGuiTableColumnFlags_PreferSortDescending,
			0.0f,
			(int)EBankTableColumn::Content);

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
			const FCodeAnalysisBank* pBank = banks[idx];

			ImGui::TableNextRow();

			constexpr ImVec4 mappedColour(0.0f, 1.0f, 0.0f, 1.0f);
			constexpr ImVec4 previouslyMappedColour(1.0f, 1.0f, 1.0f, 1.0f);
			constexpr ImVec4 neverMappedColour(0.56f, 0.56f, 0.56f, 1.0f);

			ImVec4 colour = neverMappedColour;
			if (pBank->bEverBeenMapped)
			{
				colour = pBank->Mapping != EBankAccess::None ? mappedColour : previouslyMappedColour;
			}

			ImGui::TableSetColumnIndex(0);
			ImGui::TextColored(colour, pBank->Name.c_str());

			ImGui::TableSetColumnIndex(1);
			ImGui::TextColored(colour, BankAccessToString(pBank->Mapping));

			ImGui::TableSetColumnIndex(2);
			if (pBank->bEverBeenMapped)
			{
				ImGui::TextColored(colour, "%s", NumStr(pBank->GetMappedAddress()));
				const FAddressRef bankAddr(pBank->Id, pBank->GetMappedAddress());
				if (ImGui::IsItemHovered())
				{
					DrawSnippetToolTip(state, state.GetFocussedViewState(), bankAddr, 11);

					// todo make the whole row selectable?
					if (ImGui::IsMouseDoubleClicked(0))
						state.GetFocussedViewState().GoToAddress(bankAddr, false);
				}
			}
			else
			{
				ImGui::TextColored(colour, "----");
			}
			ImGui::TableSetColumnIndex(3);
			ImGui::TextColored(colour, pBank->bEverBeenMapped ? "Y" : "N");

			ImGui::TableSetColumnIndex(4);
			ImGui::TextColored(colour, pBank->bEverBeenMapped ? BankContentToString(GetBankContent(pBank->ItemList)) : "-");
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

	// WRAM
	const int16_t ramBankId = pPCEEmu->Banks[0xf8]->GetBankId();
	if (FCodeAnalysisBank* pBank = state.GetBank(ramBankId))
	{
		banksToView.push_back(pBank);
	}

	DrawBankTable(banksToView);
}
