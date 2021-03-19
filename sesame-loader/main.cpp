#include "include/gui.hpp"

#include "third_party/xorstr.hpp"

#include <thread>
#include <unordered_map>
#include <iostream>
#include <algorithm>

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "wbemuuid.lib")

using namespace std::literals;

std::string rand_str( size_t length ) {
    auto randchar = [ ] ( ) -> char {
        VM_SHARK_BLACK_START
        const char charset [ ] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
        const size_t max_index = ( sizeof( charset ) - 1 );
        return charset[ rand( ) % max_index ];
        VM_SHARK_BLACK_END
    };

    VM_SHARK_BLACK_START
    std::string str( length , 0 );
    std::generate_n( str.begin( ) , length , randchar );

    return str;
    VM_SHARK_BLACK_END
}

int main( ) {
    VM_SHARK_BLACK_START
    srand( time(nullptr));

    ShowWindow( GetConsoleWindow(), SW_HIDE);

    const auto ph_module = LoadLibraryA( _( "ph.dll" ) );

    if ( !ph_module ) {
        MessageBoxA( nullptr , _( "Dependency ph.dll was not found." ) , _( "Error" ) , MB_ICONERROR | MB_OK );
        return 1;
    }

    gui::create_window( 300 , 216 , rand_str(16).c_str() );

    return gui::render( );
    VM_SHARK_BLACK_END
}