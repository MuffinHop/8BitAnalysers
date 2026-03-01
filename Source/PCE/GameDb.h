#include <vector>
#include <string>
#include <map>

struct FGameDbBank
{
	// Has been mapped to multiple addresses
	bool bMultipleAddresses = false;
	uint16_t Address = 0;
};

struct FGameDbEntry
{
	std::vector<FGameDbBank> banks;
};

typedef std::map<std::string, FGameDbEntry> TGameDb;

FGameDbEntry* GetGameDbEntry(const std::string& name);
FGameDbEntry& CreateGameDbEntry(const std::string& name, int bankCount);
TGameDb& GetGameDb();

bool SaveGameDbEntry(const std::string& gameName, const std::string& fname);
bool LoadGameDbEntry(const std::string& gameName, const std::string& fname);

