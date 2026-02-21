#include "CodeAnalyser/AssemblerExport.h"
#include "CodeAnalyser/6502/HuC6280Disassembler.h"
#include "PCEEmu.h"

class FPCEAsmExporterBase : public FASMExporter
{
	public:
		void ProcessLabelsOutsideExportedRange(void) override
		{
			if (!DasmState.LabelsOutsideRange.empty())
			{
				std::set<FAddressRef> labels;

				for (auto labelAddrRef : DasmState.LabelsOutsideRange)
				{
					bool bInRange = false;
					for (const FExportRange& range : ExportRanges)
					{
						const uint16_t labelAddr = labelAddrRef.GetAddress();
						if (labelAddr >= range.Min && labelAddr <= range.Max)
						{
							bInRange = true;
							break;
						}
					}
					if (!bInRange)
					{
						labels.insert(labelAddrRef);
					}
				}

				FCodeAnalysisState& state = pEmulator->GetCodeAnalysis();
				SetOutputToHeader();

				Output("\n; Labels\n");

				for (auto labelAddr : labels)
				{
					const FLabelInfo* pLabelInfo = state.GetLabelForAddress(labelAddr);
					if (pLabelInfo)
						Output("%s: \t%s %s\n", pLabelInfo->GetName(), Config.EQUText, NumStr(labelAddr.GetAddress()));
					else
						LOGINFO("Can't get label for address 0x%x", labelAddr);
				}

				Output("\n");
			}
		}
		void	ExportDidEnd() override
		{
			// Reset disassembler back to default settings
			FHuC6280DisassemblerConfig& config = GetHuC6280DisassemblerConfig();
			config = GetHuC6280DisassemblerDefaultConfig();

			// Update every code item to refresh the disassembly back to the code analysis disassembly.
			FCodeAnalysisState& state = pEmulator->GetCodeAnalysis();
			int addr = 0x2000;
			while (addr <= 0xffff)
			{
				if (const FCodeInfo* pCodeInfo = state.GetCodeInfoForPhysicalAddress(addr))
				{
					UpdateCodeInfoForAddress(state, addr);
					addr += pCodeInfo->ByteSize;
				}
				else
				{
					addr++;
				}
			}
		}
};

class FPCEASAssemblerExporter : public FPCEAsmExporterBase
{
public:
	FPCEASAssemblerExporter()
	{
		Config.DataBytePrefix = "db";
		Config.DataWordPrefix = "dw";
		Config.DataTextPrefix = "db";
		Config.ORGText = "\t.org";
		Config.EQUText = ".equ";

		// PCEAS doesn't support labels with the function as a prefix, eg. EntryPoint.loop
		// Where EntryPoint is a function and loop is a local label in the EntryPoint function.
		Config.LocalLabelPrefix = "";
		Config.bUseLocalLabelPrefix = false;
	}

	void	ExportDidBegin() override
	{
		FHuC6280DisassemblerConfig& config = GetHuC6280DisassemblerConfig();
		config.BrOp = '[';
		config.BrCl = ']';
		config.ZpPr = ZeroPagePrefix.c_str();
		config.MprIndexMode = true;
	}

	void AddHeader(void) override
	{
		// needed?
		//Output("\t.cpu 6280\n");
	}
	void AddBankSection(const FCodeAnalysisBank* pBank) override
	{
		FASMExporter::AddBankSection(pBank);

		FPCEEmu* pPCEEmu = static_cast<FPCEEmu*>(pEmulator);
		const uint8_t bankIndex = pPCEEmu->GetBankIndexForBankId(pBank->Id);
		Output("\t.bank %d\n", bankIndex);

		if (bankIndex == 0xff)
			LOGERROR("Could not lookup bank index for bank id %d", pBank->Id);
	}
	std::string ZeroPagePrefix = "<";
};

bool InitPCEAsmExporters()
{
	AddAssemblerExporter("PCEAS", new FPCEASAssemblerExporter);
	return true;
}