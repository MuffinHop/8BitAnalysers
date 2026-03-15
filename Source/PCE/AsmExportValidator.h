
#pragma once

#include <vector>
#include <string>

class FPCEEmu;

struct FAsmExportValidator
{
	bool Validate(FPCEEmu* pEmu, const std::vector<int16_t>& banksExported, const std::string& asmFname);
	bool Assemble(FPCEEmu* pEmu, const std::string& asmFname);
	bool CompareRomFiles(FPCEEmu* pEmu, const std::vector<int16_t>& banksExported, const std::string& asmFname) const;
	bool RunEmulatorTest(FPCEEmu* pEmu, const std::string& asmFname);
};
