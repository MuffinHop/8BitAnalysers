#include "MCPManager.h"
#include "MCPTools.h"
#include "MCPResources.h"

FMCPManager* g_MCPManager = nullptr;
FMCPToolsRegistry* g_MCPToolsRegistry = nullptr;
FMCPResourceRegistry* g_MCPResourceRegistry = nullptr;

void InitMCPServer(FEmuBase* pEmu)
{
	EMCPTransportType transportType = EMCPTransportType::HTTP;
	int port = 7777;

	if (g_MCPManager == nullptr)
	{
		g_MCPToolsRegistry = new FMCPToolsRegistry(pEmu);
		g_MCPResourceRegistry = new FMCPResourceRegistry(pEmu);
		g_MCPManager = new FMCPManager(g_MCPToolsRegistry, g_MCPResourceRegistry);
	}
	g_MCPManager->SetTransportType(transportType, port);
	g_MCPManager->Start();

	RegisterBaseTools(*g_MCPToolsRegistry);
	//RegisterArcadeZ80Tools(*g_MCPToolsRegistry);

	RegisterBaseResources(*g_MCPResourceRegistry);
	//RegisterArcadeZ80Resources(*g_MCPResourceRegistry);
}

void ShutdownMCPServer()
{
	if (g_MCPManager)
	{
		g_MCPManager->Stop();
		delete g_MCPManager;
		g_MCPManager = nullptr;
	}
	if (g_MCPToolsRegistry)
	{
		delete g_MCPToolsRegistry;
		g_MCPToolsRegistry = nullptr;
	}
}

void UpdateMCPServer()
{
	if (g_MCPManager)
	{
		g_MCPManager->ProcessCommands();
	}
}