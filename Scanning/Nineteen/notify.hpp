#include "globals.hh"
#include <string>
#include <chrono>
#include <vector>
#include "../resource/includes.h"

namespace Notify {

    enum State {
        In, Progress, Out
    };


    struct NotifyStruct
    {
        std::string Title;
        std::string Description;
        std::chrono::time_point<std::chrono::steady_clock> timestamp;
        State notifyState;
        float AdjustX = 250.f;
        float AdjustY = 0.f;
    };

    inline std::vector<NotifyStruct> NotifyVector = {};


    inline void sendNotify(std::string title, std::string description) {
        NotifyStruct notify;
        notify.Title = title;
        notify.Description = description;
        notify.timestamp = std::chrono::steady_clock::now();
        notify.notifyState = State::In;
        NotifyVector.push_back(notify);
    }

    inline void RenderAll() {

        auto window = ImGui::GetWindowDrawList();
        auto now = std::chrono::steady_clock::now();

        for (auto& notify : NotifyVector) {
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - notify.timestamp).count();

            if (duration >= 3) {
                notify.notifyState = State::Out;
            }
            else if (duration > 2) {
                notify.notifyState = State::Progress;
            }



            notify.AdjustX = ImLerp(notify.AdjustX, (notify.notifyState == State::In) ? 0.f : (notify.notifyState == State::Out) ? 250.f : 0.f, ImGui::GetIO().DeltaTime * 5);

            NotifyVector.erase(
                std::remove_if(NotifyVector.begin(), NotifyVector.end(), [&](const NotifyStruct& notify) {
                    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - notify.timestamp).count();
                    return duration >= 4;
                    }),
                NotifyVector.end()
            );

        }

        float notification_width = 150.0f;
        float notification_height = 45.0f;
        float notification_spacing = 10.0f;
        float start_x = globals.window_size.x - (notification_width + 10.0f);
        float start_y = globals.window_size.y - (notification_height + 10.0f);

        int index = 0;

        for (auto& notify : NotifyVector) {

            notify.AdjustY = ImLerp(notify.AdjustY, index * (notification_height + notification_spacing), ImGui::GetIO().DeltaTime * 3);
            ImVec2 pos_min = { start_x - ImGui::CalcTextSize(notify.Description.c_str()).x / 3 + notify.AdjustX, start_y - notify.AdjustY };
            ImVec2 pos_max = { start_x + notification_width + notify.AdjustX, start_y - notify.AdjustY + notification_height };

            ImVec2 TitleSize = ImGui::CalcTextSize(notify.Title.c_str());
            window->AddRectFilled(pos_min, pos_max, ImGui::GetColorU32(globals.colors.input_text_background), 5);
            window->AddRect(pos_min, pos_max, ImGui::GetColorU32(globals.colors.input_text_border), 5);
            window->AddText({ pos_min.x + 10, pos_min.y + 5 }, ImGui::GetColorU32(globals.colors.text), notify.Title.c_str());

            ImGui::PushFont(globals.FontAwesomeRegular);
            window->AddText({ pos_min.x + 13 + TitleSize.x, pos_min.y + 7 }, ImGui::GetColorU32(globals.colors.main_color), ICON_FA_EXCLAMATION);
            ImGui::PopFont();

            ImGui::PushFont(globals.m_FontSecundary);
            window->AddText({ pos_min.x + 10, pos_min.y + 25 }, ImGui::GetColorU32(globals.colors.text_secondary), notify.Description.c_str());
            ImGui::PopFont();

            index++;
        }
    }
}