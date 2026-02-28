#include <vector>
#include <string>
#include <map>

typedef std::vector<uint16_t> TBankAddresses;
typedef std::map<std::string, TBankAddresses> TGameBankMappings;

TBankAddresses& GetBankMappingsForGame(const std::string& name); 
TGameBankMappings& GetBankMappings();

void SaveBankMappings(const std::string& gameName, int bankCount, const std::string& fname);
void LoadBankMappings(const std::string& gameName, const std::string& fname);

