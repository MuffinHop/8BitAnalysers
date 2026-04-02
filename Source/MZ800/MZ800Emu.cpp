#include "MZ800Emu.h"
#include "MZ800ChipsImpl.h"
#include "MZ800Config.h"
#include <imGui/imgui.h>
#include <Util/FileUtil.h>
#include <ImGuiSupport/ImGuiTexture.h>
#include <CodeAnalyser/UI/Z80/RegisterViewZ80.h>
#include <sokol_audio.h>
#include <json/single_include/nlohmann/json.hpp>
#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// FMZ800GlobalConfig JSON serialization (implemented in .cpp for full json.hpp)
// ---------------------------------------------------------------------------
void FMZ800GlobalConfig::ReadFromJson(const nlohmann::json& j)
{
    if (j.contains("PAL"))     bPAL    = j["PAL"].get<bool>();
    if (j.contains("RomPath")) RomPath = j["RomPath"].get<std::string>();
    if (j.contains("Rom700"))  Rom700  = j["Rom700"].get<std::string>();
    if (j.contains("RomCG"))   RomCG   = j["RomCG"].get<std::string>();
    if (j.contains("Rom800"))  Rom800  = j["Rom800"].get<std::string>();
    if (j.contains("MzfPath")) MzfPath = j["MzfPath"].get<std::string>();
}

void FMZ800GlobalConfig::WriteToJson(nlohmann::json& j) const
{
    j["PAL"]     = bPAL;
    j["RomPath"] = RomPath;
    j["Rom700"]  = Rom700;
    j["RomCG"]   = RomCG;
    j["Rom800"]  = Rom800;
    j["MzfPath"] = MzfPath;
}

extern void SetWindowTitle(const char* pTitle);

/* Audio push callback — feeds PSG samples to sokol_audio */
static void MZ800PushAudio(const float* samples, int num_samples, void* user_data)
{
    FMZ800Emu* pEmu = (FMZ800Emu*)user_data;
    if (pEmu->GetGlobalConfig()->bEnableAudio)
        saudio_push(samples, num_samples);
}

// ---------------------------------------------------------------------------
// Code analysis framework integration
// ---------------------------------------------------------------------------
void FMZ800Emu::OnInstructionExecuted(int ticks, uint64_t pins)
{
	FCodeAnalysisState& state = CodeAnalysis;
	const uint16_t pc = pins & 0xffff;
	RegisterCodeExecuted(state, pc, PreviousPC);
	PreviousPC = pc;
}

void FMZ800Emu::CPUTickHook(uint64_t pins, void* user_data)
{
	FMZ800Emu* emu = (FMZ800Emu*)user_data;

	emu->InstructionsTicks++;

	const bool bNewOp = z80_opdone(&g_mz800_sys.cpu);
	if (bNewOp)
	{
		emu->OnInstructionExecuted(emu->InstructionsTicks, pins);
		emu->InstructionsTicks = 0;
	}

	emu->CodeAnalysis.OnCPUTick(pins);
}

