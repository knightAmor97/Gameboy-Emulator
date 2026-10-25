#include <SDL2/SDL.h>
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <cmath>
#include <vector>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include "tinyfiledialogs.h"
#include "library.hpp"
#include "mmu.hpp"
#include "cpu.hpp"
#include "ppu.hpp"
#include "timer.hpp"
#include "apu.hpp"



static std::string savePathFor (const std::string& romPath) {
    size_t dot = romPath.find_last_of('.');
    size_t slash = romPath.find_last_of("/\\");
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return romPath + ".sav";
    return romPath.substr(0, dot) + ".sav";
}
bool runTest(const std::string& path, long maxCycles = 100000000) {
    MMU mmu;
    Timer timer(mmu);
    mmu.connectTimer(&timer);
    APU apu(mmu);
    mmu.connectAPU(&apu);
    CPU cpu(mmu);
    PPU ppu(mmu);

    if (!mmu.loadROM(path)) return false;
    mmu.initPostBoot();
    cpu.reset();

    long total = 0;
    while (total < maxCycles) {
        int c = cpu.step();
        ppu.step(c);
        timer.step(c);
        total += c;

        if (mmu.serialLog.find("Passed") != std::string::npos) return true;
        if (mmu.serialLog.find("Failed") != std::string::npos) return false;
    }
    return false; // timed out
}

static bool mapKey(SDL_Keycode k, Button& out) {
    switch (k) {
        case SDLK_RIGHT: 
            out = Button::Right; return true;
        case SDLK_LEFT: 
            out = Button::Left; return true;
        case SDLK_UP: 
            out = Button::Up; return true;
        case SDLK_DOWN: 
            out = Button::Down; return true;
        case SDLK_z: 
            out = Button::A; return true;
        case SDLK_x: 
            out = Button::B; return true;
        case SDLK_RSHIFT:
            out = Button::Select; return true;
        case SDLK_RETURN: 
            out = Button::Start; return true;
        default: return false;
    }

}

