#include "MCPTools.h"
#include "Misc/EmuBase.h"

class FReadMemoryTool : public FMCPTool
{
public:
	FReadMemoryTool()
	{
		Description = "Reads memory from specified memory area within a 16-bit address space, the area cannot go beyond 0xFFFF";

		InputSchema = {
			{"type", "object"},
			{"properties", {
				{"address", {
					{"type", "integer"},
					{"description", "Starting memory address to read from within a 16-bit address space"}
				}},
				{"length", {
					{"type", "integer"},
					{"description", "Number of bytes to read within a 16-bit address space"}
				}}
			}},
			{"required", {"address", "length"}}
		};
	}


	nlohmann::json Execute(FEmuBase* pEmu, const nlohmann::json& arguments)
	{
		// For demonstration, return dummy data
		uint32_t address = arguments["address"].get<uint32_t>();
		uint32_t length = arguments["length"].get<uint32_t>();


		// In real implementation, read memory from the emulated system
		std::vector<uint8_t> data;
		for (uint32_t i = 0; i < length; ++i)
		{
			data.push_back(pEmu->ReadByte(address + i));
		}

		/*
		std::ostringstream hex_ss;
		for (size_t i = 0; i < data.size(); i++)
		{
			hex_ss << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)data[i];
			if (i < data.size() - 1)
				hex_ss << " ";
		}*/

		nlohmann::json result;
		result["address"] = address;
		result["length"] = length;
		result["data"] = data;//hex_ss.str();
		return result;
	}

};

// Go to address command
// GetFocussedViewState().GoToAddress

class FGoToAddressTool : public FMCPTool
{
public:
	FGoToAddressTool()
	{
		Description = "Moves the code analysis view to the specified address within a 16-bit address space";
		InputSchema = {
			{"type", "object"},
			{"properties", {
				{"address", {
					{"type", "integer"},
					{"description", "Memory address to go to within a 16-bit address space"}
				}}
			}},
			{"required", {"address"}}
		};
	}

	nlohmann::json Execute(FEmuBase* pEmu, const nlohmann::json& arguments)
	{
		FCodeAnalysisState& codeAnalysis = pEmu->GetCodeAnalysis();
		uint32_t address = arguments["address"].get<uint32_t>();

		FAddressRef addrRef = codeAnalysis.AddressRefFromPhysicalAddress(address);
		codeAnalysis.GetFocussedViewState().GoToAddress(addrRef);
		return { {"success", true} };
	}
};

// Add comment tool
class FAddCommentTool : public FMCPTool
{
public:
	FAddCommentTool()
	{
		Description = "Adds a comment to the specified address within a 16-bit address space";
		InputSchema = {
			{"type", "object"},
			{"properties", {
				{"address", {
					{"type", "integer"},
					{"description", "Memory address to add comment to within a 16-bit address space"}
				}},
				{"comment", {
					{"type", "string"},
					{"description", "The comment text to add"}
				}}
			}},
			{"required", {"address", "comment"}}
		};
	}

	nlohmann::json Execute(FEmuBase* pEmu, const nlohmann::json& arguments)
	{
		FCodeAnalysisState& codeAnalysis = pEmu->GetCodeAnalysis();
		uint32_t address = arguments["address"].get<uint32_t>();
		std::string comment = arguments["comment"].get<std::string>();
		FAddressRef addrRef = codeAnalysis.AddressRefFromPhysicalAddress(address);
		FCodeInfo* pCodeInfo = codeAnalysis.GetCodeInfoForAddress(addrRef);
		if (pCodeInfo)
		{
			pCodeInfo->Comment = comment;
			return { {"success", true} };
		}
		return { {"success", false}, {"error", "Invalid address"} };
	}
};

// Get code info tool
class FGetCodeInfoTool : public FMCPTool
{
	public:
		FGetCodeInfoTool()
		{
			Description = "Get information about the instruction code at a specific address, such as which memory addresses it reads and writes to";
			InputSchema = {
			{"type", "object"},
			{"properties", {
				{"address", {
					{"type", "integer"},
					{"description", "Memory address of the instruction"}
				}}
			}},
			{"required", {"address"}}
			};
		}

		nlohmann::json Execute(FEmuBase* pEmu, const nlohmann::json& arguments)
		{
			FCodeAnalysisState& codeAnalysis = pEmu->GetCodeAnalysis();
			uint32_t address = arguments["address"].get<uint32_t>();
			FAddressRef addrRef = codeAnalysis.AddressRefFromPhysicalAddress(address);
			FCodeInfo* pCodeInfo = codeAnalysis.GetCodeInfoForAddress(addrRef);
			if(pCodeInfo)
			{
				// todo: output code info as json
				nlohmann::json result;
				result["disassembly"] = pCodeInfo->Text;
				result["no_times_executed"] = pCodeInfo->ExecutionCount;

				// reads
				nlohmann::json reads = nlohmann::json::array();
				auto& readRefs = pCodeInfo->Reads.GetReferences();
				for (auto& read : readRefs)
				{
					reads.push_back(read.Address);
				}
				result["memory_reads"] = reads;

				// writes
				nlohmann::json writes = nlohmann::json::array();
				auto& writeRefs = pCodeInfo->Writes.GetReferences();
				for (auto& write : writeRefs)
				{
					writes.push_back(write.Address);
				}
				result["memory_writes"] = writes;

				return result;
			}
			else
			{
				return { {"success", false}, {"error", "No code at address"} };
			}
		}
};


// TODO: GetDataInfoTool
class FGetDataInfoTool : public FMCPTool
{
public:
	FGetDataInfoTool()
	{
		Description = "Get information about the data at a specific address, such as what code has read and written to it";
		InputSchema = {
		{"type", "object"},
		{"properties", {
			{"address", {
				{"type", "integer"},
				{"description", "Memory address of the data to get info on"}
			}}
		}},
		{"required", {"address"}}
		};
	}

	nlohmann::json Execute(FEmuBase* pEmu, const nlohmann::json& arguments)
	{
		FCodeAnalysisState& codeAnalysis = pEmu->GetCodeAnalysis();
		uint32_t address = arguments["address"].get<uint32_t>();
		FAddressRef addrRef = codeAnalysis.AddressRefFromPhysicalAddress(address);
		FDataInfo* pDataInfo = codeAnalysis.GetDataInfoForAddress(addrRef);
		if (pDataInfo)
		{
			nlohmann::json result;

			// reads
			nlohmann::json reads = nlohmann::json::array();
			auto& readRefs = pDataInfo->Reads.GetReferences();
			for (auto& read : readRefs)
			{
				reads.push_back(read.Address);
			}
			result["read_instrujctions"] = reads;

			// writes
			nlohmann::json writes = nlohmann::json::array();
			auto& writeRefs = pDataInfo->Writes.GetReferences();
			for (auto& write : writeRefs)
			{
				writes.push_back(write.Address);
			}
			result["write_instructions"] = writes;
			result["last_writer"] = pDataInfo->LastWriter.Address;

			return result;
		}

		return { {"success", false}, {"error", "No data at address"} };

	}
};


void RegisterBaseTools(FMCPToolsRegistry& registry)
{
	registry.RegisterTool("read_memory", new FReadMemoryTool());
	registry.RegisterTool("go_to_address", new FGoToAddressTool());
	//registry.RegisterTool("add_comment", new FAddCommentTool());
	registry.RegisterTool("get_code_info", new FGetCodeInfoTool());
	registry.RegisterTool("get_data_info", new FGetDataInfoTool());
}