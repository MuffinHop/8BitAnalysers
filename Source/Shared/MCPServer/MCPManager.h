#pragma once
#include "MCPTransport.h"
#include "MCPServer.h"

class FEmuBase;

enum class EMCPTransportType
{
	Stdio,
	HTTP,
};

class FMCPManager
{
public:
	FMCPManager(FMCPToolsRegistry* pTools, FMCPResourceRegistry* pResources) : pToolsRegistry(pTools), pResourcesRegistry(pResources) {}

	~FMCPManager()
	{
	}

	void SetTransportType(EMCPTransportType type, int port = 7777)
	{
		TransportType = type;
		Port = port;
	}

	void Start();
	void Stop();
	void ProcessCommands();

	bool IsRunning() const { return pMCPServer && pMCPServer->IsRunning(); }
	int GetTransportType() const { return (int)TransportType; }

private:
	FMCPServer*			pMCPServer = nullptr;
	FMCPToolsRegistry*	pToolsRegistry = nullptr;
	FMCPResourceRegistry* pResourcesRegistry = nullptr;
	FMCPCommandQueue		CommandQueue;
	FMCPResponseQueue		ResponseQueue;
	EMCPTransportType	TransportType = EMCPTransportType::Stdio;

	int			Port = 7777;

};

void InitMCPServer(FEmuBase* pEmu);
void ShutdownMCPServer();
void UpdateMCPServer();