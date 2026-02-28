#include "GameDb.h"

#include "json.hpp"
#include <iomanip>
#include <fstream>
#include <sstream>

using json = nlohmann::json;

std::map<std::string, TBankAddresses> gGameBankMappings;

TBankAddresses& GetBankMappingsForGame(const std::string& name)
{
	return gGameBankMappings[name];
}

TGameBankMappings& GetBankMappings()
{
	return gGameBankMappings;
}


void SaveBankMappings(const std::string& gameName, int bankCount, const std::string& fname)
{
	json jsonFile;

	jsonFile["Name"] = gameName;
	jsonFile["NumBanks"] = bankCount;

	const TBankAddresses& mappings = gGameBankMappings[gameName];
	
	assert(bankCount == mappings.size());

	for (auto mappingAddr : mappings)
	{
		json mappingJson;
		mappingJson["Addr"] = mappingAddr;
		jsonFile["Mappings"].push_back(mappingJson);
	}

	std::ofstream outFileStream(fname);
	if (outFileStream.is_open())
	{
		outFileStream << std::setw(4) << jsonFile << std::endl;
	}
	return;
}

void LoadBankMappings(const std::string& gameName, const std::string& fname)
{
	std::ifstream inFileStream(fname);
	if (inFileStream.is_open() == false)
		return;

	json jsonFile;

	inFileStream >> jsonFile;
	inFileStream.close();

	const std::string name = jsonFile["Name"];
	const int numBanks = jsonFile["NumBanks"];

	TBankAddresses& mappings = gGameBankMappings[name];
	mappings.clear();
	mappings.resize(numBanks);

	json& mappingsJson = jsonFile["Mappings"];

	for (int i = 0; i< numBanks; i++)
	{
		json& mappingJson = mappingsJson[i];
		mappings[i] = mappingJson["Addr"];
	}
}