#include "GameDb.h"

#include "json.hpp"
#include <iomanip>
#include <fstream>
#include <sstream>

using json = nlohmann::json;

std::map<std::string, TBankAddresses> gGameBankMappings;

TBankAddresses* GetBankMappingsForGame(const std::string& name)
{
	auto it = gGameBankMappings.find(name);
	if (it == gGameBankMappings.end())
		return nullptr;
	return &(it->second);
}

TBankAddresses& CreateBankMappingsForGame(const std::string& name, int bankCount)
{
	assert(gGameBankMappings.find(name) == gGameBankMappings.end());

	TBankAddresses& bankMappings = gGameBankMappings[name];
	bankMappings.resize(bankCount);
	return bankMappings;
}

TGameBankMappings& GetBankMappings()
{
	return gGameBankMappings;
}

void SaveBankMappings(const std::string& gameName, const std::string& fname)
{
	json jsonFile;

	const TBankAddresses& mappings = gGameBankMappings[gameName];

	jsonFile["Name"] = gameName;
	jsonFile["NumBanks"] = mappings.size();

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

bool LoadBankMappings(const std::string& gameName, const std::string& fname)
{
	std::ifstream inFileStream(fname);
	if (inFileStream.is_open() == false)
		return false;

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
	return true;
}