int main(int argc, char* argv[]) {
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
        return 1;
    }
    
    SDL_Window* win = SDL_CreateWindow(
        "GB Emulator",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        480, 432 + 20,
        0
    );

    if (!win) {
        std::cerr << "Window failed: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(win, -1, 0);

    if (!renderer) {
        std::cerr << "Renderer failed: " << SDL_GetError() << '\n';
        return 1;
    }

    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        160, 144
    );

    // ---- ImGui ------
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForSDLRenderer(win, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    // ---- Audio ----
    SDL_AudioSpec want{}, have{};
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = nullptr;

    SDL_AudioDeviceID audioDev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (audioDev == 0) printf("Audio error: %s\n", SDL_GetError());
    else {
        SDL_PauseAudioDevice(audioDev, 0);
    }

    // ---- Emulator ----
    MMU mmu;
    Timer timer(mmu);
    mmu.connectTimer(&timer);
    APU apu(mmu, have.freq);
    mmu.connectAPU(&apu);
    CPU cpu(mmu);
    PPU ppu(mmu);

    Romlibrary library("library.txt");
    enum class State {Menu, Playing};
    State state = State::Menu;
    std::string savPath;

    auto startRom = [&](const std::string& path) -> bool {
        if(!mmu.loadROM(path)) {
            std::cerr << "Failed to load ROM" << path << "\n";
            return false;
        }
        savPath = savePathFor(path);
        mmu.loadRam(savPath);
        mmu.initPostBoot();
        cpu.reset();
        if (audioDev) SDL_ClearQueuedAudio(audioDev);
        state = State::Playing;
        return true;
    };

    auto stopRom = [&]() {
        mmu.saveRam(savPath);
        if (audioDev) SDL_ClearQueuedAudio(audioDev);
        state = State::Menu;
    };

    if (argc > 1) startRom(argv[1]);

    using clock = std::chrono::steady_clock;
    const auto frameTime = std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(1.0 / 59.7275));
    auto nextFrame = clock::now() + frameTime;

    const int CYCLES_PER_FRAME = 70224;
    bool running = true;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)){
            ImGui_ImplSDL2_ProcessEvent(&e);
            if (e.type == SDL_QUIT) running = false;

            if (e.type == SDL_DROPFILE) {
                library.add(e.drop.file);
                SDL_free(e.drop.file);
            }
            if (state == State::Playing) {
                if(e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
                    stopRom();
                else if ((e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) && e.key.repeat == 0) {
                    Button b;
                    if (mapKey(e.key.keysym.sym, b)){
                        mmu.setButton(b, e.type == SDL_KEYDOWN);
                    }
                }   
            }
        }
            

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        std::string ROMToStart;

        if (state == State::Menu) {
            ImGui::SetNextWindowPos({0, 0});
            ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
            ImGui::Begin("Library", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

            if (ImGui::Button("Add ROM...")) {
                const char* filters[] {"*.gb", "*.gbc"};
                const char* p = tinyfd_openFileDialog("Add ROM", "", 2, filters, "Game Boy ROMs", 0);
                if (p) library.add(p);
            }
            ImGui::SameLine();
            ImGui::TextDisabled("or drag your ROMs onto this window");
            ImGui::Separator();

            int removeIdx = -1;
            if (ImGui::BeginTable("roms", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_WidthStretch, 0.6f);
                ImGui::TableSetupColumn("File", ImGuiTableColumnFlags_WidthStretch, 0.4f);
                ImGui::TableHeadersRow();

                for (size_t i = 0; i< library.list().size(); i++) {
                    const auto& r = library.list()[i];
                    ImGui::TableNextRow();
                    ImGui::PushID((int)i);

                    ImGui::TableNextColumn();

                    if(ImGui::Selectable(r.title.c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
                        if(ImGui::IsMouseDoubleClicked(0)) ROMToStart = r.path;
                    }

                    if(ImGui::BeginPopupContextItem()) {
                        if(ImGui::MenuItem("Remove from Library")) removeIdx = (int)i;
                        ImGui::EndPopup();
                    }

                    ImGui::TableNextColumn();
                    size_t slash = r.path.find_last_of("/\\");
                    std::string file = (slash == std::string::npos) ? r.path : r.path.substr(slash + 1);
                    ImGui::TextDisabled("%s", file.c_str());
                    ImGui::PopID();
                }
                ImGui::EndTable();

            }

            if (library.list().empty()) ImGui::Text("No ROMs yet. Click add ROM...");
            ImGui::End();
        }

        ImGui::Render();
        if (state == State::Playing) {
            
            int cycles = 0;
            while (cycles < CYCLES_PER_FRAME) {
                int c = cpu.step();
                ppu.step(c);
                timer.step(c);
                apu.step(c);
                cycles += c;
            }

            SDL_UpdateTexture(texture, nullptr, ppu.getFrameBuffer(), 160 * sizeof(uint32_t));
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, nullptr, nullptr);
            SDL_RenderPresent(renderer);
        
            auto samples = apu.getSamples();
            if(audioDev && !samples.empty() && SDL_GetQueuedAudioSize(audioDev) < 44100 * 4 / 4) {
                SDL_QueueAudio(audioDev, samples.data(), samples.size() * sizeof(int16_t));
            }
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        }
        else {
            SDL_SetRenderDrawColor(renderer, 20, 20, 24, 255);
            SDL_RenderClear(renderer);
        }
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);

        if (!ROMToStart.empty()) startRom(ROMToStart);

        if (state == State::Playing) {
            std::this_thread::sleep_until(nextFrame);
            nextFrame += frameTime;
            auto now = clock::now();
            if (now > nextFrame + frameTime * 5) nextFrame = now + frameTime;
        }
        else {
            SDL_Delay(16);
            nextFrame = clock::now() + frameTime;
        }
    }

    if (state == State::Playing) mmu.saveRam(savPath);

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);
    if (audioDev) SDL_CloseAudioDevice(audioDev);
    SDL_Quit();
    return 0;
}