int MZ800KeyFromImGuiKey(ImGuiKey key) {
    int mzKey = 0;
    if (key >= ImGuiKey_0 && key <= ImGuiKey_9) mzKey = '0' + (key - ImGuiKey_0);
    else if (key >= ImGuiKey_A && key <= ImGuiKey_Z) mzKey = 'A' + (key - ImGuiKey_A);
    else if (key == ImGuiKey_Space) mzKey = 32;
    else if (key == ImGuiKey_Enter) mzKey = 13;
    else if (key == ImGuiKey_Tab) mzKey = 9;
    else if (key == ImGuiKey_Escape) mzKey = 27;
    else if (key == ImGuiKey_LeftCtrl) mzKey = 140;
    else if (key == ImGuiKey_RightCtrl) mzKey = 140;
    else if (key == ImGuiKey_LeftShift) mzKey = 141;
    else if (key == ImGuiKey_RightShift) mzKey = 141;
    else if (key == ImGuiKey_Insert) mzKey = 134;
    else if (key == ImGuiKey_Delete) mzKey = 135;
    else if (key == ImGuiKey_Backspace) mzKey = 135;
    else if (key == ImGuiKey_UpArrow) mzKey = 136;
    else if (key == ImGuiKey_DownArrow) mzKey = 137;
    else if (key == ImGuiKey_RightArrow) mzKey = 138;
    else if (key == ImGuiKey_LeftArrow) mzKey = 139;
    else if (key == ImGuiKey_F1) mzKey = 142;
    else if (key == ImGuiKey_F2) mzKey = 143;
    else if (key == ImGuiKey_F3) mzKey = 144;
    else if (key == ImGuiKey_F4) mzKey = 145;
    else if (key == ImGuiKey_F5) mzKey = 146;
    else if (key == ImGuiKey_Semicolon) mzKey = ';';
    else if (key == ImGuiKey_Apostrophe) mzKey = ':';
    else if (key == ImGuiKey_Comma) mzKey = ',';
    else if (key == ImGuiKey_Period) mzKey = '.';
    else if (key == ImGuiKey_Slash) mzKey = '/';
    else if (key == ImGuiKey_Backslash) mzKey = '\\';
    else if (key == ImGuiKey_Minus) mzKey = '-';
    else if (key == ImGuiKey_Equal) mzKey = '+';
    else if (key == ImGuiKey_LeftBracket) mzKey = '[';
    else if (key == ImGuiKey_RightBracket) mzKey = ']';
    else if (key == ImGuiKey_GraveAccent) mzKey = '~';
    return mzKey;
}

// ---------------------------------------------------------------------------
// ROM LOADING
// ROM layout: [0x0000] MZ-700 monitor 4KB + [0x1000] CGROM 4KB + [0x2000] MZ-800 monitor 8KB
// Tries all-in-one 16KB file first; falls back to three separate files.
// Paths are resolved through GetBundlePath (relative to Contents/MacOS/).
// ---------------------------------------------------------------------------
bool FMZ800Emu::LoadROM()
{
    FMZ800GlobalConfig* cfg = static_cast<FMZ800GlobalConfig*>(pGlobalConfig);

    // --- try all-in-one 16KB ROM ---
    const char* allInOne = GetBundlePath(cfg->RomPath.c_str());
    if (FileExists(allInOne)) {
        size_t sz = 0;
        void* data = LoadBinaryFile(allInOne, sz);
        if (data && sz == 0x4000) {
            memcpy(g_mz800_sys.rom, data, 0x4000);
            free(data);
            bRomLoaded = true;
            printf("MZ800: ROM loaded from '%s'\n", allInOne);
            return true;
        }
        if (data) free(data);
        fprintf(stderr, "MZ800: '%s' wrong size (%zu, expected 16384)\n", allInOne, sz);
    }

    // --- try three separate files ---
    struct { const char* cfgPath; uint32_t offset; uint32_t size; } parts[3] = {
        { cfg->Rom700.c_str(), 0x0000, 0x1000 },
        { cfg->RomCG.c_str(),  0x1000, 0x1000 },
        { cfg->Rom800.c_str(), 0x2000, 0x2000 },
    };
    bool ok = true;
    for (auto& p : parts) {
        const char* full = GetBundlePath(p.cfgPath);
        size_t sz = 0;
        void* data = LoadBinaryFile(full, sz);
        if (data && sz == p.size) {
            memcpy(g_mz800_sys.rom + p.offset, data, p.size);
            free(data);
        } else {
            fprintf(stderr, "MZ800: ROM part '%s' not found or wrong size\n", full);
            if (data) free(data);
            ok = false;
        }
    }
    if (ok) {
        bRomLoaded = true;
        printf("MZ800: ROM loaded from three separate files\n");
        return true;
    }

    fprintf(stderr, "MZ800: ROM not loaded — place mz800.rom (16KB) in Contents/MacOS/Roms/\n");
    return false;
}

