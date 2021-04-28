#pragma once
#include "../xorstr.hpp"

#include <Windows.h>
#include <string_view>
#include <vector>

namespace ph {
    /// <summary>
    /// Contains information about a module a user has access to.
    /// </summary>
    struct module_t {
        int id;
        int method;
        const char* name;
        const char* version;
        const char* status;
        const char* target;
    };

    /// <summary>
    /// Contains information regarding the user's authenticated state.
    /// </summary>
    struct login_request_t {
        const char* message;
        module_t* modules;
        int module_count;
        int success;
    };

    /// <summary>
    /// Contains information regarding the status of the attempted injection.
    /// </summary>
    struct inject_request_t {
        const char* message;
        int success;
    };

    /// <summary>
    /// Sends user information to server and authenticates the client.
    /// </summary>
    /// <param name="login_request">Reference to a login request to output to.</param>
    /// <param name="modules">Reference to a module list to output to.</param>
    /// <param name="mta">Use multithreading instead of apartment state; set to true for most c++ use cases.</param>
    /// <returns>Returns true if authentication was successful, or false otherwise. Output message is stored in login_request.message field.</returns>
    __forceinline bool login( login_request_t& login_request, std::vector<module_t>& modules, bool mta = true ) {
        static auto Login = reinterpret_cast< void( __stdcall* )( login_request_t&, bool ) >( GetProcAddress( LoadLibraryA( _( "ph.dll" ) ) , _( "Login" ) ) );

        if ( !modules.empty( ) )
            modules.clear( );

        Login( login_request, mta );

        if ( login_request.modules && login_request.module_count > 0 ) {
            for ( auto i = 0; i < login_request.module_count; i++ )
                modules.push_back( login_request.modules[ i ] );
        }

        return login_request.success == 1;
    }

    /// <summary>
    /// Retrieves username of currently authenticated user.
    /// </summary>
    /// <returns>String containing retrieved username.</returns>
    __forceinline const char* get_username( ) {
        static auto GetUsername = reinterpret_cast< const char*( __stdcall* )( ) >( GetProcAddress( LoadLibraryA( _( "ph.dll" ) ) , _( "GetUsername" ) ) );
        return GetUsername( );
    }

    /// <summary>
    /// Requests loader to inject into the target process with the selected module.
    /// </summary>
    /// <param name="inject_request">Reference to an injection request to output to.</param>
    /// <param name="id">ID of wanted module to inject. This value can be retrieved from the list of subscribed modules; modules[i].id.</param>
    /// <returns>Returns true if injection was successful, or false otherwise. Output message is stored in inject_request.message field.</returns>
    __forceinline bool inject( inject_request_t& inject_request, int id ) {
        static auto Inject = reinterpret_cast< void( __stdcall* )( inject_request_t&, int ) >( GetProcAddress( LoadLibraryA( _( "ph.dll" ) ) , _( "Inject" ) ) );
        Inject( inject_request, id );
        return inject_request.success == 1;
    }
}