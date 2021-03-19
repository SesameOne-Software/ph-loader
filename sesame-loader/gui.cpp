#include "gui.hpp"

#include "third_party/xorstr.hpp"
#include "third_party/imgui/imgui.h"
#include "third_party/imgui/imgui_impl_dx9.h"
#include "third_party/imgui/imgui_impl_win32.h"
#include "third_party/imgui/custom.hpp"
#include "third_party/ph/ph.hpp"

#include "resources/fonts/sesame_ui.hpp"
#include "resources/fonts/icons.hpp"

#include <chrono>
#include <shlwapi.h>
#include <shlobj_core.h>
#include <fstream>
#include <dwmapi.h>
#include <filesystem>
#include <future>

namespace fs = std::filesystem;

/* globals */
WNDCLASSEX g_wc{ };
IDirect3D9* g_d3d = nullptr;
D3DPRESENT_PARAMETERS g_d3d_pparams = {};

__forceinline bool create_device(HWND hWnd) {
	if (!(g_d3d = Direct3DCreate9(D3D_SDK_VERSION)))
		return false;

	memset(&g_d3d_pparams, 0, sizeof(g_d3d_pparams));
	g_d3d_pparams.Windowed = TRUE;
	g_d3d_pparams.SwapEffect = D3DSWAPEFFECT_DISCARD;
	g_d3d_pparams.hDeviceWindow = hWnd;
	g_d3d_pparams.MultiSampleQuality = D3DMULTISAMPLE_NONE;
	g_d3d_pparams.BackBufferFormat = D3DFMT_A8R8G8B8;
	g_d3d_pparams.EnableAutoDepthStencil = TRUE;
	g_d3d_pparams.AutoDepthStencilFormat = D3DFMT_D16;

	if (g_d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3d_pparams, &g_d3d_device) < 0)
		return false;

	return true;
}

void cleanup_device() {
	if (g_d3d_device) {
		g_d3d_device->Release();
		g_d3d_device = nullptr;
	}

	if (g_d3d) {
		g_d3d->Release();
		g_d3d = nullptr;
	}
}

void reset_device() {
	ImGui_ImplDX9_InvalidateDeviceObjects( );

	if ( g_d3d_device->Reset ( &g_d3d_pparams ) == D3DERR_INVALIDCALL ) {
		MessageBoxA( nullptr, _ ( "Fatal error while attempting to reset device!" ), _ ( "Error" ), MB_OK | MB_ICONEXCLAMATION );
		exit( 1 );
	}

	ImGui_ImplDX9_CreateDeviceObjects( );
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND hWnd , UINT msg , WPARAM wParam , LPARAM lParam );

long __stdcall wnd_proc(HWND window, uint32_t msg, uintptr_t wparam, uintptr_t lparam) {
	static POINTS m {};
	static bool mdown = false;

	if ( ImGui_ImplWin32_WndProcHandler( window , msg , wparam , lparam ) )
		return true;

	switch (msg) {
	case WM_LBUTTONDOWN:
		SetCapture(g_window);
		m = MAKEPOINTS(lparam);
		mdown = true;
		return true;
	case WM_LBUTTONUP:
		ReleaseCapture ();
		mdown = false;
		return true;
	case WM_SIZE:
		if (g_d3d_device && wparam != SIZE_MINIMIZED) {
			g_d3d_pparams.BackBufferWidth = LOWORD(lparam);
			g_d3d_pparams.BackBufferHeight = HIWORD(lparam);
			reset_device();
		}
		return 0;
	case WM_SYSCOMMAND:
		if ((wparam & 0xfff0) == SC_KEYMENU)
			return 0;
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_MOUSEMOVE:
		const auto mouse_x = short(lparam);
		const auto mouse_y = short(lparam >> 16);

		if (wparam == MK_LBUTTON) {
			POINTS p = MAKEPOINTS(lparam);
			RECT rect;
			GetWindowRect (g_window, &rect);

			rect.left += p.x - m.x;
			rect.top += p.y - m.y;

			SetWindowPos(g_window, HWND_TOPMOST, rect.left, rect.top, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOZORDER);
		}

		return true;
	}

	return DefWindowProcA(window, msg, wparam, lparam);
}