// ---------------------------------------------------------------------------
// MZF TAPE LOADING
// MZF: 128-byte header followed by program body.
// The buffer is kept alive in MzfBuffer so sys->cmt.body remains valid.
// ---------------------------------------------------------------------------
bool FMZ800Emu::LoadMZF(const char* path)
{
    size_t sz = 0;
    void* data = LoadBinaryFile(path, sz);
    if (!data) {
        fprintf(stderr, "MZ800: Cannot open MZF '%s'\n", path);
        return false;
    }
    if (sz < MZF_HEADER_SIZE) {
        fprintf(stderr, "MZ800: MZF '%s' too small (%zu bytes)\n", path, sz);
        free(data);
        return false;
    }
    MzfBuffer.assign((uint8_t*)data, (uint8_t*)data + sz);
    free(data);
    bool ok = mz800_sys_load_mzf(&g_mz800_sys, MzfBuffer.data(), (uint32_t)MzfBuffer.size());
    if (ok)
        printf("MZ800: MZF loaded '%s' (%zu bytes)\n", path, sz);
    else
        fprintf(stderr, "MZ800: mz800_sys_load_mzf failed for '%s'\n", path);
    return ok;
}

bool FMZ800Emu::Init(const FEmulatorLaunchConfig& launchConfig)
{
    printf("MZ800 Init called!\n");
    if (!FEmuBase::Init(launchConfig)) {
        printf("FEmuBase::Init failed!\n");
        return false;
    }
    printf("FEmuBase::Init succeeded!\n");
    
    // Create Global Config
    pGlobalConfig = new FMZ800GlobalConfig();
    pGlobalConfig->Init(); pGlobalConfig->bBuiltInFont = true;
    CodeAnalysis.SetGlobalConfig(pGlobalConfig);
    
    SetWindowTitle("MZ-800 Analyser");
    
    mz800_sys_init(&g_mz800_sys);

    // Apply PAL/NTSC from config
    FMZ800GlobalConfig* cfg = static_cast<FMZ800GlobalConfig*>(pGlobalConfig);
    gdg_whid65040_set_video_standard(&g_mz800_sys.gdg, cfg->bPAL);

    // Setup audio
    {
        chips_audio_callback_t cb = {};
        cb.func = MZ800PushAudio;
        cb.user_data = this;
        mz800_sys_setup_audio(&g_mz800_sys, cb, saudio_sample_rate(), 128);
    }

    // Load monitor ROM
    LoadROM();

    // Auto-load last MZF if configured
    if (!cfg->MzfPath.empty() && FileExists(cfg->MzfPath.c_str()))
        LoadMZF(cfg->MzfPath.c_str());

    // FrameBuffer and ScreenTexture are created lazily in DrawEmulatorUI
    // when the GL context is guaranteed to be active.

    LoadFont();
    
    CPUType = ECPUType::Z80;
    
    // Initialize memory banks
    MainRAMBankId = CodeAnalysis.CreateBank("MainRAM", 64, g_mz800_sys.ram, false, 0x0000, true);
    CodeAnalysis.MapBank(MainRAMBankId, 0, EBankAccess::ReadWrite);

    // ROM banks (read-only)
    // MZ-700 monitor ROM: 4KB at 0x0000
    ROM0000BankId = CodeAnalysis.CreateBank("ROM Monitor 700", 4, &g_mz800_sys.rom[0x0000], true, 0x0000);
    // CGROM: 4KB at 0x1000
    ROM1000BankId = CodeAnalysis.CreateBank("ROM CGROM", 4, &g_mz800_sys.rom[0x1000], true, 0x1000);
    // MZ-800 monitor ROM: 8KB at 0xE000
    ROME000BankId = CodeAnalysis.CreateBank("ROM Monitor 800", 8, &g_mz800_sys.rom[0x2000], true, 0xE000);

    // Map ROM banks that are active at boot
    if (g_mz800_sys.gdg.bank_rom0000)
        CodeAnalysis.MapBank(ROM0000BankId, 0, EBankAccess::Read);
    if (g_mz800_sys.gdg.bank_rom1000)
        CodeAnalysis.MapBank(ROM1000BankId, 4, EBankAccess::Read);
    if (g_mz800_sys.gdg.bank_rome000)
        CodeAnalysis.MapBank(ROME000BankId, 56, EBankAccess::Read);

    CodeAnalysis.Init(this);
    CodeAnalysis.ViewState[0].Enabled = true;
    CodeAnalysis.ViewState[0].GoToAddress(FAddressRef(0x0000, 0));

    // Register IO analyzer devices
    PITDevice.Init("i8253 PIT", this, &g_mz800_sys.pit);
    PSGDevice.Init("SN76489AN", this, &g_mz800_sys.psg);
    PIODevice.Init("Z80 PIO", this, &g_mz800_sys.pio);
    GDGDevice.Init("GDG WHID 65040-032", this, &g_mz800_sys.gdg);

    // Wire C write hooks so the IODevices receive write logs
    g_mz800_sys.hook_user = this;
    g_mz800_sys.cpu_tick_hook = FMZ800Emu::CPUTickHook;
    g_mz800_sys.pit_write_hook = [](void* user, uint16_t pc, uint16_t port, uint8_t data) {
        FMZ800Emu* emu = (FMZ800Emu*)user;
        FAddressRef pcRef;
        pcRef.Address = pc;
        emu->PITDevice.WritePIT(pcRef, port, data);
    };
    g_mz800_sys.psg_write_hook = [](void* user, uint16_t pc, uint8_t data) {
        FMZ800Emu* emu = (FMZ800Emu*)user;
        FAddressRef pcRef;
        pcRef.Address = pc;
        emu->PSGDevice.WritePSG(pcRef, data);
    };
    g_mz800_sys.gdg_write_hook = [](void* user, uint16_t pc, uint16_t port, uint8_t data) {
        FMZ800Emu* emu = (FMZ800Emu*)user;
        FAddressRef pcRef;
        pcRef.Address = pc;
        emu->GDGDevice.WriteGDG(pcRef, port, data);
    };

    printf("FMZ800Emu::Init almost finished returning true\n");
    return true;
}

