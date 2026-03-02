#include "GameDb.h"

#include "json.hpp"
#include <iomanip>
#include <fstream>
#include <sstream>

using json = nlohmann::json;

TGameDb gGameDb;

FGameDbEntry* GetGameDbEntry(const std::string& name)
{
	auto it = gGameDb.find(name);
	if (it == gGameDb.end())
		return nullptr;
	return &(it->second);
	return nullptr;}

FGameDbEntry& CreateGameDbEntry(const std::string& name, int bankCount)
{
	assert(gGameDb.find(name) == gGameDb.end());

	FGameDbEntry& dbEntry = gGameDb[name];
	dbEntry.Banks.resize(bankCount);
	return dbEntry;
}

TGameDb& GetGameDb()
{
	return gGameDb;
}

bool SaveGameDbEntry(const std::string& gameName, const std::string& fname)
{
	json jsonFile;

	const auto it = gGameDb.find(gameName);
	if (it == gGameDb.end())
	{
		// Dont save if no entry exists
		return false;
	}

	FGameDbEntry& entry = it->second;

	jsonFile["Name"] = gameName;
	jsonFile["NumBanks"] = entry.Banks.size();

	entry.NumAmbiguousBanks = 0;

	for (auto dbBank : entry.Banks)
	{
		json mappingJson;
		mappingJson["MprSlot"] = dbBank.MprSlot;
		mappingJson["Multiple"] = dbBank.bMultipleAddresses;
		jsonFile["Mappings"].push_back(mappingJson);

		if (dbBank.MprSlot != -1 && dbBank.bMultipleAddresses)
			entry.NumAmbiguousBanks++;
	}

	std::ofstream outFileStream(fname);
	if (outFileStream.is_open())
	{
		outFileStream << std::setw(4) << jsonFile << std::endl;
		return true;
	}
	return false;
}

bool LoadGameDbEntry(const std::string& gameName, const std::string& fname)
{
	std::ifstream inFileStream(fname);
	if (inFileStream.is_open() == false)
		return false;

	json jsonFile;

	inFileStream >> jsonFile;
	inFileStream.close();

	const std::string name = jsonFile["Name"];
	const int numBanks = jsonFile["NumBanks"];

	FGameDbEntry& entry = gGameDb[name];
	entry.Banks.clear();
	entry.Banks.resize(numBanks);

	json& mappingsJson = jsonFile["Mappings"];

	entry.NumAmbiguousBanks = 0;

	for (int i = 0; i< numBanks; i++)
	{
		json& mappingJson = mappingsJson[i];
		entry.Banks[i].MprSlot = mappingJson["MprSlot"];
		entry.Banks[i].bMultipleAddresses = mappingJson["Multiple"];

		if (entry.Banks[i].MprSlot != -1 && entry.Banks[i].bMultipleAddresses)
			entry.NumAmbiguousBanks++;
	}
	
	return true;
}