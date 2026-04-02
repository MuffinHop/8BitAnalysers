#pragma once

#include "../IOAnalyser.h"
#include <chips/gdg_whid65040.h>

class FEmuBase;

struct FGDGWrite
{
    FAddressRef PC;
    int         FrameNo;
    uint16_t    Port;       /* full 16-bit port address */
    uint8_t     Data;
};

class FGDGDevice : public FIODevice
{
public:
    FGDGDevice();
    bool    Init(const char* pName, FEmuBase* pEmulator, gdg_whid65040_t* pGDG);

    void    WriteGDG(FAddressRef pc, uint16_t port, uint8_t data);

    void    OnFrameTick() override;
    void    OnMachineFrameEnd() override;
    void    DrawDetailsUI() override;

private:
    void    DrawRegistersUI();
    void    DrawTimingUI();
    void    DrawBankSwitchUI();
    void    DrawPaletteUI();
    void    DrawWriteLogUI();

    gdg_whid65040_t* pGDGState = nullptr;

    int     FrameNo = 0;

    /* Write history ring buffer */
    static const int kWriteBufferSize = 256;
    FGDGWrite   WriteBuffer[kWriteBufferSize];
    int         WriteBufferWriteIndex = 0;
};