void FMZ800Emu::Shutdown()
{
    if (ScreenTexture) { ImGui_FreeTexture(ScreenTexture); ScreenTexture = nullptr; }
    delete[] FrameBuffer; FrameBuffer = nullptr;
    FEmuBase::Shutdown();
    delete pGlobalConfig;
}

void FMZ800Emu::Tick()
{
    FEmuBase::Tick();

    FDebugger& debugger = CodeAnalysis.Debugger;

    if (debugger.IsStopped() == false)
    {
        const float frameTime = std::min(1000000.0f / ImGui::GetIO().Framerate, 32000.0f);
        const uint32_t microSeconds = std::max(static_cast<uint32_t>(frameTime), uint32_t(1));

        static int frame_cnt = 0;
        if (frame_cnt++ < 3) fprintf(stderr, "MZ800: Tick frame %d, %u us\n", frame_cnt, microSeconds);

        CodeAnalysis.OnFrameStart();
        StoreRegisters_Z80(CodeAnalysis);
        mz800_sys_exec(&g_mz800_sys, microSeconds);
        CodeAnalysis.OnFrameEnd();
    }
    
    // Update Floooh keyboard matrix
    const uint32_t frame_time_us = static_cast<FMZ800GlobalConfig*>(pGlobalConfig)->bPAL ? 20000 : 16667;
    kbd_update(&g_mz800_sys.kbd, frame_time_us); 

    // Draw UI
    DrawDockingView();
}

void FMZ800Emu::Reset()
{
    FEmuBase::Reset();
    mz800_sys_reset(&g_mz800_sys);
}

bool FMZ800Emu::LoadEmulatorFile(const FEmulatorFile* pSnapshot)
{
    return false;
}

bool FMZ800Emu::NewProjectFromEmulatorFile(const FEmulatorFile& gameSnapshot)
{
    return false;
}

