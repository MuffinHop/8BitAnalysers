#pragma once

#include "../ArcadeZ80Machine.h"

class FTimePilotMachine : public FArcadeZ80Machine
{
public:
			FTimePilotMachine();

	bool InitMachine(const FArcadeZ80MachineDesc& desc) override;
	void SetupCodeAnalysisForMachine() override;
	void SetupPalette();
	void UpdateInput() override;
	void UpdateScreen() override;

	void DrawDebugOverlays(float x, float y) override;

	void DrawCharMap(int priority);
	void DrawSprites();

	void DebugDrawCommandQueue();
	void DebugDrawString(uint16_t stringAddress);

	void ExportZXNSprites();
	void ExportZXNChars();
	void ExportZXNPalettes();

	// Game ROMS
	uint8_t		ROM1[0x2000];	// 0x0000 - 0x1FFF
	uint8_t		ROM2[0x2000];	// 0x2000 - 0x3FFF
	uint8_t		ROM3[0x2000];	// 0x4000 - 0x5FFF

	uint8_t		AudioROM[0x1000];

	// GFX ROMS
	uint8_t		TilesROM[0x2000];
	uint8_t		SpriteROM[0x4000];

	// PROMS
	uint8_t		PalettePROM[0x40];
	uint8_t		SpriteLUTPROM[0x0100];
	uint8_t		CharLUTPROM[0x0100];

	// Bank Ids
	int16_t		ROM1BankId = -1;
	int16_t		ROM2BankId = -1;
	int16_t		ROM3BankId = -1;
	int16_t		RAMBankId = -1;
	int16_t		TilesROMBankId = -1;
	int16_t		SpriteROMBankId = -1;
	int32_t		TicksPerFrame = 0;
	int32_t		FrameTicks = 0;

	uint8_t* pVideoRAM = nullptr;
	uint8_t* pColourRAM = nullptr;
	uint8_t* pSpriteRAM[2] = { nullptr, nullptr };

	// Arcade machine palettes
	uint32_t	Palette[32];
	uint32_t    TileColours[32][4];
	uint32_t	SpriteColours[64][4];

	// ZXN Palettes
	uint8_t		ZXNPalette[32];
	uint8_t		ZXNTileColours[32][4];
	uint8_t		ZXNSpriteColours[64][4];

	bool		bSpriteDebug = false;
	bool		bRotateScreen = false;

	// Exported Data for ZXN
	uint8_t		ZXNSpriteImages[256 * 16 * (16 / 2)];	// max 256 sprites of 16*16 pixels - 4bpp
	uint8_t		ZXNCharImages[512 * 8 * (8 / 2)];		// max 512 chars of 8*8 pixels - 4bpp
};

void DrawCharacter8x8(FGraphicsView* pView, const uint8_t* pSrc, int xp, int yp, const uint32_t* cols, bool bFlipX, bool bFlipY, bool bRot90, bool bMask);
void DrawSprite(FGraphicsView* pView, const uint8_t* pSrc, int xp, int yp, const uint32_t* cols, bool bFlipX, bool bFlipY, bool bRot90);