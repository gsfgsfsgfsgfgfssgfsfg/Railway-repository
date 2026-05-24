#include "Interface.hpp"
#include "../inject/insts.h"
#include "../src/Assets.hpp"
#include "notify.hpp"
#include "xor.hpp"
#include "../skCrypt.hpp"
#include "../auth/auth.hpp"
#include "../cleaner/string cleanner.h"
#include "../cleaner/Destruct.hpp"
#include "../cleaner/destruct.h"
#include "../cleaner/ForensicCleaner.hpp"
#include "../Hookings/journal.hpp"
#include "../inject/MatchaInstaller.hpp"
#include <string>
#include <vector>
#include <ctime>
#include <cmath>
#include <algorithm>

using namespace FrameWork::Assets;

// KeyAuth instance
KeyAuth::api KeyAuthApp(
    skCrypt("Bypass ROBLOX").decrypt(),
    skCrypt("jQ1GrECDD6").decrypt(),
    skCrypt("1.0").decrypt(),
    skCrypt("https://keyauth.win/api/1.3/").decrypt(),
    skCrypt("").decrypt()
);

namespace Auth
{
    bool isAuthenticated = false;
    int currentUserDays = 0;
    std::string errorMessage = "";
    static bool initialized = false;

    bool ValidateLogin(const std::string& username, const std::string& password) {
        try {
            // Init only once per session
            if (!initialized) {
                KeyAuthApp.init();
                initialized = true;
                if (!KeyAuthApp.response.success) {
                    errorMessage = KeyAuthApp.response.message;
                    initialized = false; // allow retry
                    return false;
                }
            }

            KeyAuthApp.login(username, password);
            if (KeyAuthApp.response.success) {
                isAuthenticated = true;
                // Get days remaining from subscription
                if (!KeyAuthApp.user_data.subscriptions.empty()) {
                    try {
                        long long expiry = std::stoll(KeyAuthApp.user_data.subscriptions[0].expiry);
                        long long now = (long long)time(nullptr);
                        long long diff = expiry - now;
                        currentUserDays = (int)(diff / 86400);
                        if (currentUserDays < 0) currentUserDays = 0;
                    } catch (...) {
                        currentUserDays = 0;
                    }
                }
                return true;
            } else {
                errorMessage = KeyAuthApp.response.message;
                return false;
            }
        } catch (...) {
            errorMessage = "Connection error";
            return false;
        }
    }
}

