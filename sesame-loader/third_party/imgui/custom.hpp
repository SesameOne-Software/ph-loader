#pragma once

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include <d3d11.h>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <tchar.h>

#include <string_view>
#include <vector>
#include <functional>

#include "imgui_internal.h"

namespace ImGui {
    namespace custom {
        bool BeginTabs( int* cur_tab, ImFont* icon_font );
        void AddTab( const char* icon );
        void EndTabs( );

        bool TreeNodeEx( const char* label, const char* desc );

        bool BeginSubtabs( int* cur_subtab );
        void AddSubtab( const char* title, const char* desc, const std::function<void( )>& func );
        void EndSubtabs( );

        void TextOutlined( const ImVec2& pos, ImU32 color, const char* text );
        bool Begin( const char* name, bool* p_open, ImFont* small_font );
        void End( );

        bool Spinner( const char* label , float radius , int thickness , const ImU32& color );
    }
}