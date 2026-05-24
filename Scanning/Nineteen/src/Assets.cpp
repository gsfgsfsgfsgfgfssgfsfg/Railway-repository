#include "Interface.hpp"
#include "../src/Assets.hpp"
#include "../resource/Assets/ImagesBytes.hpp"
#include "../resource/Assets/logodofonfon.hpp"
#include "../resource/Assets/laxend.hpp"
#include "../resource/Assets/logosmall.hpp"
#include "../resource/Assets/skript.h"
#include "../resource/Assets/gosth.h"
#include "../resource/awesome/awesome_brands.hpp"

namespace FrameWork
{
    void Assets::Initialize(ID3D11Device* Device)
    {
        ImGuiIO& io = ImGui::GetIO();
        ImFontConfig cfg;

        lexendfont = io.Fonts->AddFontFromMemoryTTF(lexendfontona, sizeof(lexendfontona), 18.0f, NULL, io.Fonts->GetGlyphRangesCyrillic());
        InterRegular = io.Fonts->AddFontFromMemoryCompressedTTF(InterRegular_compressed_data, InterRegular_compressed_size, 16);
        Verdana = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Verdana.ttf", 16, &cfg, io.Fonts->GetGlyphRangesCyrillic());
        VerdanaBig = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Verdana.ttf", 20, &cfg, io.Fonts->GetGlyphRangesCyrillic());
        VerdanaSmall = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Verdana.ttf", 14, &cfg, io.Fonts->GetGlyphRangesCyrillic());
        InterBlack = io.Fonts->AddFontFromMemoryCompressedTTF(InterBlack_compressed_data, InterBlack_compressed_size, 14);
        
        InterBoldBig = io.Fonts->AddFontFromMemoryCompressedTTF(InterBold_compressed_data, InterBold_compressed_size, 30);
        InterBold12 = io.Fonts->AddFontFromMemoryCompressedTTF(InterBold_compressed_data, InterBold_compressed_size, 12);
        InterExtraBold = io.Fonts->AddFontFromMemoryCompressedTTF(InterExtraBold_compressed_data, InterExtraBold_compressed_size, 14);
        InterExtraLight = io.Fonts->AddFontFromMemoryCompressedTTF(InterExtraLight_compressed_data, InterExtraLight_compressed_size, 14);
        InterLight = io.Fonts->AddFontFromMemoryCompressedTTF(InterLight_compressed_data, InterLight_compressed_size, 12);
        InterMedium = io.Fonts->AddFontFromMemoryCompressedTTF(InterMedium_compressed_data, InterMedium_compressed_size, 16);
        InterMediumSmall = io.Fonts->AddFontFromMemoryCompressedTTF(InterMedium_compressed_data, InterMedium_compressed_size, 13);
        InterMediumBig = io.Fonts->AddFontFromMemoryCompressedTTF(InterMedium_compressed_data, InterMedium_compressed_size, 18);
        InterRegular14 = io.Fonts->AddFontFromMemoryCompressedTTF(InterRegular_compressed_data, InterRegular_compressed_size, 14);
        InterSemiBold = io.Fonts->AddFontFromMemoryCompressedTTF(InterSemiBold_compressed_data, InterSemiBold_compressed_size, 16);
        InterThin = io.Fonts->AddFontFromMemoryCompressedTTF(InterThin_compressed_data, InterThin_compressed_size, 14);


        static const ImWchar IconRanges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
        ImFontConfig FontAwesomeConfig;
        FontAwesomeConfig.GlyphMinAdvanceX = 25.f * (2.0f / 3.0f);
        FontAwesomeRegular = io.Fonts->AddFontFromMemoryCompressedTTF(FontAwesomeRegular_compressed_data, FontAwesomeRegular_compressed_size, 25.f * (2.0f / 3.0f), &FontAwesomeConfig, IconRanges);
        FontAwesomeSolid = io.Fonts->AddFontFromMemoryCompressedTTF(FontAwesomeSolid_compressed_data, FontAwesomeSolid_compressed_size, 25.f * (2.0f / 3.0f), &FontAwesomeConfig, IconRanges);
        FontAwesomeSolid18 = io.Fonts->AddFontFromMemoryCompressedTTF(FontAwesomeSolid_compressed_data, FontAwesomeSolid_compressed_size, 18.f * (2.0f / 3.0f), &FontAwesomeConfig, IconRanges);
        FontAwesomeSolidBig = io.Fonts->AddFontFromMemoryCompressedTTF(FontAwesomeSolid_compressed_data, FontAwesomeSolid_compressed_size, 30.f * (2.0f / 3.0f), &FontAwesomeConfig, IconRanges);


        D3DX11CreateShaderResourceViewFromMemory(Device, LogoMenuRawBytes, sizeof(LogoMenuRawBytes), NULL, NULL, &LogoMenu, NULL);
        D3DX11CreateShaderResourceViewFromMemory(Device, nicolasnetoo, sizeof(nicolasnetoo), NULL, NULL, &LogoLogin, NULL);
        D3DX11CreateShaderResourceViewFromMemory(Device, logosmall, sizeof(logosmall), NULL, NULL, &logopequena, NULL);
        D3DX11CreateShaderResourceViewFromMemory(Device, skript, sizeof(skript), NULL, NULL, &pic_skript, NULL);
        D3DX11CreateShaderResourceViewFromMemory(Device, gosth, sizeof(gosth), NULL, NULL, &pic_gosth, NULL);

        io.Fonts->Build();
    }
}