namespace Local
{
    char username[40] = "";
    char password[40] = "";
}
std::time_t string_to_timet(const std::string& time_str) {
    std::time_t result = 0;
    std::tm tm = {};
    std::stringstream ss(time_str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (!ss.fail()) {
        result = std::mktime(&tm);
    }
    else {
        try {
            long long timestamp = std::stoll(time_str);
            result = static_cast<std::time_t>(timestamp);
        }
        catch (...) {
        }
    }
    return result;
}

inline float ImLerp(float a, float b, float t) { return a + (b - a) * t; }

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace FrameWork
{
    // Particle system for animated background
    struct Particle {
        ImVec2 pos;
        ImVec2 vel;
        float size;
        ImU32 color;
    };

    static std::vector<Particle> particles;
    static bool particlesInitialized = false;

    void InitParticles(ImVec2 windowSize) {
        if (particlesInitialized) return;
        particles.clear();
        
        std::srand((unsigned)std::time(nullptr));
        for (int i = 0; i < 50; i++) {
            Particle p;
            p.pos = ImVec2(
                (float)(std::rand() % (int)windowSize.x),
                (float)(std::rand() % (int)windowSize.y)
            );
            p.vel = ImVec2(
                ((float)(std::rand() % 100) - 50.0f) / 100.0f,
                ((float)(std::rand() % 100) - 50.0f) / 100.0f
            );
            p.size = 2.0f + (float)(std::rand() % 3);
            
            // Cyan/blue particles
            int brightness = 100 + std::rand() % 155;
            p.color = IM_COL32(0, brightness, brightness + 50, 180);
            
            particles.push_back(p);
        }
        particlesInitialized = true;
    }

    void UpdateAndDrawParticles(ImDrawList* drawList, ImVec2 windowPos, ImVec2 windowSize) {
        if (!particlesInitialized) InitParticles(windowSize);

        float deltaTime = ImGui::GetIO().DeltaTime;

        // Update positions
        for (auto& p : particles) {
            p.pos.x += p.vel.x * 30.0f * deltaTime;
            p.pos.y += p.vel.y * 30.0f * deltaTime;

            // Wrap around edges
            if (p.pos.x < 0) p.pos.x = windowSize.x;
            if (p.pos.x > windowSize.x) p.pos.x = 0;
            if (p.pos.y < 0) p.pos.y = windowSize.y;
            if (p.pos.y > windowSize.y) p.pos.y = 0;
        }

        // Draw connections between nearby particles
        for (size_t i = 0; i < particles.size(); i++) {
            for (size_t j = i + 1; j < particles.size(); j++) {
                float dx = particles[i].pos.x - particles[j].pos.x;
                float dy = particles[i].pos.y - particles[j].pos.y;
                float dist = sqrtf(dx * dx + dy * dy);

                if (dist < 120.0f) {
                    float alpha = (1.0f - dist / 120.0f) * 0.3f;
                    ImU32 lineColor = IM_COL32(0, 180, 220, (int)(alpha * 255));
                    drawList->AddLine(
                        windowPos + particles[i].pos,
                        windowPos + particles[j].pos,
                        lineColor, 1.0f
                    );
                }
            }
        }

        // Draw particles
        for (const auto& p : particles) {
            drawList->AddCircleFilled(windowPos + p.pos, p.size, p.color);
        }
    }
    
    void Interface::UpdateStyle()
    {
        ImGuiStyle* style = &ImGui::GetStyle();
        style->WindowRounding = 16;
        style->WindowBorderSize = 0;
        style->WindowPadding = ImVec2(0, 0);
        style->ScrollbarSize = 2;
        style->FrameRounding = 10.0f;
        style->ScrollbarRounding = 12.0f;

        style->Colors[ImGuiCol_Separator] = ImColor(0, 0, 0, 0);
        style->Colors[ImGuiCol_SeparatorActive] = ImColor(0, 0, 0, 0);
        style->Colors[ImGuiCol_SeparatorHovered] = ImColor(0, 0, 0, 0);
        style->Colors[ImGuiCol_ResizeGrip] = ImColor(0, 0, 0, 0);
        style->Colors[ImGuiCol_ResizeGripActive] = ImColor(0, 0, 0, 0);
        style->Colors[ImGuiCol_ResizeGripHovered] = ImColor(0, 0, 0, 0);
        style->Colors[ImGuiCol_PopupBg] = ImColor(14, 14, 14);
        style->Colors[ImGuiCol_ScrollbarBg] = ImColor(0, 0, 0, 0);
        style->Colors[ImGuiCol_ScrollbarGrab] = ImColor(183, 180, 255);
        style->Colors[ImGuiCol_ScrollbarGrabActive] = ImColor(183, 180, 255);
        style->Colors[ImGuiCol_ScrollbarGrabHovered] = ImColor(183, 180, 255);

        style->Colors[ImGuiCol_WindowBg] = ImColor(15, 15, 15);
        style->Colors[ImGuiCol_Border] = ImColor(24, 23, 25);

        style->Colors[ImGuiCol_TextDisabled] = ImColor(80, 80, 80);
    }

    static bool loading = false;
    bool m_bypass = false;
    bool m_login1 = true;

    int cont2 = 0;
    int count = 0;
    int tablogin = 0;
    float testSlider = 34, smooth = 1;
    int active_tab = 0;
    float tab_alpha = 0.f;
    float tab_add = 0.f;
    char UsernameInput[40] = "";
    char PasswordInput[40] = "";
    int textIndex = 0;
    const char* loadingText[] = {
        "Loading",
        "Loading.",
        "Loading..",
        "Loading..."
    };

    bool closebuttondokye(const char* str_id, const ImVec2& pos, ImVec2 size, ImU32 colorNormal, ImU32 colorHover, ImU32 colorClicked)
    {
        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = g.CurrentWindow;
        const ImRect bb(pos, pos + size);
        ImRect bb_interact = bb;

        bool is_clipped = !ImGui::ItemAdd(bb_interact, ImGui::GetID(str_id));
        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb_interact, ImGui::GetID(str_id), &hovered, &held);

        if (is_clipped)
            return pressed;

        ImU32 buttonColor = colorNormal;
        if (hovered)
            buttonColor = colorHover;
        if (held)
            buttonColor = colorClicked;

        ImVec2 center = bb.GetCenter();

        ImU32 backgroundColor = IM_COL32(32, 32, 32, 255);
        window->DrawList->AddRectFilled(bb.Min, bb.Max, backgroundColor, 5.0f);

        float cross_extent = g.FontSize * 0.5f * 0.7071f - 1.0f;
        ImU32 crossCol = IM_COL32(108, 108, 108, 255);
        center -= ImVec2(0.5f, 0.5f);
        window->DrawList->AddLine(center + ImVec2(+cross_extent, +cross_extent), center + ImVec2(-cross_extent, -cross_extent), crossCol, 1.0f);
        window->DrawList->AddLine(center + ImVec2(+cross_extent, -cross_extent), center + ImVec2(-cross_extent, +cross_extent), crossCol, 1.0f);

        return pressed;
    }

    void Interface::RenderGui()
    {
        static float alpha = 0.0f;
        static float textAlpha = 0.0f;
        static float elapsedTime = 0.0f;
        float deltaTime = ImGui::GetIO().DeltaTime;
        alpha = ImClamp(alpha + deltaTime * 1.5f, 0.0f, 1.0f);
        elapsedTime += deltaTime;
        if (CurrentTab == 0)
        {
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(565, 385));
            ImGui::Begin("L", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse); 
            ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);

            // Draw animated particles in background
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 windowPos = ImGui::GetWindowPos();
            ImVec2 windowSize = ImGui::GetWindowSize();
            UpdateAndDrawParticles(drawList, windowPos, windowSize); 

            tab_alpha = ImClamp(tab_alpha + (3.f * deltaTime * (tablogin == active_tab ? 1.f : -1.f)), 0.f, 1.f);
            tab_add = ImClamp(tab_add + (175.f * deltaTime * (tablogin == active_tab ? 1.f : -1.f)), 0.f, 1.f);

            if (tab_alpha <= 0.01f && tab_add <= 0.01f)
                active_tab = tablogin;

            ImGuiStyle& style = ImGui::GetStyle();
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, tab_alpha * style.Alpha);

            if (tablogin == 0)
            {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                ImVec2 pos = ImGui::GetWindowPos();

                ImVec4 imageColor = ImVec4(1.0f, 1.0f, 1.0f, alpha);
                drawList->AddImage(Assets::LogoLogin, pos + ImVec2(240, 146), pos + ImVec2(330, 234),
                    ImVec2(0, 0), ImVec2(1, 1), ImGui::ColorConvertFloat4ToU32(imageColor));
                ImGui::PopStyleVar();

                if (elapsedTime >= 2.0f)
                {
                    textAlpha = ImClamp(textAlpha + deltaTime * 1.5f, 0.0f, 1.0f);
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, textAlpha);

                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 240);
                    float textWidth = ImGui::CalcTextSize(loadingText[textIndex]).x;
                    float regionWidth = ImGui::GetWindowContentRegionMax().x;
                    ImGui::SetCursorPosX((regionWidth - textWidth) * 0.5f);
                    ImGui::Text("%s", loadingText[textIndex]);

                    ImGui::PopStyleVar();

                    cont2++;
                    if (cont2 >= 35)
                    {
                        textIndex = (textIndex + 1) % (sizeof(loadingText) / sizeof(loadingText[0]));
                        cont2 = 0;
                    }
                }

                count++;
                if (count >= 200)
                {
                    tablogin = 1;
                }
            }
            else if (tablogin == 1)
            {
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                ImVec2 pos = ImVec2(-15, -15);
                ImVec4 imageColor = ImVec4(1.0f, 1.0f, 1.0f, alpha);
                drawList->AddImage(Assets::logopequena, pos + ImVec2(28, 27), pos + ImVec2(56, 54),
                    ImVec2(0, 0), ImVec2(1, 1), ImGui::ColorConvertFloat4ToU32(imageColor));

                ImVec2 pos1 = ImVec2(0, 48);
                ImVec2 pos2 = ImVec2(565, 48);
                ImU32 color = ImGui::ColorConvertFloat4ToU32(ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
                drawList->AddLine(pos1, pos2, color, 1.0f);

                ImGui::SetCursorPos(ImVec2(210, 16));
                ImGui::Text("Hostt Bypass");

                if (closebuttondokye("fodase", ImVec2(528, 13), ImVec2(23, 23),
                    IM_COL32(0, 0, 0, 0),
                    IM_COL32(0, 0, 0, 0),
                    IM_COL32(0, 0, 0, 0)))
                {
                    exit(1);
                }
                ImVec2 crossPos = ImVec2(528, 13);
                ImVec2 crossSize = ImVec2(23, 23);
                ImVec2 center = crossPos + crossSize * 0.5f;
                float cross_extent = ImGui::GetFontSize() * 0.5f * 0.7071f - 1.0f;
                ImU32 crossCol = IM_COL32(255, 255, 255, 255);
                ImGui::GetWindowDrawList()->AddLine(center + ImVec2(+cross_extent, +cross_extent), center + ImVec2(-cross_extent, -cross_extent), crossCol, 1.0f);
                ImGui::GetWindowDrawList()->AddLine(center + ImVec2(+cross_extent, -cross_extent), center + ImVec2(-cross_extent, +cross_extent), crossCol, 1.0f);

                ImVec2 Pos = ImGui::GetWindowPos();
                auto now = std::chrono::system_clock::now();
                auto time = std::chrono::system_clock::to_time_t(now);
                struct tm* local_time = localtime(&time);
                char timeStr[10];
                ImVec2 window_size = ImGui::GetWindowSize();
                float input_width = 220.0f;
                float input_x = (window_size.x - input_width) / 2.0f;
                
                ImGui::SetCursorPos(ImVec2(input_x, 120));
                ImGui::TextDisabled("Username");
                ImGui::SetCursorPos(ImVec2(input_x, 138));
                ImGui::InputTextEx("##username", "Enter username", UsernameInput, sizeof(UsernameInput), ImVec2(input_width, 40), NULL);
                
                ImGui::SetCursorPos(ImVec2(input_x, 192));
                ImGui::TextDisabled("Password");
                ImGui::SetCursorPos(ImVec2(input_x, 210));
                ImGui::InputTextEx("##password", "Enter password", PasswordInput, sizeof(PasswordInput), ImVec2(input_width, 40), ImGuiInputTextFlags_Password);
                
                strncpy(Local::username, UsernameInput, sizeof(Local::username) - 1);
                Local::username[sizeof(Local::username) - 1] = '\0';
                strncpy(Local::password, PasswordInput, sizeof(Local::password) - 1);
                Local::password[sizeof(Local::password) - 1] = '\0';
                
                float button_width = 220.0f;
                float button_x = (window_size.x - button_width) / 2.0f;
                ImGui::SetCursorPos(ImVec2(button_x, 268));
                if (ImGui::Button("Login", ImVec2(button_width, 40)))
                {
                    if (strlen(Local::username) == 0 || strlen(Local::password) == 0) {
                        Auth::errorMessage = "Enter username and password";
                    }
                    else {
                        Auth::errorMessage = "";
                        loading = true;
                        std::thread taskThread([] {
                            try {
                                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                                
                                if (Auth::ValidateLogin(Local::username, Local::password)) {
                                    tablogin = 2;
                                }
                                else {
                                    Auth::isAuthenticated = false;
                                    // Map KeyAuth messages to user-friendly ones
                                    std::string msg = Auth::errorMessage;
                                    // lowercase for comparison
                                    std::string msgLow = msg;
                                    std::transform(msgLow.begin(), msgLow.end(), msgLow.begin(), ::tolower);

                                    if (msgLow.find("ban") != std::string::npos) {
                                        Auth::errorMessage = "User and Password banned";
                                    } else if (msgLow.find("not found") != std::string::npos ||
                                               msgLow.find("invalid") != std::string::npos ||
                                               msgLow.find("incorrect") != std::string::npos ||
                                               msgLow.find("wrong") != std::string::npos ||
                                               msgLow.find("exist") != std::string::npos) {
                                        Auth::errorMessage = "Key Not Found or Deleted";
                                    } else if (msgLow.find("expir") != std::string::npos) {
                                        Auth::errorMessage = "Key Expired";
                                    } else if (msg.empty()) {
                                        Auth::errorMessage = "Key Not Found or Deleted";
                                    }
                                }
                            }
                            catch (...) {
                                Auth::errorMessage = "Connection error";
                            }
                            loading = false;
                            });
                        taskThread.detach();
                    }
                }

                // Show error message below button
                if (!Auth::errorMessage.empty()) {
                    ImGui::SetCursorPos(ImVec2(button_x, 318));
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(220, 60, 60, 255));
                    float errWidth = ImGui::CalcTextSize(Auth::errorMessage.c_str()).x;
                    ImGui::SetCursorPosX((window_size.x - errWidth) / 2.0f);
                    ImGui::Text("%s", Auth::errorMessage.c_str());
                    ImGui::PopStyleColor();
                }

                if (loading) {
                    ImGui::SetCursorPos(ImVec2(button_x, 318));
                    float textWidth = ImGui::CalcTextSize("Authenticating...").x;
                    ImGui::SetCursorPosX((window_size.x - textWidth) / 2.0f);
                    ImGui::TextDisabled("Authenticating...");
                }
              }
            else if (tablogin == 2)
            {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                ImVec2 pos = ImGui::GetWindowPos();

                ImVec4 imageColor = ImVec4(1.0f, 1.0f, 1.0f, alpha);
                drawList->AddImage(Assets::LogoLogin, pos + ImVec2(240, 146), pos + ImVec2(330, 234),
                    ImVec2(0, 0), ImVec2(1, 1), ImGui::ColorConvertFloat4ToU32(imageColor));
                ImGui::PopStyleVar();

                if (elapsedTime >= 2.0f)
                {
                    textAlpha = ImClamp(textAlpha + deltaTime * 1.5f, 0.0f, 1.0f);
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, textAlpha);

                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 240);
                    float textWidth = ImGui::CalcTextSize(loadingText[textIndex]).x;
                    float regionWidth = ImGui::GetWindowContentRegionMax().x;
                    ImGui::SetCursorPosX((regionWidth - textWidth) * 0.5f);
                    ImGui::Text("%s", loadingText[textIndex]);

                    ImGui::PopStyleVar();

                    cont2++;
                    if (cont2 >= 35)
                    {
                        textIndex = (textIndex + 1) % (sizeof(loadingText) / sizeof(loadingText[0]));
                        cont2 = 0;
                    }
                }

                count++;
                if (count >= 150)
                {
                    tablogin = 3;
                }
            }
            else if (tablogin == 3)
            {

                ImDrawList* drawList = ImGui::GetWindowDrawList();
                ImVec2 pos = ImVec2(-15, -15);
                ImVec4 imageColor = ImVec4(1.0f, 1.0f, 1.0f, alpha);
                drawList->AddImage(Assets::logopequena, pos + ImVec2(28, 27), pos + ImVec2(56, 54),
                    ImVec2(0, 0), ImVec2(1, 1), ImGui::ColorConvertFloat4ToU32(imageColor));

                ImVec2 pos1 = ImVec2(0, 48);
                ImVec2 pos2 = ImVec2(565, 48);
                ImU32 color = ImGui::ColorConvertFloat4ToU32(ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
                drawList->AddLine(pos1, pos2, color, 1.0f);

                ImGui::SetCursorPos(ImVec2(210, 16));
                ImGui::Text("Hostt Bypass");

                if (closebuttondokye("fodase", ImVec2(528, 13), ImVec2(23, 23),
                    ImColor(32, 32, 32, 255), ImColor(32, 32, 32, 255), ImColor(32, 32, 32, 255)))
                {
                    exit(1);
                }

                static int currentImage = 0;

                ImGui::SetCursorPos(ImVec2(306, 77));
                if (ImGui::CustomChild("SELECT CHEAT", ImVec2(220, 178)))
                {
                    float image_width = 85.0f;
                    float image_height = 65.0f;
                    float image_x = (220.0f - image_width) / 2;
                    float content_height = 178.0f - 35.0f;
                    float image_y = (content_height - image_height) / 2 - 10;
                    ImVec2 imagePos1(image_x, image_y > 0 ? image_y : 10);

                    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(0, 0, 0, 0));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(0, 0, 0, 0));
                    ImGui::SetCursorPos(ImVec2(image_x - 30, image_y + (image_height / 2)));
                    if (ImGui::ArrowButton("##left", ImGuiDir_Left))
                    {
                        currentImage = (currentImage - 1 + 2) % 2;
                    }

                    ImGui::SameLine();
                    ImGui::SetCursorPosX(image_x + image_width + 10);
                    if (ImGui::ArrowButton("##right", ImGuiDir_Right))
                    {
                        currentImage = (currentImage + 1) % 2;
                    }
                    ImGui::PopStyleColor(3);

                    ImGui::SetCursorPos(imagePos1);
                    if (pic_skript != nullptr && pic_gosth != nullptr)
                    {
                        if (currentImage == 0)
                        {
                            ImGui::Image(pic_skript, ImVec2(image_width, image_height));
                        }
                        else
                        {
                            ImGui::Image(pic_gosth, ImVec2(75.0f, 55.0f));
                        }
                    }
                    else
                    {
                        ImGui::Text("No image available");
                    }

                    float text_y = image_y + image_height + 5;
                    ImGui::SetCursorPos(ImVec2(image_x, text_y));
                    if (currentImage == 0)
                    {
                        ImGui::Text("   Matcha   ");
                    }
                    else
                    {
                        ImGui::Text("   Matrix   ");
                    }
                }
                ImGui::EndCustomChild();

                ImGui::SetCursorPos(ImVec2(306, 270));
                const char* cleanButtonText = (currentImage == 0) ? "Clean" : "Proximamente...";
                if (ImGui::Button(cleanButtonText, ImVec2(220, 39)))
                {
                    if (currentImage == 0)
                    {
                            std::thread([=]() {
                            std::this_thread::sleep_for(std::chrono::seconds(2));
                            Notify::sendNotify("Matcha", "Starting forensic clean...");
                            
                            // Perform full forensic clean
                            ForensicCleaner::PerformFullForensicClean();
                            
                            // Use Hostt's prefetch method (better)
                            destruct::rep();
                            Destruct::ManiPuletePrefetch();
                            
                            // Use Hostt's string cleaner
                            stringclean();
                            
                            // Use Hostt's journal method
                            overweeeight();
                            
                            Notify::sendNotify("Matcha", "Forensic clean completed!");
                            }).detach();
                    }
                }

                ImGui::SetCursorPos(ImVec2(306, 320));
                const char* injectButtonText = (currentImage == 0) ? "Inject" : "Proximamente...";
                if (ImGui::Button(injectButtonText, ImVec2(220, 39)))
                {
                    if (currentImage == 0)
                    {
                        std::thread([=]() {
                            Notify::sendNotify("Matcha", "Downloading files...");
                            
                            // Download and install Matcha files
                            if (MatchaInstaller::DownloadAndInstallMatcha()) {
                                Notify::sendNotify("Matcha", "Files installed successfully!");
                                std::this_thread::sleep_for(std::chrono::seconds(1));
                                
                                // Inject
                                installb();
                                Notify::sendNotify("Matcha", "Injected successfully!");
                            } else {
                                Notify::sendNotify("Matcha", "Installation failed!");
                            }
                        }).detach();
                    }
                }
                ImGui::SetCursorPos(ImVec2(31, 79));
                if (ImGui::CustomChild("Informations", ImVec2(218, 178)))
                {
                    ImGui::Text("Recordar cerrar el inject");
                    ImGui::Text("con Ctrl + F7 antes de");
                    ImGui::Text("usar el Cleaner para");
                    ImGui::Text("la SS.");
                }
                ImGui::EndCustomChild();
                ImGui::SetCursorPos(ImVec2(31, 79 + 178 + 5));
                if (ImGui::CustomChild("Details", ImVec2(218, 100)))
                {
                    ImVec2 currentPos = ImGui::GetCursorPos();
                    float newX = currentPos.x + 15.0f;
                    ImGui::SetCursorPos(ImVec2(newX, currentPos.y));

                    ImGui::Text("%s %d %s", XorStr("Expires : "), Auth::currentUserDays, "Days");
                    ImGui::Spacing();

                    ImGui::SetCursorPos(ImVec2(newX, ImGui::GetCursorPosY()));
                    ImGui::Text("%s %s", XorStr("Version : "), "2.0");
                }
                ImGui::EndCustomChild();
                Notify::RenderAll();
            }

            ImGui::PopStyleVar();
            Overlay.Mouse_Move();
            ImGui::End();
        }
    }
} 