bool FMZ800Emu::LoadProject(FProjectConfig* pConfig, bool bLoadGame)
{
    return false;
}

bool FMZ800Emu::SaveProject()
{
    return false;
}

void FMZ800Emu::DrawEmulatorUI()
{
    if (ImGui::Begin("MZ-800 Screen"))
    {
        bool bWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
        if (bWindowFocused)
        {
            for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_COUNT; key++)
            {
                if (ImGui::IsKeyPressed((ImGuiKey)key,false))
                { 
                    int mz_key = MZ800KeyFromImGuiKey((ImGuiKey)key);
                    if (mz_key != 0)
                        mz800_sys_key_down(&g_mz800_sys, mz_key);
                }
                else if (ImGui::IsKeyReleased((ImGuiKey)key))
                {
                    int mz_key = MZ800KeyFromImGuiKey((ImGuiKey)key);
                    if (mz_key != 0)
                        mz800_sys_key_up(&g_mz800_sys, mz_key);
                }
            }
        }

        FMZ800GlobalConfig* cfg = static_cast<FMZ800GlobalConfig*>(pGlobalConfig);

        ImGui::Text("Tick: %d  PC: %04X", g_mz800_sys.tick_count, g_mz800_sys.cpu.pc);

        // --- ROM status ---
        ImGui::Separator();
        ImGui::Text("ROM: %s", bRomLoaded ? "Loaded" : "NOT LOADED (place mz800.rom in Roms/)");
        if (!bRomLoaded && ImGui::Button("Reload ROM"))
            LoadROM();

        // --- PAL/NTSC runtime toggle ---
        ImGui::Separator();
        bool isPAL = g_mz800_sys.gdg.bPAL;
        if (ImGui::RadioButton("PAL (50Hz)", isPAL)) {
            gdg_whid65040_set_video_standard(&g_mz800_sys.gdg, true);
            cfg->bPAL = true;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("NTSC (60Hz)", !isPAL)) {
            gdg_whid65040_set_video_standard(&g_mz800_sys.gdg, false);
            cfg->bPAL = false;
        }

        // --- MZF tape loader ---
        ImGui::Separator();
        ImGui::Text("Tape / MZF");

        // Show currently loaded MZF info
        if (g_mz800_sys.cmt.has_file) {
            // Filename is at header[1], 16 chars, 0x0D terminated
            char mzfName[18] = {};
            for (int i = 0; i < 16; i++) {
                uint8_t c = g_mz800_sys.cmt.header[1 + i];
                if (c == 0x0D || c == 0x00) break;
                mzfName[i] = (char)c;
            }
            uint16_t fsize = (uint16_t)(g_mz800_sys.cmt.header[18] | (g_mz800_sys.cmt.header[19] << 8));
            uint16_t fload = (uint16_t)(g_mz800_sys.cmt.header[20] | (g_mz800_sys.cmt.header[21] << 8));
            uint16_t fexec = (uint16_t)(g_mz800_sys.cmt.header[22] | (g_mz800_sys.cmt.header[23] << 8));
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f),
                "[%s]  size=%04Xh  load=%04Xh  exec=%04Xh", mzfName, fsize, fload, fexec);
        } else {
            ImGui::TextDisabled("No tape loaded");
        }

        // File path input + load button
        static char mzfPathBuf[512] = {};
        if (mzfPathBuf[0] == 0 && !cfg->MzfPath.empty())
            snprintf(mzfPathBuf, sizeof(mzfPathBuf), "%s", cfg->MzfPath.c_str());
        ImGui::SetNextItemWidth(-80.0f);
        ImGui::InputText("##mzfpath", mzfPathBuf, sizeof(mzfPathBuf));
        ImGui::SameLine();
        if (ImGui::Button("Load MZF")) {
            if (mzfPathBuf[0] != 0) {
                if (LoadMZF(mzfPathBuf)) {
                    cfg->MzfPath = mzfPathBuf;
                }
            }
        }

        // --- Display the screen ---
        ImGui::Separator();

        // Convert indexed framebuffer to RGBA and update GPU texture
        chips_display_info_t disp = gdg_whid65040_display_info(&g_mz800_sys.gdg);


        const int fbW = disp.frame.dim.width;
        const int fbH = disp.frame.dim.height;

        // Lazy texture creation (ensures GL context is active)
        if (!ScreenTexture && fbW > 0 && fbH > 0)
        {
            if (!FrameBuffer)
            {
                FrameBuffer = new uint32_t[(size_t)fbW * fbH];
                memset(FrameBuffer, 0, (size_t)fbW * fbH * sizeof(uint32_t));
            }
            ScreenTexture = ImGui_CreateTextureRGBA(FrameBuffer, fbW, fbH);
        }

        if (FrameBuffer && ScreenTexture)
        {
            const uint8_t* pix = (const uint8_t*)disp.frame.buffer.ptr;
            const uint32_t* pal = (const uint32_t*)disp.palette.ptr;
            const size_t num_pixels = (size_t)fbW * fbH;
            for (size_t i = 0; i < num_pixels; i++)
                FrameBuffer[i] = pal[pix[i] & 0x0F];

            ImGui_UpdateTextureRGBA(ScreenTexture, FrameBuffer);

            // Diagnostic: dump VRAM state once after ~120 frames
            {
                static int diag_frame = 0;
                diag_frame++;
                if (diag_frame == 120) {
                    FILE* df = fopen("/tmp/mz800_diag.txt", "w");
                    if (df) {
                    const gdg_whid65040_t* gdg = &g_mz800_sys.gdg;
                    // CG-RAM occupancy (vram[0][0x0000-0x0FFF])
                    int cgram_nonzero = 0;
                    for (int i = 0; i < 0x1000; i++)
                        if (gdg->vram[0][i] != 0) cgram_nonzero++;
                    // Char code occupancy (vram[0][0x1000-0x17FF])
                    int charcode_nonzero = 0;
                    for (int i = 0x1000; i < 0x1800; i++)
                        if (gdg->vram[0][i] != 0) charcode_nonzero++;
                    // Attr occupancy (vram[0][0x1800-0x1FFF])
                    int attr_nonzero = 0;
                    for (int i = 0x1800; i < 0x2000; i++)
                        if (gdg->vram[0][i] != 0) attr_nonzero++;
                    // Indexed framebuffer non-zero pixels
                    int fb_nonzero = 0;
                    for (size_t i = 0; i < num_pixels; i++)
                        if (pix[i] != 0) fb_nonzero++;
                    // RGBA non-black pixels
                    int rgba_nonblack = 0;
                    for (size_t i = 0; i < num_pixels; i++)
                        if (FrameBuffer[i] != 0xFF000000 && FrameBuffer[i] != 0) rgba_nonblack++;

                    fprintf(df, "DMD=%02X banks: rom0=%d rom1=%d romE=%d vram=%d\n",
                        gdg->dmd, gdg->bank_rom0000, gdg->bank_rom1000,
                        gdg->bank_rome000, gdg->bank_vram);
                    fprintf(df, "CG-RAM nonzero: %d/4096\n", cgram_nonzero);
                    fprintf(df, "CharCodes nonzero: %d/2048  Attrs nonzero: %d/2048\n",
                        charcode_nonzero, attr_nonzero);
                    fprintf(df, "First 10 charcodes: ");
                    for (int i = 0; i < 10; i++) fprintf(df, "%02X ", gdg->vram[0][0x1000 + i]);
                    fprintf(df, "\n");
                    fprintf(df, "First 10 attrs: ");
                    for (int i = 0; i < 10; i++) fprintf(df, "%02X ", gdg->vram[0][0x1800 + i]);
                    fprintf(df, "\n");
                    fprintf(df, "CG-RAM 'A'(0x41) pattern: ");
                    for (int i = 0; i < 8; i++) fprintf(df, "%02X ", gdg->vram[0][0x41 * 8 + i]);
                    fprintf(df, "\n");
                    fprintf(df, "CGROM 'A'(0x41) pattern: ");
                    for (int i = 0; i < 8; i++) fprintf(df, "%02X ", g_mz800_sys.rom[0x1000 + 0x41 * 8 + i]);
                    fprintf(df, "\n");
                    fprintf(df, "FB indexed nonzero: %d/%zu  RGBA nonblack: %d/%zu\n",
                        fb_nonzero, num_pixels, rgba_nonblack, num_pixels);
                    fprintf(df, "Screen rect: x=%d y=%d w=%d h=%d  Frame: %dx%d\n",
                        disp.screen.x, disp.screen.y, disp.screen.width, disp.screen.height, fbW, fbH);
                    fprintf(df, "canvas: first=%d last=%d  active: start=%d end=%d\n",
                        gdg->vt.canvas_first_line, gdg->vt.canvas_last_line,
                        gdg->vt.active_start, gdg->vt.active_end);
                    fprintf(df, "PC=%04X tick_count=%u\n", g_mz800_sys.cpu.pc, g_mz800_sys.tick_count);
                    fprintf(df, "vdisp: first=%d last=%d  border_left=%d border_right=%d\n",
                        gdg->vt.vdisp_first_line, gdg->vt.vdisp_last_line,
                        gdg->vt.border_left_start, gdg->vt.border_right_end);
                    // Dump ROM bytes around current PC
                    uint16_t pc = g_mz800_sys.cpu.pc;
                    fprintf(df, "ROM bytes at PC=%04X: ", pc);
                    for (int i = -4; i < 16; i++) {
                        uint16_t a = pc + i;
                        uint8_t b;
                        if (a >= 0xE000 && g_mz800_sys.gdg.bank_rome000)
                            b = g_mz800_sys.rom[a & 0x3FFF];
                        else
                            b = g_mz800_sys.ram[a];
                        fprintf(df, "%02X ", b);
                    }
                    fprintf(df, "\n");
                    // Dump ROM at E800-E840 to see boot sequence
                    fprintf(df, "ROM E800-E840: ");
                    for (int i = 0; i < 0x40; i++)
                        fprintf(df, "%02X ", g_mz800_sys.rom[0x2800 + i]);
                    fprintf(df, "\n");
                    // Check if CPU is in a HALT or tight loop
                    fprintf(df, "CPU: halted=%d iff1=%d iff2=%d\n",
                        (g_mz800_sys.cpu.pins & Z80_HALT) ? 1 : 0,
                        g_mz800_sys.cpu.iff1, g_mz800_sys.cpu.iff2);
                    fclose(df);
                    }
                    // Write screen capture as PPM for visual inspection
                    FILE* ppm = fopen("/tmp/mz800_screen.ppm", "wb");
                    if (ppm) {
                        int sx = disp.screen.x, sy = disp.screen.y;
                        int sw = disp.screen.width, sh = disp.screen.height;
                        fprintf(ppm, "P6\n%d %d\n255\n", sw, sh);
                        for (int y = 0; y < sh; y++) {
                            for (int x = 0; x < sw; x++) {
                                uint32_t rgba = FrameBuffer[(sy + y) * fbW + (sx + x)];
                                uint8_t r = rgba & 0xFF;
                                uint8_t g = (rgba >> 8) & 0xFF;
                                uint8_t b = (rgba >> 16) & 0xFF;
                                fputc(r, ppm); fputc(g, ppm); fputc(b, ppm);
                            }
                        }
                        fclose(ppm);
                    }
                }
            }

            // UV crop to visible screen area
            const chips_rect_t scr = disp.screen;
            const ImVec2 uv0((float)scr.x / fbW, (float)scr.y / fbH);
            const ImVec2 uv1((float)(scr.x + scr.width) / fbW, (float)(scr.y + scr.height) / fbH);

            // Scale to fit window content area maintaining 4:3 CRT aspect ratio
            ImVec2 avail = ImGui::GetContentRegionAvail();
            const float targetAspect = 4.0f / 3.0f;
            float displayW, displayH;
            if (avail.x / avail.y > targetAspect) {
                displayH = avail.y;
                displayW = displayH * targetAspect;
            } else {
                displayW = avail.x;
                displayH = displayW / targetAspect;
            }
            if (displayW < 64.0f) displayW = 64.0f;
            if (displayH < 48.0f) displayH = 48.0f;

            ImGui::Image(ScreenTexture, ImVec2(displayW, displayH), uv0, uv1);
        }
    }
    ImGui::End();
}

