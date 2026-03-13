
#pragma once

#include <vector>
#include <string>

class FPCEEmu;

struct FAsmExportValidator
{
	bool Validate(FPCEEmu* pEmu, const std::vector<int16_t>& banksExported, const std::string& asmFname);
};
