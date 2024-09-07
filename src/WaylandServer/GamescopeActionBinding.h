#pragma once

#include "WaylandProtocol.h"

#include "gamescope-action-binding-protocol.h"

#include <string>
#include <span>
#include <unordered_set>

#include "convar.h"
#include "Utils/Algorithm.h"

#include "wlr_begin.hpp"
#include <xkbcommon/xkbcommon.h>
#include <wlr/types/wlr_keyboard.h>
#include "wlr_end.hpp"

using namespace std::literals;

uint64_t get_time_in_nanos();

namespace gamescope::WaylandServer
{
    struct Keybind_t
    {
        std::unordered_set<xkb_keysym_t> setKeySyms;
    };

    ///////////////////////////
    // CGamescopeActionBinding
    ///////////////////////////
    class CGamescopeActionBinding : public CWaylandResource
    {
    public:
		WL_PROTO_DEFINE( gamescope_action_binding, 1 );

		CGamescopeActionBinding( WaylandResourceDesc_t desc )
            : CWaylandResource( desc )
        {
            s_Bindings.push_back( this );
        }

        ~CGamescopeActionBinding()
        {
            std::erase_if( s_Bindings, [this]( CGamescopeActionBinding *pBinding ){ return pBinding == this; } );
        }

        // gamescope_action_binding

        void SetDescription( const char *pszDescription )
        {
            m_sDescription = pszDescription;
        }

        void AddKeyboardTrigger( wl_array *pKeysymsArray )
        {
            size_t zKeysymCount = pKeysymsArray->size / sizeof( xkb_keysym_t );

            std::span<const xkb_keysym_t> pKeysyms = std::span<const xkb_keysym_t> {
                reinterpret_cast<const xkb_keysym_t *>( pKeysymsArray->data ),
                zKeysymCount };

            std::unordered_set<xkb_keysym_t> setKeySyms;
            for ( xkb_keysym_t uKeySym : pKeysyms )
            {
                setKeySyms.emplace( uKeySym );   
            }

            m_KeyboardTriggers.emplace_back( std::move( setKeySyms ) );
        }

        void ClearTriggers()
        {
            m_KeyboardTriggers.clear();
        }

        void Arm( uint32_t uArmFlags )
        {
            m_ouArmFlags = uArmFlags;
        }

        void Disarm()
        {
            m_ouArmFlags = std::nullopt;
        }

        //

        bool IsArmed() { return m_ouArmFlags != std::nullopt; }
        std::span<Keybind_t> GetKeyboardTriggers() { return m_KeyboardTriggers; }

        bool Execute()
        {
            if ( !IsArmed() )
                return false;

            uint32_t uArmFlags = *m_ouArmFlags;
            bool bBlockInput = !!( uArmFlags & GAMESCOPE_ACTION_BINDING_ARM_FLAG_NO_BLOCK );

            uint32_t uTriggerFlags = GAMESCOPE_ACTION_BINDING_TRIGGER_FLAG_KEYBOARD;

            uint64_t ulNow = get_time_in_nanos();

            static uint32_t s_uSequence = 0;
            uint32_t uTimeLo = static_cast<uint32_t>( ulNow & 0xffffffff );
            uint32_t uTimeHi = static_cast<uint32_t>( ulNow >> 32 );
            gamescope_action_binding_send_triggered( GetResource(), s_uSequence++, uTriggerFlags, uTimeLo, uTimeHi );

            if ( uArmFlags & GAMESCOPE_ACTION_BINDING_ARM_FLAG_ONE_SHOT )
                Disarm();

            return bBlockInput;
        }

        static std::span<CGamescopeActionBinding *> GetBindings()
        {
            return s_Bindings;
        }

    private:
        std::string m_sDescription;
        std::vector<Keybind_t> m_KeyboardTriggers;

        std::optional<uint32_t> m_ouArmFlags;

        static std::vector<CGamescopeActionBinding *> s_Bindings;
    };

	const struct gamescope_action_binding_interface CGamescopeActionBinding::Implementation =
	{
		.destroy = WL_PROTO_DESTROY(),
        .set_description = WL_PROTO( CGamescopeActionBinding, SetDescription ),
        .add_keyboard_trigger = WL_PROTO( CGamescopeActionBinding, AddKeyboardTrigger ),
        .clear_triggers = WL_PROTO( CGamescopeActionBinding, ClearTriggers ),
        .arm = WL_PROTO( CGamescopeActionBinding, Arm ),
        .disarm = WL_PROTO( CGamescopeActionBinding, Disarm ),
	};

    std::vector<CGamescopeActionBinding *> CGamescopeActionBinding::s_Bindings;

    //////////////////////////////////
    // CGamescopeActionBindingManager
    //////////////////////////////////
	class CGamescopeActionBindingManager : public CWaylandResource
	{
	public:
		WL_PROTO_DEFINE( gamescope_action_binding_manager, 1 );
		WL_PROTO_DEFAULT_CONSTRUCTOR();

		void CreateActionBinding( uint32_t uId )
		{
			CWaylandResource::Create<CGamescopeActionBinding>( m_pClient, m_uVersion, uId );
		}
	};

	const struct gamescope_action_binding_manager_interface CGamescopeActionBindingManager::Implementation =
	{
		.destroy = WL_PROTO_DESTROY(),
		.create_action_binding = WL_PROTO( CGamescopeActionBindingManager, CreateActionBinding ),
	};

}
