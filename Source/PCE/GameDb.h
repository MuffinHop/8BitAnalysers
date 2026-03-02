#include <vector>
#include <string>
#include <map>

struct FGameDbBank
{
	uint16_t GetMappedAddress() const { return MprSlot == -1 ? 0 : MprSlot * 0x2000; }

	// Has been mapped to multiple mpr slots?
	bool bMultipleAddresses = false;
	
	// todo make this a bitfield of all slots this banks has been mapped to?
	int MprSlot = -1;
};

struct FGameDbEntry
{
	std::vector<FGameDbBank> Banks;

	// How many banks are mapped into multiple mpr slots?
	// Note: this is not saved in the json file.
	int NumAmbiguousBanks = 0;
};

typedef std::map<std::string, FGameDbEntry> TGameDb;

FGameDbEntry* GetGameDbEntry(const std::string& name);
FGameDbEntry& CreateGameDbEntry(const std::string& name, int bankCount);
TGameDb& GetGameDb();

bool SaveGameDbEntry(const std::string& gameName, const std::string& fname);
bool LoadGameDbEntry(const std::string& gameName, const std::string& fname);

