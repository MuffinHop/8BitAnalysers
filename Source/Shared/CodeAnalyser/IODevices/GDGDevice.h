#pragma once

#include "../IOAnalyser.h"
#include <chips/gdg_whid65040.h>
#include <ui/ui_util.h>
#include <ui/ui_chip.h>

class FEmuBase;

struct FGDGWrite
{
    FAddressRef PC;
    int         FrameNo;
    uint16_t    Port;       /* full 16-bit port address */
    uint8_t     Data;
    /* Beam position at the moment of the write */
    uint16_t    Line;
    uint16_t    Pixel;
};

/* GDG chip UI state (pin diagram) */
typedef struct {
    const char* title;
    gdg_whid65040_t* gdg;
    bool valid;
    ui_chip_t chip;
} ui_gdg_mz_t;

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
    void    DrawPinDiagramUI();
    void    DrawRegistersUI();
    void    DrawTimingUI();
    void    DrawGraphsUI();
    void    DrawBankSwitchUI();
    void    DrawPaletteUI();
    void    DrawVRAMViewerUI();
    void    DrawWriteLogUI();

    gdg_whid65040_t* pGDGState = nullptr;
    ui_gdg_mz_t UIState;

    int     FrameNo = 0;

    /* Write history ring buffer */
    static const int kWriteBufferSize = 256;
    FGDGWrite   WriteBuffer[kWriteBufferSize];
    int         WriteBufferWriteIndex = 0;

    /* Per-frame graphs */
    static const int kGraphSamples = 200;
    float   WaitStallValues[kGraphSamples];
    float   ModeValues[kGraphSamples];      /* DMD register over time */
    int     GraphOffset = 0;
};
