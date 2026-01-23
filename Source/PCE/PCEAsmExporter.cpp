#include "CodeAnalyser/AssemblerExport.h"
#include "CodeAnalyser/6502/HuC6280Disassembler.h"

class FPCEAsmExporterBase : public FASMExporter
{
	public:
		void ProcessLabelsOutsideExportedRange(void) override
		{
			#if 0
			FCodeAnalysisState& state = pEmulator->GetCodeAnalysis();

			SetOutputToHeader();

			Output("\n\n; ROM Labels\n");

			for (auto labelAddr : DasmState.LabelsOutsideRange)
			{
				const FLabelInfo* pLabelInfo = state.GetLabelForPhysicalAddress(labelAddr);
				if (labelAddr < 0x4000)
					Output("%s: \t%s %s\n", pLabelInfo->GetName(),Config.EQUText, NumStr(labelAddr));
			}

			Output("\n; Screen Memory Labels\n");

			for (auto labelAddr : DasmState.LabelsOutsideRange)
			{
				const FLabelInfo* pLabelInfo = state.GetLabelForPhysicalAddress(labelAddr);
				if (labelAddr >= 0x4000)
					Output("%s: \t%s %s\n", pLabelInfo->GetName(), Config.EQUText, NumStr(labelAddr));
			}

			Output("\n");
			#endif
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
		Config.ORGText = "\torg";
		Config.EQUText = "equ";
		Config.LocalLabelPrefix = ".";
	}

	void	ExportDidBegin() override
	{
		FHuC6280DisassemblerConfig& config = GetHuC6280DisassemblerConfig();
		config.BrOp = '[';
		config.BrCl = ']';
	}

	void	ExportDidEnd() override
	{
		FHuC6280DisassemblerConfig& config = GetHuC6280DisassemblerConfig();
		config = GetHuC6280DisassemblerDefaultConfig();
	}

	void AddHeader(void) override
	{
		// needed?
		//Output("\t.cpu 6280\n");
	}
};

bool InitPCEAsmExporters()
{
	AddAssemblerExporter("PCEAS", new FPCEASAssemblerExporter);
	return true;
}