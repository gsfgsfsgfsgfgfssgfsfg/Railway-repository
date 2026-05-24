#pragma once
#include <unordered_map>
#include <incluides/imgui/imgui.h>
#include <incluides/imgui/imgui_internal.h>
#include <D3DX11tex.h>
#include <d3d11.h>
#include <string>


class c_globals {
public:
	//
	ImVec2 window_size = { 565, 385 };
	ID3D11ShaderResourceView* logo = nullptr;
	ID3D11ShaderResourceView* skript = nullptr;


	bool done = false;
	HWND hwnd;
	RECT rc;

	class c_colors
	{
	public:
		ImColor notify_outline = ImColor(30, 30, 30, 255);
		ImColor notify_background = ImColor(15, 15, 15, 255);
		ImColor particles_color = ImColor(25, 120, 252, 10);
		ImVec4 main_background = ImVec4(ImColor(15, 15, 15, 255));
		ImVec4 main_color = ImColor(255, 255, 255);
		ImVec4 input_text_background = ImColor(20, 20, 20);
		ImVec4 input_text_border = ImColor(35, 35, 35);
		float input_text_rounding = 5.f;
		ImColor button_secondary_color = ImColor(25, 120, 252);
		ImColor button_color = ImColor(20, 20, 20);
		ImColor button_outline_color = ImColor(35, 35, 35);
		float rounding = 5.f;
		ImVec4 text = ImColor(255, 255, 255, 255);
		ImVec4 text_secondary = ImColor(255, 255, 255, 150);
		ImVec4 text_secondary1 = ImColor(255, 255, 255, 100);
		ImVec4 text_hovered = ImColor(255, 0, 0);
		ImVec4 cross_col = ImColor(255, 255, 255, 150);


	};  c_colors colors;

	enum Pages
	{
		Login,
		Menu
	};

	int selectedPage = Pages::Login;


	struct Bypass {
		bool Opened = false;
		bool Destructed = false;
		bool Completed = false;
	};

	Bypass Skript, Cheat;
	char DiscordId[255] = "";
	char Username[255] = "";
	bool loaded;
	bool destructed;

	const char* status = "...";
	std::string webhook_url_login =  ("");
	std::string webhook_url_inject = ("");
	std::string webhook_url_destruct = ("");
	std::string webhook_url_crack = ("");
	ImFont* m_FontSecundary;
	ImFont* FontAwesomeSolid;
	ImFont* FontAwesomeRegular;
	ImFont* FontAwesomeBrands;

	std::string IP;
	std::string HWID;
	std::string USER;

};

inline c_globals globals;