uint8_t FMZ800Emu::ReadByte(uint16_t address) const { 
    const mz800_sys_t* sys = &g_mz800_sys;
    uint8_t addr_hi = address >> 12;

    // ROM at 0x0000-0x0FFF (MZ-700 monitor)
    if (addr_hi == 0x00 && sys->gdg.bank_rom0000)
        return sys->rom[address];
    // CGROM at 0x1000-0x1FFF
    if (addr_hi == 0x01 && sys->gdg.bank_rom1000)
        return sys->rom[address];
    // MZ-700 CG-RAM at 0xC000-0xCFFF
    if (addr_hi == 0x0C && (sys->gdg.dmd & GDG_DMD_MZ700) && sys->gdg.bank_vram)
        return sys->gdg.vram[0][address & 0x0FFF];
    // MZ-700 char/attr VRAM at 0xD000-0xDFFF
    if (addr_hi == 0x0D && (sys->gdg.dmd & GDG_DMD_MZ700) && sys->gdg.bank_rome000)
        return sys->gdg.vram[0][0x1000 | (address & 0x0FFF)];
    // ROM at 0xE000-0xFFFF (MZ-800 monitor)
    if ((addr_hi == 0x0E || addr_hi == 0x0F) && sys->gdg.bank_rome000) {
        if (addr_hi == 0x0E && (address & 0x0FFF) <= 0x08)
            return sys->ram[address]; // hardware register region
        return sys->rom[address & 0x3FFF];
    }
    return sys->ram[address]; 
}

