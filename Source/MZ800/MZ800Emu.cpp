#include "MZ800Emu.h"
#include "MZ800ChipsImpl.h"
#include "MZ800Config.h"
#include <imGui/imgui.h>
#include <Util/FileUtil.h>
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
    mz800_set_video_standard(&g_mz800_sys, cfg->bPAL);

    // Load monitor ROM
    LoadROM();

    // Auto-load last MZF if configured
    if (!cfg->MzfPath.empty() && FileExists(cfg->MzfPath.c_str()))
        LoadMZF(cfg->MzfPath.c_str());

    LoadFont();
    
    CPUType = ECPUType::Z80;
    
    // Initialize standard memory
    MainRAMBankId = CodeAnalysis.CreateBank("MainRAM", 64, g_mz800_sys.ram, false, 0x0000, true);
    CodeAnalysis.MapBank(MainRAMBankId, 0, EBankAccess::ReadWrite);
    CodeAnalysis.Init(this);
    CodeAnalysis.ViewState[0].Enabled = true;

    printf("FMZ800Emu::Init almost finished returning true\n");
    return true;
}

void FMZ800Emu::Shutdown()
{
    FEmuBase::Shutdown();
    delete pGlobalConfig;
}

void FMZ800Emu::Tick()
{
    FEmuBase::Tick();
    mz800_sys_tick(&g_mz800_sys);
    
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
        bool isPAL = g_mz800_sys.bPAL;
        if (ImGui::RadioButton("PAL (50Hz)", isPAL)) {
            mz800_set_video_standard(&g_mz800_sys, true);
            cfg->bPAL = true;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("NTSC (60Hz)", !isPAL)) {
            mz800_set_video_standard(&g_mz800_sys, false);
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
    }
    ImGui::End();
}

uint8_t FMZ800Emu::ReadByte(uint16_t address) const { 
    return g_mz800_sys.ram[address]; 
}

uint16_t FMZ800Emu::ReadWord(uint16_t address) const { 
    return (uint16_t)g_mz800_sys.ram[address] | ((uint16_t)g_mz800_sys.ram[(address + 1) & 0xFFFF] << 8); 
}

const uint8_t* FMZ800Emu::GetMemPtr(uint16_t address) const { 
    return &g_mz800_sys.ram[address]; 
}

void FMZ800Emu::WriteByte(uint16_t address, uint8_t value) {
    g_mz800_sys.ram[address] = value;
}

FAddressRef FMZ800Emu::GetPC(void) { 
    return FAddressRef(g_mz800_sys.cpu.pc, 0); 
}

uint16_t FMZ800Emu::GetSP(void) { 
    return g_mz800_sys.cpu.sp; 
}

void* FMZ800Emu::GetCPUEmulator(void) const {
    return (void*)&g_mz800_sys.cpu;
}