void gui::create_window(int w, int h, const char* window_name) {
	VM_SHARK_WHITE_START
	g_wc = { sizeof(WNDCLASSEX), 0, WNDPROC(wnd_proc), 0L, 0L, GetModuleHandleA(nullptr), LoadIconA( nullptr, MAKEINTRESOURCEA ( 32512 ) ), nullptr, ( HBRUSH ) CreateSolidBrush( RGB ( 0, 0, 0 ) ), window_name, window_name, LoadIconA( nullptr, MAKEINTRESOURCEA ( 32512 ) ) };
	RegisterClassExA(&g_wc);
	g_window = CreateWindowExA( 0 , g_wc.lpszClassName, window_name, WS_POPUP, 100, 100, w, h, nullptr, nullptr, g_wc.hInstance, nullptr);

	if (!create_device(g_window)) {
		cleanup_device();
		UnregisterClassA(g_wc.lpszClassName, g_wc.hInstance);
		return;
	}

	//LI_FN(ShowWindow) ( LI_FN(GetConsoleWindow)(), SW_HIDE );
	ShowWindow(g_window, SW_SHOWDEFAULT);
	UpdateWindow(g_window);
	VM_SHARK_WHITE_END
}

std::wstring str_towstr(const std::string& str) {
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), nullptr, 0);
	std::wstring wstrTo(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
	return wstrTo;
}

namespace stb {
	static unsigned int stb_decompress_length( const unsigned char* input ) {
		return ( input[ 8 ] << 24 ) + ( input[ 9 ] << 16 ) + ( input[ 10 ] << 8 ) + input[ 11 ];
	}

	static unsigned char* stb__barrier_out_e , * stb__barrier_out_b;
	static const unsigned char* stb__barrier_in_b;
	static unsigned char* stb__dout;
	static void stb__match( const unsigned char* data , unsigned int length ) {
		// INVERSE of memmove... write each byte before copying the next...
		IM_ASSERT( stb__dout + length <= stb__barrier_out_e );
		if ( stb__dout + length > stb__barrier_out_e ) { stb__dout += length; return; }
		if ( data < stb__barrier_out_b ) { stb__dout = stb__barrier_out_e + 1; return; }
		while ( length-- ) *stb__dout++ = *data++;
	}

	static void stb__lit( const unsigned char* data , unsigned int length ) {
		IM_ASSERT( stb__dout + length <= stb__barrier_out_e );
		if ( stb__dout + length > stb__barrier_out_e ) { stb__dout += length; return; }
		if ( data < stb__barrier_in_b ) { stb__dout = stb__barrier_out_e + 1; return; }
		memcpy( stb__dout , data , length );
		stb__dout += length;
	}

#define stb__in2(x)   ((i[x] << 8) + i[(x)+1])
#define stb__in3(x)   ((i[x] << 16) + stb__in2((x)+1))
#define stb__in4(x)   ((i[x] << 24) + stb__in3((x)+1))

	static const unsigned char* stb_decompress_token( const unsigned char* i ) {
		if ( *i >= 0x20 ) { // use fewer if's for cases that expand small
			if ( *i >= 0x80 )       stb__match( stb__dout - i[ 1 ] - 1 , i[ 0 ] - 0x80 + 1 ) , i += 2;
			else if ( *i >= 0x40 )  stb__match( stb__dout - ( stb__in2( 0 ) - 0x4000 + 1 ) , i[ 2 ] + 1 ) , i += 3;
			else /* *i >= 0x20 */ stb__lit( i + 1 , i[ 0 ] - 0x20 + 1 ) , i += 1 + ( i[ 0 ] - 0x20 + 1 );
		}
		else { // more ifs for cases that expand large, since overhead is amortized
			if ( *i >= 0x18 )       stb__match( stb__dout - ( stb__in3( 0 ) - 0x180000 + 1 ) , i[ 3 ] + 1 ) , i += 4;
			else if ( *i >= 0x10 )  stb__match( stb__dout - ( stb__in3( 0 ) - 0x100000 + 1 ) , stb__in2( 3 ) + 1 ) , i += 5;
			else if ( *i >= 0x08 )  stb__lit( i + 2 , stb__in2( 0 ) - 0x0800 + 1 ) , i += 2 + ( stb__in2( 0 ) - 0x0800 + 1 );
			else if ( *i == 0x07 )  stb__lit( i + 3 , stb__in2( 1 ) + 1 ) , i += 3 + ( stb__in2( 1 ) + 1 );
			else if ( *i == 0x06 )  stb__match( stb__dout - ( stb__in3( 1 ) + 1 ) , i[ 4 ] + 1 ) , i += 5;
			else if ( *i == 0x04 )  stb__match( stb__dout - ( stb__in3( 1 ) + 1 ) , stb__in2( 4 ) + 1 ) , i += 6;
		}
		return i;
	}

