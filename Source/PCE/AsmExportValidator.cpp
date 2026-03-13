#include <imgui.h>

#include "AsmExportValidator.h"

#include "PCEEmu.h"

//#include <chrono>
#include "PCEConfig.h"
#include "Util/FileUtil.h"
//#include <geargrafx_core.h>
#include "Debug/DebugLog.h"
#include "PCEGameConfig.h"

bool FAsmExportValidator::Validate(FPCEEmu* pEmu, const std::vector<int16_t>& banksExported, const std::string& asmFname)
{
#ifdef _WIN32
	const int bankCount = pEmu->GetBankCount();
	printf("--------------------------------------------------------------------------------------------------------\n");
	//printf("Assembling %s [%d banks]\n", pCurrentProjectConfig->Name.c_str(), (int)banksToExport.size());

	LOGINFO("Assembling: %s. [%d/%d banks]", pEmu->GetProjectConfig()->Name.c_str(), (int)banksExported.size(), bankCount);

	// todo: export to temp directory?

	const std::string outputPceFname = RemoveFileExtension(asmFname.c_str()) + ".pce";

	// create tmp.txt and output which file we are assembling
	std::string echoCmd = "echo Assembling " + pEmu->GetProjectConfig()->Name + " > tmp.txt";
	std::system(echoCmd.c_str());
		
	// echo blank line
	std::system("echo[ >> tmp.txt");

	// This presumes pceas.exe is in your windows path.
	char cmdTxt[256];
		
	// append the results to out.txt
	snprintf(cmdTxt, 256, "pceas.exe --raw \"%s\" >> tmp.txt", asmFname.c_str());
	const int errorCode = std::system(cmdTxt);

	// append to the batch log file
	std::system("type tmp.txt >> AssembleLog.txt");

	// print the contents to std output so we can see the result in the PCEAnalyser command window
	std::system("type tmp.txt");

	LOGINFO("Assembled '%s' : %s", pEmu->GetProjectConfig()->Name.c_str(), errorCode ? "FAILURE" : "SUCESS");

	std::system("echo -------------------------------------------------------------------------------------------------------- >> BatchAssembleLog.txt");
	printf("--------------------------------------------------------------------------------------------------------\n");

	if (errorCode)
		return false;

	size_t newFileSize = 0;
	uint8_t* pOrigData = (uint8_t*)LoadBinaryFile(outputPceFname.c_str(), newFileSize);
	if (pOrigData == nullptr)
	{
		LOGINFO("Could not load '%s' to verify contents.", outputPceFname.c_str());
		return false;
	}

	LOGINFO("Produced .pce is %d bytes, %.1fKB", newFileSize, (float)newFileSize / 1024.f);

	size_t origFileSize = 0;
	uint8_t* pNewData = nullptr;
	auto findIt = pEmu->GetGamesLists().find(pEmu->GetProjectConfig()->EmulatorFile.ListName);
	if (findIt != pEmu->GetGamesLists().end())
	{
		const std::string origFname = findIt->second.GetRootDir() + pEmu->GetProjectConfig()->EmulatorFile.FileName;
		pNewData = (uint8_t*)LoadBinaryFile(origFname.c_str(), origFileSize);
		if (pNewData != nullptr)
		{			
			LOGINFO("Original .pce is %d bytes, %.1fKB", origFileSize, (float)origFileSize / 1024.f);

			if (newFileSize == origFileSize)
			{
				LOGINFO(".pce files are the same size.");
			}
			else
			{
				const long diffBytes = abs((long)(newFileSize - origFileSize));
				LOGINFO("pce files size do not match. Difference is %d bytes (0x%x) %.1fKB]", diffBytes, diffBytes, (float)diffBytes / 1024.0f);
			}

			int numDiffs = 0;

			// check the data for the exported pce file matches the original pce file.
			// only check the bytes for the banks we exported.
			for (auto bankId : banksExported)
			{
				const uint8_t bankIndex = pEmu->GetBankIndexForBankId(bankId);
				uint8_t* pNewBankData = pNewData + 0x2000 * bankIndex;
				uint8_t* pOrigBankData = pOrigData + 0x2000 * bankIndex;

				const FCodeAnalysisBank* pBank = pEmu->GetCodeAnalysis().GetBank(bankId);

				if (bankIndex < bankCount)
				{
					int numBankDiffs = 0;
					for (int i = 0; i < 0x2000; i++)
					{
						if (pNewBankData[i] != pOrigBankData[i])
							numBankDiffs++;
					}

					if (numBankDiffs)
						LOGINFO("Found %04d diffs in %s (bank index %d).", numBankDiffs, pBank->Name.c_str(), bankIndex);

					numDiffs += numBankDiffs;
				}
				else
				{
					LOGINFO("Invalid bank index %d for %s", bankIndex, pBank->Name.c_str());
				}
			}

			if (!numDiffs)
			{
				if (newFileSize != origFileSize)
					LOGINFO("Exported banks match the originals.");
				else
					LOGINFO("Files are identical!");
			}
			else
			{
				LOGINFO("Found %d bytes that are different.", numDiffs);
			}

			free(pNewData);
		}
		else
		{
			LOGINFO("Could not load '%s' to verify contents.", origFname.c_str());
		}
	}

	free(pOrigData);

	// todo return if it failed
	return true;
#endif
}