uint16_t FMZ800Emu::ReadWord(uint16_t address) const { 
    return (uint16_t)ReadByte(address) | ((uint16_t)ReadByte((address + 1) & 0xFFFF) << 8); 
}

const uint8_t* FMZ800Emu::GetMemPtr(uint16_t address) const { 
    const mz800_sys_t* sys = &g_mz800_sys;
    uint8_t addr_hi = address >> 12;

    if (addr_hi == 0x00 && sys->gdg.bank_rom0000)
        return &sys->rom[address];
    if (addr_hi == 0x01 && sys->gdg.bank_rom1000)
        return &sys->rom[address];
    if ((addr_hi == 0x0E || addr_hi == 0x0F) && sys->gdg.bank_rome000) {
        if (addr_hi == 0x0E && (address & 0x0FFF) <= 0x08)
            return &sys->ram[address];
        return &sys->rom[address & 0x3FFF];
    }
    return &sys->ram[address]; 
}

void FMZ800Emu::WriteByte(uint16_t address, uint8_t value) {
    const mz800_sys_t* sys = &g_mz800_sys;
    uint8_t addr_hi = address >> 12;
    // Protect ROM areas from writes
    if (addr_hi == 0x00 && sys->gdg.bank_rom0000) return;
    if (addr_hi == 0x01 && sys->gdg.bank_rom1000) return;
    if ((addr_hi == 0x0E || addr_hi == 0x0F) && sys->gdg.bank_rome000) return;
    g_mz800_sys.ram[address] = value;
}

FAddressRef FMZ800Emu::GetPC(void) { 
    return CodeAnalysis.Debugger.GetPC(); 
}

uint16_t FMZ800Emu::GetSP(void) { 
    return g_mz800_sys.cpu.sp; 
}

void* FMZ800Emu::GetCPUEmulator(void) const {
    return (void*)&g_mz800_sys.cpu;
}