	static unsigned int stb_adler32( unsigned int adler32 , unsigned char* buffer , unsigned int buflen ) {
		const unsigned long ADLER_MOD = 65521;
		unsigned long s1 = adler32 & 0xffff , s2 = adler32 >> 16;
		unsigned long blocklen = buflen % 5552;

		unsigned long i;
		while ( buflen ) {
			for ( i = 0; i + 7 < blocklen; i += 8 ) {
				s1 += buffer[ 0 ] , s2 += s1;
				s1 += buffer[ 1 ] , s2 += s1;
				s1 += buffer[ 2 ] , s2 += s1;
				s1 += buffer[ 3 ] , s2 += s1;
				s1 += buffer[ 4 ] , s2 += s1;
				s1 += buffer[ 5 ] , s2 += s1;
				s1 += buffer[ 6 ] , s2 += s1;
				s1 += buffer[ 7 ] , s2 += s1;

				buffer += 8;
			}

			for ( ; i < blocklen; ++i )
				s1 += *buffer++ , s2 += s1;

			s1 %= ADLER_MOD , s2 %= ADLER_MOD;
			buflen -= blocklen;
			blocklen = 5552;
		}
		return ( unsigned int ) ( s2 << 16 ) + ( unsigned int ) s1;
	}

	static unsigned int stb_decompress( unsigned char* output , const unsigned char* i , unsigned int /*length*/ ) {
		if ( stb__in4( 0 ) != 0x57bC0000 ) return 0;
		if ( stb__in4( 4 ) != 0 )          return 0; // error! stream is > 4GB
		const unsigned int olen = stb_decompress_length( i );
		stb__barrier_in_b = i;
		stb__barrier_out_e = output + olen;
		stb__barrier_out_b = output;
		i += 16;

		stb__dout = output;
		for ( ;;) {
			const unsigned char* old_i = i;
			i = stb_decompress_token( i );
			if ( i == old_i ) {
				if ( *i == 0x05 && i[ 1 ] == 0xfa ) {
					IM_ASSERT( stb__dout == output + olen );
					if ( stb__dout != output + olen ) return 0;
					if ( stb_adler32( 1 , output , olen ) != ( unsigned int ) stb__in4( 2 ) )
						return 0;
					return olen;
				}
				else {
					IM_ASSERT( 0 ); /* NOTREACHED */
					return 0;
				}
			}
			IM_ASSERT( stb__dout <= output + olen );
			if ( stb__dout > output + olen )
				return 0;
		}
	}
}

int cur_tab_idx = 0;
int selected_module = 0;
bool signed_in = false;
bool injected = false;
bool injected_success = false;
bool running_auth = false;
bool running_injector = false;
bool open_popup = false;
bool open_popup_next_frame = false;
bool exit_after_popup = false;

ph::login_request_t login_request {};
ph::inject_request_t inject_request {};
std::vector<ph::module_t> modules {};

std::string popup_text = "";
std::string popup_title = "";

