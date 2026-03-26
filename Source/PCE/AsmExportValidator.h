
#pragma once

#include <vector>
#include <string>

class FPCEEmu;

struct FAsmExportValidator
{
	struct FResults
	{
		bool DidPass();
		bool bAssembledOk = false;
		bool bRomFilePartialMatch = false;
		bool bRomFileIdentical = false;
		bool bEmulatorTestOk = false;
	};

	FAsmExportValidator(FPCEEmu* pEmu) : pPCEEmu(pEmu) {}

	bool Validate(const std::vector<int16_t>& banksExported, const std::string& asmFname, bool bOutputListing = false);
	bool Assemble(const std::string& asmFname, bool bOutputListing);
	bool CompareRomFiles(const std::vector<int16_t>& banksExported, const std::string& asmFname);
	bool RunEmulatorTest(const std::string& asmFname);
	void Reset(bool bStartValidating);
	void Tick();
	const FResults& GetResults() const { return Results; }

protected:
	int GameFrameNo = 0;
	bool bIsValidating = false;
	std::vector<uint32_t> FramebufferCRCs;
	FPCEEmu* pPCEEmu = nullptr;
	FResults Results;
};
