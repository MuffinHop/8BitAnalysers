#include "MZ800Emu.h"
#include "Misc/MainLoop.h"
#include <stdio.h>

int main(int argc, char** argv)
{
    printf("Starting MZ800 Analyser...\n");
    FEmulatorLaunchConfig launchConfig;
    launchConfig.ParseCommandline(argc, argv);

    FEmuBase* pEmulator = new FMZ800Emu();
    printf("Created emulator instance.\n");
    int res = RunMainLoop(pEmulator, launchConfig);
    printf("RunMainLoop exited with code %d.\n", res);
    
    delete (FMZ800Emu*)pEmulator;
    printf("Deleted emulator instance.\n");
    return 0;
}
