
#pragma once

#include <vector>
#include <string>

class FPCEEmu;


struct FAsmExportValidator
{
	// 30 secs worth of frames
	//static constexpr int kNumFramebufferCRCs = 1800;
	static constexpr int kNumFramebufferCRCs = 600;
	static constexpr int kNumIgnoredCRCs = 30;

	struct FResults
	{
		bool bAssembledOk = false;
		bool bRomFilePartialMatch = false;
		bool bRomFileIdentical = false;
		bool bEmulatorTestOk = false;
	};

	FAsmExportValidator(FPCEEmu* pEmu) : pPCEEmu(pEmu) {}

	bool Validate(const std::vector<int16_t>& banksExported, const std::string& asmFname);
	bool Assemble(const std::string& asmFname);
	bool CompareRomFiles(const std::vector<int16_t>& banksExported, const std::string& asmFname);
	bool RunEmulatorTest(const std::string& asmFname);
	void Reset();
	void Tick();
	const FResults& GetResults() const { return Results; }

protected:
	int GameFrameNo = 0;
	std::vector<uint32_t> FramebufferCRCs;
	FPCEEmu* pPCEEmu = nullptr;
	FResults Results;
};