int gui::render( ) {
	VM_SHARK_WHITE_START
	MSG msg {};
	LARGE_INTEGER li { 0 };

	ImGui::CreateContext( );
	ImGuiIO& io = ImGui::GetIO( ); ( void ) io;
	ImGui::StyleColorsSesame( );

	ImGui_ImplWin32_Init( g_window );
	ImGui_ImplDX9_Init( g_d3d_device );

	io.MouseDrawCursor = false;

	static const ImWchar custom_font_ranges_all [ ] = { 0x20, 0xFFFF, 0 };

	static unsigned int buf_decompressed_size = 0;
	static unsigned char* buf_decompressed_data = nullptr;

	if ( !buf_decompressed_size ) {
		buf_decompressed_size = stb::stb_decompress_length( ( const unsigned char* ) sesame_ui_compressed_data );
		buf_decompressed_data = ( unsigned char* ) IM_ALLOC( buf_decompressed_size );
		stb::stb_decompress( buf_decompressed_data , ( const unsigned char* ) sesame_ui_compressed_data , ( unsigned int ) sesame_ui_compressed_size );
	}

	auto font_cfg = ImFontConfig( );

	font_cfg.FontDataOwnedByAtlas = false;
	font_cfg.OversampleH = 2;
	font_cfg.PixelSnapH = false;
	//io.Fonts->Build ( );

	//_("C:\\Windows\\Fonts\\segoeui.ttf")
	font_cfg.RasterizerMultiply = 1.1f;
	const auto gui_ui_font = io.Fonts->AddFontFromMemoryTTF( buf_decompressed_data , buf_decompressed_size , 13.5f , &font_cfg , custom_font_ranges_all );
	gui_ui_font->SetFallbackChar( '?' );

	font_cfg.RasterizerMultiply = 1.2f;
	const auto gui_small_font = io.Fonts->AddFontFromMemoryTTF( buf_decompressed_data , buf_decompressed_size , 12.0f , &font_cfg , io.Fonts->GetGlyphRangesCyrillic( ) );
	gui_small_font->SetFallbackChar( '?' );

	font_cfg.RasterizerMultiply = 1.0f;
	const auto gui_icons_font = io.Fonts->AddFontFromMemoryTTF( sesame_icons_data , sesame_icons_size , 28.0f , nullptr , io.Fonts->GetGlyphRangesDefault( ) );
	gui_icons_font->SetFallbackChar( '?' );

	ImGui::GetStyle( ).AntiAliasedFill = ImGui::GetStyle( ).AntiAliasedLines = true;

	VM_SHARK_WHITE_END

	while ( msg.message != WM_QUIT && !g_close_gui ) {
		if ( PeekMessageA( &msg , nullptr , 0 , 0 , PM_REMOVE ) ) {
			VM_TIGER_RED_START
			TranslateMessage( &msg );
			DispatchMessageA( &msg );
			VM_TIGER_RED_END
			continue;
		}
		VM_TIGER_RED_START
		ImGui_ImplDX9_NewFrame( );
		ImGui_ImplWin32_NewFrame( );
		ImGui::NewFrame( );

		ImGui::PushFont( gui_ui_font );

		bool open = true;
		VM_TIGER_RED_END
		if ( ImGui::custom::Begin( _( "© 2019-2021 Sesame Software" ) , &open , gui_small_font ) ) {
			VM_TIGER_RED_START
			if ( ImGui::custom::BeginTabs( &cur_tab_idx , gui_icons_font ) ) {
				if ( !injected_success ) {
					ImGui::custom::AddTab( _( "C" ) );

					if ( signed_in )
						ImGui::custom::AddTab( _( "F" ) );
				}

				ImGui::custom::EndTabs( );
			}

			ImGui::SetNextWindowPos( ImVec2( ImGui::GetWindowPos( ).x + ImGui::GetWindowSize( ).x * 0.5f , ImGui::GetWindowPos( ).y + ImGui::GetWindowSize( ).y * 0.5f ) , ImGuiCond_Always , ImVec2( 0.5f , 0.5f ) );

			if ( ImGui::BeginPopupModal( ( popup_title + _( "##popup" ) ).c_str() , nullptr , ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings ) ) {
				ImGui::Text( popup_text.c_str() );

				if ( ImGui::Button( "Ok" , ImVec2( -1.0f , 0.0f ) ) ) {
					ImGui::CloseCurrentPopup( );

					if ( exit_after_popup )
						exit( 0 );
				}

				ImGui::EndPopup( );
			}
			VM_TIGER_RED_END
			switch ( cur_tab_idx ) {
			case 0: {
				VM_TIGER_RED_START
				if ( injected_success )
					break;

				if ( running_auth ) {
					const auto r = 8.0f;
					ImGui::SetCursorPos( ImVec2( ImGui::GetWindowContentRegionMax( ).x * 0.5f - r , ImGui::GetWindowContentRegionMax().y * 0.5f - r ) );
					ImGui::custom::Spinner(_("##Update Spinner"), r , 3.0f, ImGui::ColorConvertFloat4ToU32( ImGui::GetStyleColorVec4( ImGuiCol_Button ) ) );				
					
					ImGui::PushStyleColor( ImGuiCol_Text, ImVec4(1.0, 1.0f, 1.0f, (sin(ImGui::GetTime() * IM_PI * 2.0f) * 0.5f + 0.5f) * 0.5f + 0.5f) );
					const auto text_size = ImGui::CalcTextSize( _( "Fetching Updates" ) );
					ImGui::SetCursorPos( ImVec2( ImGui::GetWindowContentRegionMax( ).x * 0.5f - text_size.x * 0.5f, ImGui::GetWindowContentRegionMax( ).y * 0.5f + r + 8.0f ) );
					ImGui::Text( _( "Fetching Updates" ) );
					ImGui::PopStyleColor( );
				}
				else {
					bool backup_signed_in = signed_in;

					if ( backup_signed_in ) {
						ImGui::PushItemFlag( ImGuiItemFlags_Disabled , true );
						ImGui::PushStyleVar( ImGuiStyleVar_Alpha , ImGui::GetStyle( ).Alpha * 0.5f );
						ImGui::PushStyleColor( ImGuiCol_Button , ImGui::ColorConvertFloat4ToU32( ImVec4( 0.5f , 0.5f , 0.5f , 1.0f ) ) );
						ImGui::PushStyleColor( ImGuiCol_ButtonHovered , ImGui::ColorConvertFloat4ToU32( ImVec4( 0.5f , 0.5f , 0.5f , 1.0f ) ) );
						ImGui::PushStyleColor( ImGuiCol_ButtonActive , ImGui::ColorConvertFloat4ToU32( ImVec4( 0.5f , 0.5f , 0.5f , 1.0f ) ) );
					}

					if ( ImGui::Button( _( "Authenticate" ) , ImVec2( -1.0f , 0.0f ) ) && !running_auth ) {
						// run sign-in on separate thread, use loading spinner if thread not finished (and put text that says loader is updating)

						std::thread( [ ] ( ) {
							VM_SHARK_WHITE_START
							running_auth = true;
							if ( signed_in = ph::login( login_request , modules , true ) ) {
								cur_tab_idx = 1;

								std::thread( [ ] ( ) {
									VM_SHARK_WHITE_START
									popup_text = login_request.message;
									popup_title = _( "Success" );
									open_popup = true;
									VM_SHARK_WHITE_END
									} ).detach( );
							}
							else {
								std::thread( [ ] ( ) {
									VM_SHARK_WHITE_START
									popup_text = login_request.message;
									popup_title = _( "Error" );
									open_popup = true;
									VM_SHARK_WHITE_END
									} ).detach( );
							}
							running_auth = false;
							VM_SHARK_WHITE_END
							} ).detach();
					}

					if ( backup_signed_in ) {
						ImGui::PopItemFlag( );
						ImGui::PopStyleVar( );
						ImGui::PopStyleColor( );
						ImGui::PopStyleColor( );
						ImGui::PopStyleColor( );
					}

					if ( ImGui::Button( _( "Register" ) , ImVec2( -1.0f , 0.0f ) ) )
						ShellExecuteA( nullptr , nullptr , _( "http://sesame.one/auth.php" ) , nullptr , nullptr , SW_SHOW );

					if ( ImGui::Button( _( "Close" ) , ImVec2( -1.0f , 0.0f ) ) )
						exit( 0 );
				}
				VM_TIGER_RED_END
			} break;
			case 1: {
				VM_TIGER_RED_START
				if ( !signed_in || injected_success )
					break;

				if ( running_injector ) {
					const auto r = 8.0f;
					ImGui::SetCursorPos( ImVec2( ImGui::GetWindowContentRegionMax( ).x * 0.5f - r , ImGui::GetWindowContentRegionMax( ).y * 0.5f - r ) );
					ImGui::custom::Spinner( _( "##Update Spinner" ) , r , 3.0f , ImGui::ColorConvertFloat4ToU32( ImGui::GetStyleColorVec4( ImGuiCol_Button ) ) );

					ImGui::PushStyleColor( ImGuiCol_Text , ImVec4( 1.0 , 1.0f , 1.0f , ( sin( ImGui::GetTime( )* IM_PI * 2.0f ) * 0.5f + 0.5f ) * 0.5f + 0.5f ) );
					const auto text_size = ImGui::CalcTextSize( _( "Loading Module" ) );
					ImGui::SetCursorPos( ImVec2( ImGui::GetWindowContentRegionMax( ).x * 0.5f - text_size.x * 0.5f , ImGui::GetWindowContentRegionMax( ).y * 0.5f + r + 8.0f ) );
					ImGui::Text( _( "Loading Module" ) );
					ImGui::PopStyleColor( );
				}
				else {
					std::vector<const char*> names {};
					names.push_back( _( "None" ) );
					for ( auto& iter : modules )
						names.push_back( iter.name );

					if ( !selected_module ) {
						ImGui::NewLine( );
						ImGui::NewLine( );
						ImGui::NewLine( );
					}
					else {
						ImGui::Text( _( "Status:\t%s" ) , modules[ selected_module - 1 ].status );
						ImGui::Text( _( "Target:\t%s" ) , modules[ selected_module - 1 ].target );
						ImGui::Text( _( "Version:\t%s" ) , modules[ selected_module - 1 ].version );
					}
					ImGui::NewLine( );

					ImGui::PushItemWidth( -1.0f );
					ImGui::Combo( _( "Module" ) , &selected_module , names.data( ) , names.size( ) );
					ImGui::PopItemWidth( );

					bool backup_running_injector = running_injector;

					if ( !selected_module || backup_running_injector ) {
						ImGui::PushItemFlag( ImGuiItemFlags_Disabled , true );
						ImGui::PushStyleVar( ImGuiStyleVar_Alpha , ImGui::GetStyle( ).Alpha * 0.5f );
						ImGui::PushStyleColor( ImGuiCol_Button , ImGui::ColorConvertFloat4ToU32( ImVec4( 0.5f , 0.5f , 0.5f , 1.0f ) ) );
						ImGui::PushStyleColor( ImGuiCol_ButtonHovered , ImGui::ColorConvertFloat4ToU32( ImVec4( 0.5f , 0.5f , 0.5f , 1.0f ) ) );
						ImGui::PushStyleColor( ImGuiCol_ButtonActive , ImGui::ColorConvertFloat4ToU32( ImVec4( 0.5f , 0.5f , 0.5f , 1.0f ) ) );
					}

					if ( ImGui::Button( _( "Load" ) , ImVec2( -1.0f , 0.0f ) ) && !running_injector ) {
						// run sign-in on separate thread, use loading spinner if thread not finished (and put text that says loader is updating)

						std::thread( [ ] ( ) {
							VM_SHARK_WHITE_START
							running_injector = true;
							if ( injected = ph::inject( inject_request , modules[ selected_module - 1 ].id ) ) {
								injected_success = true;
								std::thread( [ ] ( ) {
									VM_SHARK_WHITE_START
									popup_text = inject_request.message;
									popup_title = _( "Success" );
									open_popup = true;
									exit_after_popup = true;
									VM_SHARK_WHITE_END
									} ).detach( );
							}
							else {
								std::thread( [ ] ( ) {
									VM_SHARK_WHITE_START
									popup_text = inject_request.message;
									popup_title = _( "Error" );
									open_popup = true;
									VM_SHARK_WHITE_END
									} ).detach( );
							}
							running_injector = false;
							VM_SHARK_WHITE_END
							} ).detach( );
					}

					if ( !selected_module || backup_running_injector ) {
						ImGui::PopItemFlag( );
						ImGui::PopStyleVar( );
						ImGui::PopStyleColor( );
						ImGui::PopStyleColor( );
						ImGui::PopStyleColor( );
					}
				}
				VM_TIGER_RED_END
			} break;
			}

			VM_TIGER_RED_START

			if ( open_popup_next_frame ) {
				ImGui::OpenPopup( ( popup_title + _( "##popup" ) ).c_str( ) );
				open_popup_next_frame = false;
			}

			if ( open_popup ) {
				open_popup_next_frame = true;
				open_popup = false;
			}

			ImGui::custom::End( );
			VM_TIGER_RED_END
		}

		VM_TIGER_RED_START
		ImGui::PopFont( );
		ImGui::EndFrame( );

		g_d3d_device->Clear( 0 , nullptr , D3DCLEAR_TARGET , D3DCOLOR_RGBA( 55 , 55 , 55 , 255 ) , 1.0f , 0 );

		if ( g_d3d_device->BeginScene( ) >= 0 ) {
			ImGui::Render( );
			ImGui_ImplDX9_RenderDrawData( ImGui::GetDrawData( ) );

			g_d3d_device->EndScene( );
		}

		const auto result = g_d3d_device->Present( nullptr , nullptr , nullptr , nullptr );

		if ( result == D3DERR_DEVICELOST && g_d3d_device->TestCooperativeLevel( ) == D3DERR_DEVICENOTRESET )
			reset_device( );

		VM_TIGER_RED_END
	}

	VM_SHARK_WHITE_START
	ImGui_ImplDX9_Shutdown( );
	ImGui_ImplWin32_Shutdown( );
	ImGui::DestroyContext( );

	cleanup_device( );

	DestroyWindow( g_window );
	UnregisterClassA( g_wc.lpszClassName , g_wc.hInstance );

	return 0;
	VM_SHARK_WHITE_END
}