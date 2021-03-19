#pragma once
#include <d3d9.h>
#include <d3dx9.h>
#include <cinttypes>

inline HWND g_window = nullptr;
inline IDirect3DDevice9* g_d3d_device = nullptr;
inline bool g_close_gui = false;

namespace gui {
	inline bool mdown = false;
	inline POINTS m { };

	void create_window ( int w, int h, const char* window_name );
	int render ( );
}