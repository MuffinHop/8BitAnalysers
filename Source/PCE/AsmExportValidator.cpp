#include <imgui.h>

#include "AsmExportValidator.h"

#include "crc.h"

#include "PCEEmu.h"
#include "GameDb.h"

//#include <chrono>
#include "PCEConfig.h"
#include "Util/FileUtil.h"
#include "Debug/DebugLog.h"
#include "PCEGameConfig.h"
#include "DebugStats.h"

// todo copy failed pces to a directory for easier manual inspection?

bool FAsmExportValidator::Validate(const std::vector<int16_t>& banksExported, const std::string& asmFname)
{
#ifdef _WIN32
	LOGINFO("Assembling: %s. [%d/%d banks]", pPCEEmu->GetProjectConfig()->Name.c_str(), (int)banksExported.size(), pPCEEmu->GetBankCount());

	if (!Assemble(asmFname))
	{
		return false;
	}

	CompareRomFiles(banksExported, asmFname);

	RunEmulatorTest(asmFname);

	// todo return if it failed
	return true;
#endif
}

bool FAsmExportValidator::Assemble(const std::string& asmFname)
{
	printf("--------------------------------------------------------------------------------------------------------\n");

	// todo: export to temp directory?

	// create tmp.txt and output which file we are assembling
	std::string echoCmd = "echo Assembling " + pPCEEmu->GetProjectConfig()->Name + " > tmp.txt";
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

	LOGINFO("Assembled '%s' : %s", pPCEEmu->GetProjectConfig()->Name.c_str(), errorCode ? "FAILURE" : "SUCESS");

	std::system("echo -------------------------------------------------------------------------------------------------------- >> BatchAssembleLog.txt");
	printf("--------------------------------------------------------------------------------------------------------\n");

	Results.bAssembledOk = errorCode == 0 ? true : false;

	return Results.bAssembledOk;
}

