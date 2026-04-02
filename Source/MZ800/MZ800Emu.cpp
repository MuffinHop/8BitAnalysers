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
    if (sz < CMT_MZF_HEADER_SIZE) {
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

// ---------------------------------------------------------------------------
// WAV TAPE LOADING
// Parses a WAV file and converts the audio to a binary signal for CMT playback.
// Supports 8-bit and 16-bit PCM WAV, mono and stereo.
// ---------------------------------------------------------------------------
bool FMZ800Emu::LoadWAV(const char* path)
{
    size_t sz = 0;
    void* data = LoadBinaryFile(path, sz);
    if (!data) {
        fprintf(stderr, "MZ800: Cannot open WAV '%s'\n", path);
        return false;
    }
    const uint8_t* buf = (const uint8_t*)data;

    // Validate RIFF/WAVE header
    if (sz < 44 || memcmp(buf, "RIFF", 4) != 0 || memcmp(buf + 8, "WAVE", 4) != 0) {
        fprintf(stderr, "MZ800: Not a valid WAV file '%s'\n", path);
        free(data);
        return false;
    }

    // Find "fmt " chunk
    uint32_t pos = 12;
    uint16_t audio_format = 0, num_channels = 0, bits_per_sample = 0;
    uint32_t sample_rate = 0;
    bool fmt_found = false;

    while (pos + 8 <= sz) {
        uint32_t chunk_size = buf[pos+4] | (buf[pos+5] << 8) | (buf[pos+6] << 16) | (buf[pos+7] << 24);
        if (memcmp(buf + pos, "fmt ", 4) == 0 && chunk_size >= 16) {
            audio_format    = buf[pos+8]  | (buf[pos+9]  << 8);
            num_channels    = buf[pos+10] | (buf[pos+11] << 8);
            sample_rate     = buf[pos+12] | (buf[pos+13] << 8) | (buf[pos+14] << 16) | (buf[pos+15] << 24);
            bits_per_sample = buf[pos+22] | (buf[pos+23] << 8);
            fmt_found = true;
        }
        if (memcmp(buf + pos, "data", 4) == 0) {
            break;  // data chunk found
        }
        pos += 8 + chunk_size;
        if (chunk_size & 1) pos++;  // word-align
    }

    if (!fmt_found || audio_format != 1 /* PCM */ || sample_rate == 0) {
        fprintf(stderr, "MZ800: Unsupported WAV format in '%s' (format=%u)\n", path, audio_format);
        free(data);
        return false;
    }

    // Find "data" chunk
    pos = 12;
    const uint8_t* pcm_data = nullptr;
    uint32_t pcm_size = 0;
    while (pos + 8 <= sz) {
        uint32_t chunk_size = buf[pos+4] | (buf[pos+5] << 8) | (buf[pos+6] << 16) | (buf[pos+7] << 24);
        if (memcmp(buf + pos, "data", 4) == 0) {
            pcm_data = buf + pos + 8;
            pcm_size = chunk_size;
            if (pos + 8 + pcm_size > sz) pcm_size = (uint32_t)(sz - pos - 8);
            break;
        }
        pos += 8 + chunk_size;
        if (chunk_size & 1) pos++;
    }

    if (!pcm_data || pcm_size == 0) {
        fprintf(stderr, "MZ800: No data chunk in WAV '%s'\n", path);
        free(data);
        return false;
    }

    // Calculate number of mono samples
    uint32_t bytes_per_sample = (bits_per_sample / 8) * num_channels;
    if (bytes_per_sample == 0) { free(data); return false; }
    uint32_t num_samples = pcm_size / bytes_per_sample;

    // Convert PCM to binary signal (0/1) based on zero-crossing
    uint8_t* signal = (uint8_t*)malloc(num_samples);
    if (!signal) { free(data); return false; }

    for (uint32_t i = 0; i < num_samples; i++) {
        uint32_t offset = i * bytes_per_sample;
        int16_t sample = 0;

        if (bits_per_sample == 8) {
            // 8-bit WAV is unsigned, center at 128
            sample = (int16_t)pcm_data[offset] - 128;
        } else if (bits_per_sample == 16) {
            // 16-bit WAV is signed little-endian
            sample = (int16_t)(pcm_data[offset] | (pcm_data[offset + 1] << 8));
        }
        // For stereo, just use the first channel

        signal[i] = (sample >= 0) ? 1 : 0;
    }

    free(data);

    // Load into CMT (takes ownership of signal buffer)
    bool ok = cmt_load_wav_signal(&g_mz800_sys.cmt, signal, num_samples, sample_rate);
    if (ok)
        printf("MZ800: WAV loaded '%s' (%u samples @ %u Hz)\n", path, num_samples, sample_rate);
    else {
        fprintf(stderr, "MZ800: WAV load failed for '%s'\n", path);
        free(signal);
    }
    return ok;
}

// ---------------------------------------------------------------------------
// MZQ QUICK DISK LOADING
// Parses an MZQ container file, extracts the first MZF file, loads to CMT.
// ---------------------------------------------------------------------------
bool FMZ800Emu::LoadMZQ(const char* path)
{
    size_t sz = 0;
    void* data = LoadBinaryFile(path, sz);
    if (!data) {
        fprintf(stderr, "MZ800: Cannot open MZQ '%s'\n", path);
        return false;
    }

    MzfBuffer.assign((uint8_t*)data, (uint8_t*)data + sz);
    free(data);

    bool ok = cmt_load_mzq(&g_mz800_sys.cmt, MzfBuffer.data(), (uint32_t)MzfBuffer.size());
    if (ok)
        printf("MZ800: MZQ loaded '%s' (%zu bytes)\n", path, sz);
    else
        fprintf(stderr, "MZ800: MZQ parse failed for '%s'\n", path);
    return ok;
}

// ---------------------------------------------------------------------------
// DSK FLOPPY DISK LOADING
// Supports Extended CPC DSK images directly and raw sector dumps.
// Raw images are auto-detected and wrapped (16 sectors/track, 256 bytes/sector).
// ---------------------------------------------------------------------------
bool FMZ800Emu::LoadDSK(const char* path, int drive)
{
    if (drive < 0 || drive >= WD2793_NUM_DRIVES) return false;

    size_t sz = 0;
    void* data = LoadBinaryFile(path, sz);
    if (!data) {
        fprintf(stderr, "MZ800: Cannot open DSK '%s'\n", path);
        return false;
    }

    DskBuffer[drive].assign((uint8_t*)data, (uint8_t*)data + sz);
    free(data);

    const uint8_t* dsk_data = DskBuffer[drive].data();
    uint32_t dsk_size = (uint32_t)DskBuffer[drive].size();

    // Try Extended CPC DSK format first
    if (dsk_size >= 0x100 && memcmp(dsk_data, "EXTENDED CPC DSK File\r\nDisk-Info\r\n", 34) == 0) {
        bool ok = wd2793_insert_disk(&g_mz800_sys.fdc, drive, dsk_data, dsk_size, false);
        if (ok)
            printf("MZ800: CPC DSK loaded into FD%d '%s' (%u bytes)\n", drive, path, dsk_size);
        else
            fprintf(stderr, "MZ800: CPC DSK insert failed for FD%d '%s'\n", drive, path);
        return ok;
    }

    // Auto-detect raw sector dump: MZ-800 standard = 16 sectors × 256 bytes
    // Common sizes: 163840 (1S/40T), 327680 (2S/40T), 655360 (2S/80T)
    uint8_t num_sides = 1;
    if (dsk_size == 327680 || dsk_size == 655360) num_sides = 2;
    // Also accept non-standard sizes as single-sided
    bool ok = wd2793_insert_raw_disk(&g_mz800_sys.fdc, drive, dsk_data, dsk_size, 16, 256, num_sides);
    if (ok)
        printf("MZ800: Raw DSK loaded into FD%d '%s' (%u bytes, %d sides)\n", drive, path, dsk_size, num_sides);
    else
        fprintf(stderr, "MZ800: Raw DSK insert failed for FD%d '%s'\n", drive, path);
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
    PPIDevice.Init("i8255 PPI", this, &g_mz800_sys.ppi);
    FDCDevice.Init("WD2793 FDC", this, &g_mz800_sys.fdc);
    CMTDevice.Init("CMT Tape", this, &g_mz800_sys.cmt);

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
    g_mz800_sys.ppi_write_hook = [](void* user, uint16_t pc, uint16_t port, uint8_t data) {
        FMZ800Emu* emu = (FMZ800Emu*)user;
        FAddressRef pcRef;
        pcRef.Address = pc;
        emu->PPIDevice.WritePPI(pcRef, port, data);
    };
    g_mz800_sys.fdc_write_hook = [](void* user, uint16_t pc, uint16_t port, uint8_t data) {
        FMZ800Emu* emu = (FMZ800Emu*)user;
        FAddressRef pcRef;
        pcRef.Address = pc;
        emu->FDCDevice.WriteFDC(pcRef, port, data);
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

        ImGui::Text("Tick: %llu  PC: %04X", g_mz800_sys.tick_count, g_mz800_sys.cpu.pc);

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
        ImGui::Text("Tape (CMT)");

        // Show currently loaded tape info
        if (g_mz800_sys.cmt.has_file) {
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
            if (g_mz800_sys.cmt.wav_mode) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "(WAV)");
            }
            float progress = cmt_get_progress(&g_mz800_sys.cmt);
            ImGui::ProgressBar(progress, ImVec2(-1, 0));
        } else {
            ImGui::TextDisabled("No tape loaded");
        }

        // Load tape file (auto-detects format by extension)
        if (ImGui::Button("Load Tape...")) {
            std::string picked;
            if (OpenFileDialog(picked, nullptr, "Tape Files\0*.mzf;*.wav;*.mzq\0MZF Files\0*.mzf\0WAV Files\0*.wav\0MZQ Files\0*.mzq\0All Files\0*.*\0")) {
                const char* ext = strrchr(picked.c_str(), '.');
                if (ext) {
                    if (strcasecmp(ext, ".mzf") == 0) {
                        if (LoadMZF(picked.c_str()))
                            cfg->MzfPath = picked;
                    } else if (strcasecmp(ext, ".wav") == 0) {
                        LoadWAV(picked.c_str());
                    } else if (strcasecmp(ext, ".mzq") == 0) {
                        LoadMZQ(picked.c_str());
                    }
                }
            }
        }

        // Tape transport controls
        if (g_mz800_sys.cmt.has_file) {
            if (ImGui::Button("Play")) cmt_play(&g_mz800_sys.cmt);
            ImGui::SameLine();
            if (ImGui::Button("Stop")) cmt_stop(&g_mz800_sys.cmt);
            ImGui::SameLine();
            if (ImGui::Button("Rewind")) {
                if (g_mz800_sys.cmt.wav_mode) {
                    g_mz800_sys.cmt.wav_signal_pos = 0;
                    g_mz800_sys.cmt.wav_tick_acc = 0;
                } else {
                    g_mz800_sys.cmt.phase = CMT_PHASE_LEADER;
                    g_mz800_sys.cmt.current_block = 0;
                    g_mz800_sys.cmt.byte_pos = 0;
                    g_mz800_sys.cmt.bit_pos = 0;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Eject")) cmt_eject(&g_mz800_sys.cmt);
            ImGui::SameLine();
            ImGui::Text("Motor: %s  State: %s",
                g_mz800_sys.cmt.motor_on ? "ON" : "OFF",
                g_mz800_sys.cmt.state == CMT_STATE_PLAYING ? "Playing" :
                g_mz800_sys.cmt.state == CMT_STATE_STOPPED ? "Stopped" : "Empty");
        }

        // --- Floppy Disk (FDC) ---
        ImGui::Separator();
        ImGui::Text("Floppy Disk (WD2793)");

        static int selectedDrive = 0;
        ImGui::SetNextItemWidth(50.0f);
        ImGui::Combo("##drv", &selectedDrive, "FD0\0FD1\0FD2\0FD3\0");
        ImGui::SameLine();
        if (ImGui::Button("Insert Disk...")) {
            std::string picked;
            if (OpenFileDialog(picked, nullptr, "Disk Images\0*.dsk\0All Files\0*.*\0"))
                LoadDSK(picked.c_str(), selectedDrive);
        }

        // Show drive status
        for (int d = 0; d < WD2793_NUM_DRIVES; d++) {
            const wd2793_drive_t& drv = g_mz800_sys.fdc.drive[d];
            if (drv.dsk_present) {
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f),
                    "FD%d: %uKB  track=%d side=%d",
                    d, drv.dsk_size / 1024, drv.current_track, drv.current_side);
                ImGui::SameLine();
                char ejectLabel[16];
                snprintf(ejectLabel, sizeof(ejectLabel), "Eject##fd%d", d);
                if (ImGui::SmallButton(ejectLabel))
                    wd2793_eject_disk(&g_mz800_sys.fdc, d);
            } else {
                ImGui::TextDisabled("FD%d: Empty", d);
            }
        }

        // --- Display the screen ---
        ImGui::Separator();

        // Convert indexed framebuffer to RGBA and update GPU texture
        chips_display_info_t disp = gdg_whid65040_display_info(&g_mz800_sys.gdg);


        const int fbW = disp.frame.dim.width;
        const int fbH = disp.frame.dim.height;

        // Recreate texture when dimensions change (PAL<->NTSC)
        if (fbW != FBWidth || fbH != FBHeight)
        {
            if (ScreenTexture) { ImGui_FreeTexture(ScreenTexture); ScreenTexture = nullptr; }
            delete[] FrameBuffer; FrameBuffer = nullptr;
            FBWidth = fbW;
            FBHeight = fbH;
        }
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
