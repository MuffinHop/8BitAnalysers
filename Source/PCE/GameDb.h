#include <vector>
#include <string>
#include <map>

typedef std::vector<uint16_t> TBankAddresses;
typedef std::map<std::string, TBankAddresses> TGameBankMappings;

TBankAddresses* GetBankMappingsForGame(const std::string& name); 
TBankAddresses& CreateBankMappingsForGame(const std::string& name, int bankCount);
TGameBankMappings& GetBankMappings();

void SaveBankMappings(const std::string& gameName, const std::string& fname);
bool LoadBankMappings(const std::string& gameName, const std::string& fname);