bool FAsmExportValidator::CompareRomFiles(const std::vector<int16_t>& banksExported, const std::string& asmFname)
{
	const std::string outputPceFname = RemoveFileExtension(asmFname.c_str()) + ".pce";

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
	auto findIt = pPCEEmu->GetGamesLists().find(pPCEEmu->GetProjectConfig()->EmulatorFile.ListName);
	if (findIt != pPCEEmu->GetGamesLists().end())
	{
		const int bankCount = pPCEEmu->GetBankCount();
		const std::string origFname = findIt->second.GetRootDir() + pPCEEmu->GetProjectConfig()->EmulatorFile.FileName;
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
				LOGINFO("pce files size do not match. Difference is %d bytes (0x%x) [%.1fKB]", diffBytes, diffBytes, (float)diffBytes / 1024.0f);
			}

			int numDiffs = 0;

			// check the data for the exported pce file matches the original pce file.
			// only check the bytes for the banks we exported.
			for (auto bankId : banksExported)
			{
				const uint8_t bankIndex = pPCEEmu->GetBankIndexForBankId(bankId);
				uint8_t* pNewBankData = pNewData + 0x2000 * bankIndex;
				uint8_t* pOrigBankData = pOrigData + 0x2000 * bankIndex;

				const FCodeAnalysisBank* pBank = pPCEEmu->GetCodeAnalysis().GetBank(bankId);

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
				const bool bExportedAllBanks = bankCount == banksExported.size();
				if (bExportedAllBanks && newFileSize == origFileSize)
				{
					LOGINFO("Files are identical! Exported all banks.");
					Results.bRomFileIdentical = true;
				}
				else
				{
					Results.bRomFilePartialMatch = true;
					LOGINFO("Exported banks match the originals. Did not export all banks.");
				}
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

	return Results.bRomFileIdentical;
}

void NormaliseFilePath(char* outFilePath, const char* inFilePath);

// problematic games:
// false positives:
// false negatives:

bool FAsmExportValidator::RunEmulatorTest(const std::string& asmFname)
{
	const std::string outputPceFname = RemoveFileExtension(asmFname.c_str()) + ".pce";

	// turn off callback to improve performance
	pPCEEmu->EnableGeargrafxCallbacks(false);

	// do I need to reset anything here?
	// do I need to worry about tidying up the analyser state if this fails?

	LOGINFO("Running emulator frame buffer test on '%s'...", outputPceFname.c_str());

	if (pPCEEmu->GetCore()->LoadMedia(outputPceFname.c_str()) == false)
		return false;

	if (pPCEEmu->GetMedia()->IsReady() == false)
		return false;

	FDebugger& debugger = pPCEEmu->GetCodeAnalysis().Debugger;
	const bool bWasStopped = debugger.IsStopped();
	
	debugger.Continue();

	LOGINFO("Checking %d frames...", GameFrameNo);
	
	int numDiffs = 0;
	for (int i = 0; i < GameFrameNo; i++)
	{
		int audioSampleCount = 0;
		pPCEEmu->GetCore()->RunToVBlank(pPCEEmu->GetFrameBuffer(), pPCEEmu->GetAudioBuffer(), &audioSampleCount);

		const u32 framebufCRC = CalculateCRC32(0, pPCEEmu->GetFrameBuffer(), FPCEEmu::kFramebufferSize);
		
		LOGINFO("%03d CRC %8x [%s]%s", i, framebufCRC, framebufCRC == FramebufferCRCs[i] ? "MATCH" : "DIFF", i < kNumIgnoredCRCs ? "[IGNORED]" : "");

		// Skip the first few frames because the frame CRCs often don't match - for some unknown reason
		if (i < kNumIgnoredCRCs)
			continue;

		if (framebufCRC != FramebufferCRCs[i])
			numDiffs++;
	}

	pPCEEmu->EnableGeargrafxCallbacks(true);
	
	if (bWasStopped)
		debugger.Break();
	
	if (numDiffs)
	{
		LOGINFO("Test Failed. %d frames differ", numDiffs);
	}
	else
	{
		// todo take into account diffs we ignored?

		if (GameFrameNo == kNumFramebufferCRCs)
		{
			LOGINFO("Test passed. %d frames are identical.", kNumFramebufferCRCs);
		}
		else
			LOGINFO("Test partially passed. %d frames are identical.", GameFrameNo);

		Results.bEmulatorTestOk = true;
	}

	// copy pce to specific directory based on if it passed or not
	EnsureDirectoryExists("EmuTestPassed");
	EnsureDirectoryExists("EmuTestFailed");

	char cmdTxt[256];
	if (Results.bEmulatorTestOk)
		snprintf(cmdTxt, 256, "copy \"%s\" EmuTestPassed", outputPceFname.c_str());
	else
		snprintf(cmdTxt, 256, "copy \"%s\" EmuTestFailed", outputPceFname.c_str());

	char cmdTxtNormalised[256];
	NormaliseFilePath(cmdTxtNormalised, cmdTxt);
	std::system(cmdTxtNormalised);

	return true;
}

void FAsmExportValidator::Reset()
{
	GameFrameNo = 0;

	FramebufferCRCs.clear();
	FramebufferCRCs.resize(kNumFramebufferCRCs);

	Results.bAssembledOk = false;
	Results.bEmulatorTestOk = false;
	Results.bRomFileIdentical = false;
	Results.bRomFilePartialMatch = false;
}

void FAsmExportValidator::Tick()
{
	if (GameFrameNo < kNumFramebufferCRCs)
	{
		const u32 framebufCRC = CalculateCRC32(0, pPCEEmu->GetFrameBuffer(), FPCEEmu::kFramebufferSize);
		FramebufferCRCs[GameFrameNo] = framebufCRC; 
		LOGINFO("%03d CRC %x", GameFrameNo, framebufCRC);
		GameFrameNo++;
	}

}
