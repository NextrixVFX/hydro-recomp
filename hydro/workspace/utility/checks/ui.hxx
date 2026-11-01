#pragma once

#include <workspace/core/ui/action.hxx>
#include <workspace/core/ui/entity.hxx>
#include <workspace/core/ui/hud.hxx>
#include <workspace/core/ui/button.hxx>
#include <workspace/core/ui/draw.hxx>
#include <workspace/core/ui/flow.hxx>
#include <workspace/core/ui/screen.hxx>
#include <workspace/core/ui/wiring.hxx>
#include <workspace/core/ui/compose.hxx>
#include <workspace/core/ui/gauge.hxx>

#include <algorithm>
#include <array>
#include <cstdio>

// The ui area's oracle-backed checks (one file per parallel agent; included by check.hxx after every
// shared check, so anything above is usable here). verify_ui_all runs them all.

// The front end's projects as entity_load_project builds them (ui_world: the
// captured ctor facts and components, the VuTemplateAssets served by name, the
// UI property loads), one tree per golden project, pre-order.
struct ui_loaded_t
{
    std::map< std::string, value_t > templates;
    std::vector< value_t > documents;
    std::vector< std::unique_ptr< entity_t > > roots;
    std::vector< std::vector< std::pair< int, const entity_t* > > > trees;
};

inline const ui_loaded_t& ui_loaded( )
{
    static ui_loaded_t loaded;
    static bool done = false;
    if ( done )
        return loaded;
    done = true;
    for ( std::size_t t = 0; t < g_golden_ui_templates_count; ++t )
    {
        const golden_ui_document_t& doc = g_golden_ui_templates[ t ];
        const unsigned char* raw = reinterpret_cast< const unsigned char* >( doc.m_words );
        loaded.templates[ doc.m_name ] = c_reader( std::vector< std::uint8_t >( raw, raw + doc.m_bytes ) ).parse( );
    }
    std::vector< std::string > names( g_golden_entity_templates, g_golden_entity_templates + g_golden_entity_templates_count );
    static const entity_registry_t registry = build_entity_registry( names );
    std::map< std::string, value_t >* templates = &loaded.templates;
    const entity_world_t world = ui_world( registry, [ templates ]( const std::string& name ) -> const value_t*
    {
        const auto found = templates->find( "VuTemplateAsset/" + name );
        return found == templates->end( ) ? nullptr : &found->second;
    } );
    loaded.documents.reserve( g_golden_ui_projects_count );
    for ( std::size_t p = 0; p < g_golden_ui_projects_count; ++p )
    {
        const golden_ui_project_t& project = g_golden_ui_projects[ p ];
        const unsigned char* raw = reinterpret_cast< const unsigned char* >( project.m_words );
        loaded.documents.push_back( c_reader( std::vector< std::uint8_t >( raw, raw + project.m_bytes ) ).parse( ) );
        loaded.roots.push_back( entity_load_project( member( loaded.documents.back( ), "VuProject" ), project.m_path, world ) );
        std::vector< std::pair< int, const entity_t* > > tree;
        const auto walk = [ &tree ]( const auto& self, const entity_t& e, int parent ) -> void
        {
            const int index = static_cast< int >( tree.size( ) );
            tree.push_back( { parent, &e } );
            for ( const auto& child : e.m_children )
                self( self, *child, index );
        };
        if ( loaded.roots.back( ) )
            walk( walk, *loaded.roots.back( ), -1 );
        loaded.trees.push_back( std::move( tree ) );
    }
    return loaded;
}

// entity_load_project on the real front-end / HUD projects against retail's
// VuProject::load 0x556390 (oracle case load_ui_projects): every entity
// pre-order - parent, name, vtable (so: known), +0x14 flags, template owner.
inline bool verify_ui_projects( )
{
    const ui_loaded_t& loaded = ui_loaded( );
    int failed = 0;
    int compared = 0;
    for ( std::size_t p = 0; p < g_golden_ui_projects_count; ++p )
    {
        const golden_ui_project_t& project = g_golden_ui_projects[ p ];
        const auto& tree = loaded.trees[ p ];
        if ( ( loaded.roots[ p ] != nullptr ) != ( project.m_ok != 0 ) || tree.size( ) != static_cast< std::size_t >( project.m_count ) )
        {
            LOG_ERROR( "ui projects: %s built %d entities, retail %d", project.m_asset, static_cast< int >( tree.size( ) ), project.m_count );
            ++failed;
            continue;
        }
        for ( int k = 0; k < project.m_count; ++k )
        {
            const golden_ui_entity_t& want = g_golden_ui_entities[ project.m_first + k ];
            const entity_t& have = *tree[ k ].second;
            ++compared;
            const std::string want_template = want.m_template ? want.m_template : "";
            if ( !have.m_ctor || tree[ k ].first != want.m_parent || have.m_name != want.m_name || have.m_ctor->m_vtable != want.m_vtable ||
                 have.m_flags != want.m_flags || have.m_template != want_template )
            {
                if ( ++failed <= 8 )
                    LOG_ERROR( "ui projects: %s entity %d is '%s' (%s) flags %u, retail '%s' flags %u", project.m_asset, k, have.m_name.c_str( ),
                        have.m_ctor ? "known" : "UNKNOWN", have.m_flags, want.m_name, want.m_flags );
            }
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "entity_load_project matches retail's VuProject::load 0x556390 on %d front-end / HUD projects: %d entities - "
                     "parents, names, child order, vtables, flags, template owners",
            static_cast< int >( g_golden_ui_projects_count ), compared );
    else
        LOG_ERROR( "the front-end project loads FAILED %d checks against 0x556390", failed );
    return failed == 0;
}

// One ui_state_t's nodes against retail's golden nodes (name, class, then the
// target bit for bit / text exactly); returns the mismatches.
inline int ui_compare_nodes( const ui_state_t& s, int first, int count, const char* where, const char* entity, int& compared, int& logged )
{
    if ( s.m_count != count )
    {
        if ( ++logged <= 8 )
            LOG_ERROR( "ui: %s '%s' has %d nodes, retail %d", where, entity, s.m_count, count );
        return 1;
    }
    int failed = 0;
    for ( int n = 0; n < s.m_count; ++n )
    {
        const golden_ui_node_t& node = g_golden_ui_nodes[ first + n ];
        const ui_property_row_t& row = *s.m_rows[ n ];
        const ui_slot_t& slot = s.m_value[ n ];
        ++compared;
        bool same = std::strcmp( row.m_name, node.m_name ) == 0 && row.m_vtable == node.m_vtable;
        if ( same && node.m_kind == 1 )
            same = slot.m_text == node.m_text;
        else if ( same && node.m_kind == 2 )
        {
            const int width = ui_width( row.m_kind );
            for ( int w = 0; w < width; ++w )
                same = same && slot.m_bits[ w ] == node.m_bits[ w ];
        }
        else if ( same )
            same = false;  // retail's target was not read: a class the case does not know
        if ( !same )
        {
            ++failed;
            if ( ++logged <= 8 )
                LOG_ERROR( "ui: %s '%s'.%s = 0x%X '%s', retail 0x%X '%s'", where, entity, node.m_name, slot.m_bits[ 0 ],
                    slot.m_text.c_str( ), node.m_bits[ 0 ], node.m_text ? node.m_text : "" );
        }
    }
    return failed;
}

// The UI property loads (ui_entity_load_properties over schema.hxx's rows)
// against every property node retail's load left, entity by entity, in the
// same projects. Nodes of types without rows are counted as skipped (said,
// not compared).
inline bool verify_ui_properties( )
{
    const ui_loaded_t& loaded = ui_loaded( );
    int failed = 0;
    int logged = 0;
    int compared = 0;
    int skipped = 0;
    for ( std::size_t p = 0; p < g_golden_ui_projects_count; ++p )
    {
        const golden_ui_project_t& project = g_golden_ui_projects[ p ];
        const auto& tree = loaded.trees[ p ];
        if ( tree.size( ) != static_cast< std::size_t >( project.m_count ) )
            continue;  // verify_ui_projects reports it
        for ( int k = 0; k < project.m_count; ++k )
        {
            const golden_ui_entity_t& want = g_golden_ui_entities[ project.m_first + k ];
            const entity_t& have = *tree[ k ].second;
            int count = 0;
            if ( !ui_rows_for( have.m_type, count ) || !have.m_state )
            {
                skipped += want.m_nodes;
                continue;
            }
            failed += ui_compare_nodes( *static_cast< const ui_state_t* >( have.m_state.get( ) ), want.m_first_node, want.m_nodes,
                project.m_asset, want.m_name, compared, logged );
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "the UI property loads match retail's (bool / float / angle / int / int enum / colour / vector2 / rect / vector3 / "
                     "rotation / the string family) on %d front-end / HUD projects (the last hand-built): %d property targets bit "
                     "for bit (%d nodes on types without rows: not compared)",
            static_cast< int >( g_golden_ui_projects_count ), compared, skipped );
    else
        LOG_ERROR( "the UI property loads FAILED %d checks against retail's", failed );
    return failed == 0;
}

// The components after the same loads: each entity's +0x48 list (vtables, in
// order) against retail's, then every node of a component with a state - the
// transform (hydro world/transform.hxx: Position = local row 3, Rotation,
// Scale) and the captured ones (ui_components_for) - bit for bit.
inline bool verify_ui_components( )
{
    const ui_loaded_t& loaded = ui_loaded( );
    int failed = 0;
    int logged = 0;
    int compared = 0;
    int lists = 0;
    for ( std::size_t p = 0; p < g_golden_ui_projects_count; ++p )
    {
        const golden_ui_project_t& project = g_golden_ui_projects[ p ];
        const auto& tree = loaded.trees[ p ];
        if ( tree.size( ) != static_cast< std::size_t >( project.m_count ) )
            continue;
        for ( int k = 0; k < project.m_count; ++k )
        {
            const golden_ui_entity_t& want = g_golden_ui_entities[ project.m_first + k ];
            entity_t& have = const_cast< entity_t& >( *tree[ k ].second );
            ++lists;
            bool same = have.m_components.size( ) == static_cast< std::size_t >( want.m_components );
            for ( int c = 0; same && c < want.m_components; ++c )
                same = have.m_components[ c ].m_vtable == g_golden_ui_components[ want.m_first_component + c ].m_vtable;
            if ( !same )
            {
                ++failed;
                if ( ++logged <= 8 )
                    LOG_ERROR( "ui components: %s '%s' has %d components, retail %d (or a vtable differs)", project.m_asset, want.m_name,
                        static_cast< int >( have.m_components.size( ) ), want.m_components );
                continue;
            }
            for ( int c = 0; c < want.m_components; ++c )
            {
                const golden_ui_component_t& gc = g_golden_ui_components[ want.m_first_component + c ];
                entity_component_t& component = have.m_components[ c ];
                if ( component.m_vtable == k_transform_component_vtable && component.m_state )
                {
                    const transform_state_t* s = static_cast< const transform_state_t* >( component.m_state.get( ) );
                    const float* lanes[ 3 ] = { s->c.local + 12, s->c.rotation, s->c.scale };
                    for ( int n = 0; n < gc.m_nodes && n < 3; ++n )
                    {
                        const golden_ui_node_t& node = g_golden_ui_nodes[ gc.m_first_node + n ];
                        std::uint32_t bits[ 3 ];
                        std::memcpy( bits, lanes[ n ], sizeof( bits ) );
                        ++compared;
                        if ( gc.m_nodes != 3 || node.m_kind != 2 || bits[ 0 ] != node.m_bits[ 0 ] || bits[ 1 ] != node.m_bits[ 1 ] ||
                             bits[ 2 ] != node.m_bits[ 2 ] )
                        {
                            ++failed;
                            if ( ++logged <= 8 )
                                LOG_ERROR( "ui components: %s '%s' transform %s = 0x%X, retail 0x%X", project.m_asset, want.m_name,
                                    node.m_name, bits[ 0 ], node.m_bits[ 0 ] );
                        }
                    }
                }
                else if ( ui_is_component_state( have, component ) )
                    failed += ui_compare_nodes( *static_cast< const ui_state_t* >( component.m_state.get( ) ), gc.m_first_node, gc.m_nodes,
                        project.m_asset, want.m_name, compared, logged );
                else if ( gc.m_nodes != 0 )
                {
                    ++failed;
                    if ( ++logged <= 8 )
                        LOG_ERROR( "ui components: %s '%s' component 0x%X has %d nodes retail and no state here", project.m_asset,
                            want.m_name, gc.m_vtable, gc.m_nodes );
                }
            }
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "the front end's components match retail's after VuEntity::load's Components step (0x501F70 / 0x502010) on %d "
                     "projects: %d component lists (vtables in order), %d node targets bit for bit (transform Position / "
                     "Rotation / Scale, VuScriptComponent Enable, VuUIDrawComponent Depth, Vu3dDrawComponent Reflect / Shadow)",
            static_cast< int >( g_golden_ui_projects_count ), lists, compared );
    else
        LOG_ERROR( "the front end's components FAILED %d checks against retail's", failed );
    return failed == 0;
}

// c_front_end (hydro core/ui/flow.hxx) against retail's VuFrontEndGameMode run
// end to end (oracle case front_end_flow): the ctor's event registrations
// (name -> handler), the FSM it builds (states: name, flag, hash, the four
// delegates' functions; transitions; conditions: name, hash, value / pulse),
// TransitionDuration from the real ConstantDB; then every step of the scripted
// flow (fade writes, the start, ticks, the eight handlers) - the tick's return,
// the hooks reached in order (sound / project load / analytics) and the whole
// state after: next mode, next / current screen, the stack, the settings
// depth, gameutil's operation / screen, the FSM's state, its time bit for bit,
// every condition, the fade manager.
inline bool verify_ui_front_end( )
{
    const unsigned* cdb = g_golden_vujb_words + g_golden_vujb_offsets[ 0 ];
    const unsigned char* cdb_raw = reinterpret_cast< const unsigned char* >( cdb + 2 );
    const value_t constant_db = c_reader( std::vector< std::uint8_t >( cdb_raw, cdb_raw + cdb[ 1 ] ) ).parse( );
    std::vector< std::pair< std::string, std::string > > log;
    front_end_engine_t engine;
    engine.m_display = []( int& w, int& h ) { w = 1280; h = 720; };
    engine.m_sound = [ &log ]( const char* name ) { log.push_back( { "sound", name } ); };
    engine.m_load_project = [ &log ]( const std::string& name ) { log.push_back( { "load", name } ); return 0; };
    engine.m_analytics = [ &log ]( const char* event, const std::string& ) { log.push_back( { "analytics", event } ); };
    c_front_end fe( engine, constant_db );
    int failed = 0;
    auto bad = [ &failed ]( const char* what, int step )
    {
        if ( ++failed <= 8 )
            LOG_ERROR( "ui front end: %s differs from retail (step %d)", what, step );
    };
    std::uint32_t td = 0;
    std::memcpy( &td, &fe.m_transition_duration, 4 );
    if ( td != g_golden_fe_transition_duration )
        bad( "TransitionDuration", -1 );
    if ( fe.m_events.size( ) != g_golden_fe_registered_count )
        bad( "the event registrations", -1 );
    for ( std::size_t k = 0; k < g_golden_fe_registered_count && k < fe.m_events.size( ); ++k )
        if ( std::strcmp( fe.m_events[ k ].m_name, g_golden_fe_registered[ k ].m_name ) != 0 ||
             fe.m_events[ k ].m_address != g_golden_fe_registered[ k ].m_handler )
            bad( "an event registration", -1 );
    if ( fe.m_fsm.m_states.size( ) != g_golden_fe_states_count || fe.m_fsm.m_conditions.size( ) != g_golden_fe_conditions_count )
        bad( "the FSM's state / condition count", -1 );
    for ( std::size_t k = 0; k < g_golden_fe_states_count && k < fe.m_fsm.m_states.size( ); ++k )
    {
        const golden_fe_state_t& want = g_golden_fe_states[ k ];
        const fsm_state_t& have = *fe.m_fsm.m_states[ k ];
        bool same = have.m_name == want.m_name && static_cast< int >( have.m_flag ) == want.m_flag && have.m_hash == want.m_hash &&
                    have.m_transitions.size( ) == static_cast< std::size_t >( want.m_transitions );
        const bool bound[ 4 ] = { static_cast< bool >( have.m_enter ), static_cast< bool >( have.m_exit ), static_cast< bool >( have.m_tick ),
                                  static_cast< bool >( have.m_draw ) };
        for ( int m = 0; m < 4; ++m )
            same = same && have.m_address[ m ] == want.m_methods[ m ] && bound[ m ] == ( want.m_methods[ m ] != 0 );
        for ( int t = 0; same && t < want.m_transitions; ++t )
            same = have.m_transitions[ t ].first == g_golden_fe_transitions[ want.m_first_transition + t ].m_condition &&
                   have.m_transitions[ t ].second == g_golden_fe_transitions[ want.m_first_transition + t ].m_target;
        if ( !same )
            bad( "an FSM state", static_cast< int >( k ) );
    }
    for ( std::size_t k = 0; k < g_golden_fe_conditions_count && k < fe.m_fsm.m_conditions.size( ); ++k )
    {
        const fsm_condition_t& c = fe.m_fsm.m_conditions[ k ];
        if ( c.m_name != g_golden_fe_conditions[ k ].m_name || c.m_hash != g_golden_fe_conditions[ k ].m_hash ||
             ( c.m_value | ( c.m_pulse << 8 ) ) != g_golden_fe_conditions[ k ].m_word )
            bad( "an FSM condition", static_cast< int >( k ) );
    }
    for ( std::size_t s = 0; s < g_golden_fe_steps_count; ++s )
    {
        const golden_fe_step_t& st = g_golden_fe_steps[ s ];
        const int step = static_cast< int >( s );
        float value = 0.0f;
        std::memcpy( &value, &st.m_bits, 4 );
        log.clear( );
        std::string ret;
        const std::string op = st.m_op;
        if ( op == "fade" )
            engine.m_fade.m_rate = value;
        else if ( op == "fadelen" )
            engine.m_fade.m_duration = value;
        else if ( op == "start" )
            fe.start( );
        else if ( op == "tick" )
            ret = fe.tick( value );
        else
        {
            ui_params_t params;
            for ( int k = 0; k < st.m_strings; ++k )
                params.add_string( g_golden_fe_strings[ st.m_first_string + k ] );
            fe.handle( st.m_op, params );
        }
        if ( st.m_ret && ret != st.m_ret )
            bad( "the tick's return", step );
        bool same = log.size( ) == static_cast< std::size_t >( st.m_logs );
        for ( int k = 0; same && k < st.m_logs; ++k )
            same = log[ k ].first == g_golden_fe_logs[ st.m_first_log + k ].m_kind && log[ k ].second == g_golden_fe_logs[ st.m_first_log + k ].m_text;
        if ( !same )
            bad( "the hooks reached", step );
        if ( fe.m_mode != st.m_mode || fe.m_next != st.m_next || fe.m_current != st.m_current )
            bad( "mode / next / current", step );
        same = fe.m_stack.size( ) == static_cast< std::size_t >( st.m_stack );
        for ( int k = 0; same && k < st.m_stack; ++k )
            same = fe.m_stack[ k ] == g_golden_fe_strings[ st.m_first_stack + k ];
        if ( !same )
            bad( "the screen stack", step );
        if ( static_cast< unsigned >( fe.m_depth ) != st.m_depth || engine.m_operation != st.m_operation || engine.m_screen != st.m_screen )
            bad( "depth / operation / screen", step );
        if ( !fe.m_fsm.current( ) || fe.m_fsm.current( )->m_name != st.m_state )
            bad( "the FSM's state", step );
        std::uint32_t time = 0;
        std::memcpy( &time, &fe.m_fsm.m_time, 4 );
        if ( time != st.m_time )
            bad( "the FSM's time", step );
        same = fe.m_fsm.m_conditions.size( ) == static_cast< std::size_t >( st.m_condition_count );
        for ( int k = 0; same && k < st.m_condition_count; ++k )
            same = static_cast< unsigned >( fe.m_fsm.m_conditions[ k ].m_value | ( fe.m_fsm.m_conditions[ k ].m_pulse << 8 ) ) == st.m_conditions[ k ];
        if ( !same )
            bad( "the conditions", step );
        const float fade[ 3 ] = { engine.m_fade.m_duration, engine.m_fade.m_value, engine.m_fade.m_rate };
        std::uint32_t fade_bits[ 3 ];
        std::memcpy( fade_bits, fade, sizeof( fade ) );
        if ( fade_bits[ 0 ] != st.m_fade[ 0 ] || fade_bits[ 1 ] != st.m_fade[ 1 ] || fade_bits[ 2 ] != st.m_fade[ 2 ] )
            bad( "the fade manager", step );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "the front end's screen flow matches retail's VuFrontEndGameMode (ctor 0x4BEEA0, VuFSM 0x555850 / 0x555920 / "
                     "0x555AD0, tick 0x4BFC10, the eight handlers, the states' methods): %d registrations, %d states, %d conditions, "
                     "%d scripted steps - stack, screens, depth, FSM state / time, conditions, fade, hooks",
            static_cast< int >( g_golden_fe_registered_count ), static_cast< int >( g_golden_fe_states_count ),
            static_cast< int >( g_golden_fe_conditions_count ), static_cast< int >( g_golden_fe_steps_count ) );
    else
        LOG_ERROR( "the front end's screen flow FAILED %d checks against retail's", failed );
    return failed == 0;
}

inline std::string ui_hex( const ui_params_t& p )
{
    static const char* digits = "0123456789abcdef";
    std::string s;
    for ( int k = 0; k < p.m_size; ++k )
    {
        s += digits[ p.m_bytes[ k ] >> 4 ];
        s += digits[ p.m_bytes[ k ] & 15 ];
    }
    return s;
}

// c_ui_button (hydro core/ui/button.hxx) against retail's VuBarracudaUIButtonEntity
// handlers (oracle case ui_button): the id of "Main/Buttons/Play" (0x500D50 /
// 0x500BF0), then every scripted step - the VuRetVal, the broadcasts / fires
// (params byte for byte) / sounds in order, the state bytes, the press state,
// String ID.
inline bool verify_ui_button( )
{
    std::vector< std::array< std::string, 3 > > log;
    ui_button_hooks_t hooks;
    hooks.m_broadcast = [ &log ]( std::uint32_t hash, const ui_params_t& p )
    {
        char h[ 16 ];
        std::snprintf( h, sizeof( h ), "0x%08X", hash );
        log.push_back( { "broadcast", h, ui_hex( p ) } );
    };
    // The case observes a fire where it is delivered - at the one fake connection each connected plug has
    // (NavLeft has none: its fire reaches nobody and leaves no record).
    hooks.m_fire = [ &log ]( const char* plug, const ui_params_t& p )
    {
        if ( std::strcmp( plug, "NavLeft" ) != 0 )
            log.push_back( { "fire", plug, ui_hex( p ) } );
    };
    hooks.m_connections = []( const char* plug ) { return std::strcmp( plug, "NavLeft" ) == 0 ? 0 : 1; };
    hooks.m_sound = [ &log ]( const char* name ) { log.push_back( { "sound", name, "" } ); };
    c_ui_button b( ui_entity_id( "Main/Buttons/Play" ), hooks );
    int failed = 0;
    auto bad = [ &failed ]( const char* what, int step )
    {
        if ( ++failed <= 8 )
            LOG_ERROR( "ui button: %s differs from retail (step %d)", what, step );
    };
    if ( b.m_id != g_golden_btn_id )
        bad( "the entity id", -1 );
    for ( std::size_t s = 0; s < g_golden_btn_steps_count; ++s )
    {
        const golden_btn_step_t& st = g_golden_btn_steps[ s ];
        const int step = static_cast< int >( s );
        const std::string op = st.m_op;
        ui_params_t p;
        for ( int k = 0; k < st.m_records; ++k )
        {
            const golden_btn_record_t& r = g_golden_btn_records[ st.m_first_record + k ];
            if ( r.m_kind == 4 )
                p.add_string( r.m_text );
            else if ( r.m_kind == 8 )
                ui_params_add( p, 1, b.m_id );
            else if ( r.m_kind != 9 )
                ui_params_add( p, static_cast< std::uint32_t >( r.m_kind ), r.m_value );
        }
        log.clear( );
        std::string ret;
        if ( op == "write" )
        {
            const unsigned w = g_golden_btn_records[ st.m_first_record ].m_value;
            if ( ( w & 0xFFFF ) == 106 )
                b.m_can_focus = ( w >> 16 ) != 0;
            else
                bad( "an unknown write", step );
        }
        else if ( op == "SetFocus" ) { b.set_focus( ); ret = "0000000000000000"; }
        else if ( op == "SetStringID" ) { b.set_string_id( p ); ret = "0000000000000000"; }
        else if ( op == "Enable" ) { b.enable( ); ret = "0000000000000000"; }
        else if ( op == "Disable" ) { b.disable( ); ret = "0000000000000000"; }
        else if ( op == "IsEnabled" ) ret = b.is_enabled( ) ? "0000000300000001" : "0000000300000000";
        else if ( op == "OnUISetFocus" ) b.on_set_focus( p );
        else if ( op == "OnUITick" ) b.on_tick( );
        else if ( op == "OnUIEvent" ) b.on_event( p );
        else
            bad( "an unknown op", step );
        if ( ( st.m_ret ? std::string( st.m_ret ) : std::string( ) ) != ret )
            bad( "the VuRetVal", step );
        bool same = log.size( ) == static_cast< std::size_t >( st.m_logs );
        for ( int k = 0; same && k < st.m_logs; ++k )
        {
            const golden_btn_log_t& g = g_golden_btn_logs[ st.m_first_log + k ];
            same = log[ k ][ 0 ] == g.m_kind && log[ k ][ 1 ] == g.m_a && log[ k ][ 2 ] == ( g.m_b ? g.m_b : "" );
        }
        if ( !same )
            bad( "the hooks reached", step );
        char state[ 32 ];
        std::snprintf( state, sizeof( state ), "%02x%02x%02x.%02x%02x%02x%02x%02x", b.m_enabled, b.m_label, b.m_can_focus, b.m_focused,
            b.m_want_focus, b.m_touched, b.m_touch_press, b.m_touch_release );
        if ( std::string( state ) != st.m_state || b.m_press != st.m_press || b.m_string_id != st.m_string || b.m_touch_unknown )
            bad( "the button's state", step );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "c_ui_button matches retail's VuBarracudaUIButtonEntity (SetFocus 0x4428E0, SetStringID 0x442980, Enable / Disable / "
                     "IsEnabled, OnUISetFocus 0x442A40, OnUITick 0x442A90, OnUIEvent 0x442D60, the id 0x500D50 / 0x500BF0) on %d steps - "
                     "returns, broadcasts / fires / sounds with their params, state bytes, String ID",
            static_cast< int >( g_golden_btn_steps_count ) );
    else
        LOG_ERROR( "the front end's button FAILED %d checks against retail's", failed );
    return failed == 0;
}

// c_ui_screen / c_ui_event / the button's event map (hydro core/ui/screen.hxx)
// against retail's VuUIScreenEntity ScreenEnter / ScreenExit / tick input half
// and VuUIEventEntity 0x62E950 delivering to real children (oracle case
// ui_screen_events): every hook in order - the input queries, the Trigger /
// button fires with their params, the sounds.
inline bool verify_ui_screen_events( )
{
    std::vector< std::array< std::string, 3 > > log;
    std::vector< std::string > down;
    c_ui_screen screen;
    screen.m_input = [ &log, &down ]( int pad, const char* name )
    {
        log.push_back( { "query", std::to_string( pad ), name } );
        return std::find( down.begin( ), down.end( ), name ) != down.end( );
    };
    std::vector< std::unique_ptr< c_ui_event > > events;
    std::vector< std::unique_ptr< ui_event_map_t > > maps;
    for ( std::size_t k = 0; k < g_golden_scr_events_count; ++k )
    {
        events.push_back( std::make_unique< c_ui_event >( g_golden_scr_events[ k ][ 1 ] ) );
        const std::string who = g_golden_scr_events[ k ][ 0 ];
        events.back( )->m_fire = [ &log, who ]( const ui_params_t& p ) { log.push_back( { "fire", who, ui_hex( p ) } ); };
        maps.push_back( std::make_unique< ui_event_map_t >( ) );
        events.back( )->events( *maps.back( ) );
        screen.m_children.push_back( maps.back( ).get( ) );
    }
    ui_button_hooks_t hooks;
    hooks.m_fire = [ &log ]( const char* plug, const ui_params_t& p ) { log.push_back( { "fire", std::string( "Play." ) + plug, ui_hex( p ) } ); };
    hooks.m_connections = []( const char* ) { return 1; };
    hooks.m_sound = [ &log ]( const char* name ) { log.push_back( { "sound", name, "" } ); };
    c_ui_button button( ui_entity_id( "Main/Play" ), hooks );
    button.m_focused = true;
    button.m_want_focus = true;
    maps.push_back( std::make_unique< ui_event_map_t >( ) );
    ui_button_events( button, *maps.back( ) );
    screen.m_children.push_back( maps.back( ).get( ) );
    int failed = 0;
    for ( std::size_t s = 0; s < g_golden_scr_steps_count; ++s )
    {
        const golden_scr_step_t& st = g_golden_scr_steps[ s ];
        const std::string op = st.m_op;
        log.clear( );
        down.assign( g_golden_scr_down + st.m_first_down, g_golden_scr_down + st.m_first_down + st.m_downs );
        if ( op == "enter" )
            screen.screen_enter( );
        else if ( op == "exit" )
            screen.screen_exit( );
        else
            screen.tick( 0.016666668f, st.m_input != 0 );
        bool same = log.size( ) == static_cast< std::size_t >( st.m_logs );
        for ( int k = 0; same && k < st.m_logs; ++k )
        {
            const golden_btn_log_t& g = g_golden_scr_logs[ st.m_first_log + k ];
            same = log[ k ][ 0 ] == g.m_kind && log[ k ][ 1 ] == g.m_a && log[ k ][ 2 ] == g.m_b;
        }
        if ( !same && ++failed <= 8 )
            LOG_ERROR( "ui screen events: step %d (%s) reached %d hooks, retail %d (or one differs)", static_cast< int >( s ), st.m_op,
                static_cast< int >( log.size( ) ), st.m_logs );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "a screen's UI events match retail's (VuUIScreenEntity 0x629A70 / 0x629B00 / 0x629B90 + 0x629F10 / 0x5011A0 / "
                     "0x5155C0, VuUIEventEntity 0x62E950, the button's event map) on %d steps: input queries, fires with params, sounds",
            static_cast< int >( g_golden_scr_steps_count ) );
    else
        LOG_ERROR( "a screen's UI events FAILED %d checks against retail's", failed );
    return failed == 0;
}

// c_front_end::enter (hydro core/ui/flow.hxx) against retail's slot 2
// 0x4BF630 on three game-data variants (oracle case front_end_initial), then
// two ticks each: the hooks in order, next / current, the stack, the FSM state,
// time and conditions, the preload byte, the tick's return.
inline bool verify_ui_front_end_enter( )
{
    const unsigned* cdb = g_golden_vujb_words + g_golden_vujb_offsets[ 0 ];
    const unsigned char* cdb_raw = reinterpret_cast< const unsigned char* >( cdb + 2 );
    const value_t constant_db = c_reader( std::vector< std::uint8_t >( cdb_raw, cdb_raw + cdb[ 1 ] ) ).parse( );
    int failed = 0;
    int steps = 0;
    for ( std::size_t v = 0; v < g_golden_fei_variants_count; ++v )
    {
        const golden_fei_variant_t& var = g_golden_fei_variants[ v ];
        std::vector< std::pair< std::string, std::string > > log;
        front_end_engine_t engine;
        engine.m_display = []( int& w, int& h ) { w = 1280; h = 720; };
        engine.m_sound = [ &log ]( const char* name ) { log.push_back( { "sound", name } ); };
        engine.m_load_project = [ &log ]( const std::string& name ) { log.push_back( { "load", name } ); return 0; };
        engine.m_analytics = [ &log ]( const char* event, const std::string& ) { log.push_back( { "analytics", event } ); };
        engine.m_preload_action_game = [ &log ] { log.push_back( { "preload", "ActionGameData" } ); };
        engine.m_title_voice = [ &log ]( const char* name ) { log.push_back( { "voice", name } ); return false; };
        engine.m_platform = [ &log ]( const char* what, int arg )
        {
            const std::string w = what;
            log.push_back( { w, w == "mode manager" ? "slot 9" : ( w == "profile" || w == "cloud" ) ? "" : std::to_string( arg ) } );
        };
        value_t data;
        if ( var.m_initial || var.m_results )
        {
            data.kind = value_t::kind_t::object;
            data.tag = 6;
        }
        if ( var.m_initial )
        {
            value_t s;
            s.kind = value_t::kind_t::text;
            s.tag = 4;
            s.text = var.m_initial;
            data.object.push_back( { "InitialScreen", s } );
        }
        if ( var.m_results )
        {
            value_t place;
            place.kind = value_t::kind_t::integer;
            place.tag = 1;
            place.integer = 1;
            value_t results;
            results.kind = value_t::kind_t::object;
            results.tag = 6;
            results.object.push_back( { "Place", place } );
            data.object.push_back( { "GameResults", results } );
        }
        c_front_end fe( engine, constant_db );
        for ( int s = 0; s < var.m_steps; ++s )
        {
            const golden_fei_step_t& st = g_golden_fei_steps[ var.m_first_step + s ];
            log.clear( );
            std::string ret;
            if ( std::strcmp( st.m_op, "enter" ) == 0 )
                fe.enter( data );
            else
                ret = fe.tick( 0.016666668f );
            ++steps;
            bool same = log.size( ) == static_cast< std::size_t >( st.m_logs ) && ( !st.m_ret || ret == st.m_ret ) &&
                        fe.m_next == st.m_next && fe.m_current == st.m_current && fe.m_stack.size( ) == static_cast< std::size_t >( st.m_stack ) &&
                        fe.m_fsm.current( ) && fe.m_fsm.current( )->m_name == st.m_state &&
                        static_cast< int >( fe.m_preload_pending ) == st.m_preload &&
                        fe.m_fsm.m_conditions.size( ) == static_cast< std::size_t >( st.m_condition_count );
            for ( int k = 0; same && k < st.m_logs; ++k )
                same = log[ k ].first == g_golden_fei_logs[ st.m_first_log + k ].m_kind && log[ k ].second == g_golden_fei_logs[ st.m_first_log + k ].m_a;
            for ( int k = 0; same && k < st.m_stack; ++k )
                same = fe.m_stack[ k ] == g_golden_fei_stack[ st.m_first_stack + k ];
            for ( int k = 0; same && k < st.m_condition_count; ++k )
                same = static_cast< unsigned >( fe.m_fsm.m_conditions[ k ].m_value | ( fe.m_fsm.m_conditions[ k ].m_pulse << 8 ) ) == st.m_conditions[ k ];
            std::uint32_t time = 0;
            std::memcpy( &time, &fe.m_fsm.m_time, 4 );
            if ( !same || time != st.m_time )
            {
                if ( ++failed <= 8 )
                    LOG_ERROR( "ui front end enter: variant %s step %d (%s) differs from retail", var.m_name, s, st.m_op );
            }
        }
    }
    if ( failed == 0 )
        LOG_SUCCESS( "the front end's enter matches retail's 0x4BF630 on %d game-data variants (%d steps with the ticks after): "
                     "the first screen, the stack, the title voice-over / preload, the FSM start, every hook in order",
            static_cast< int >( g_golden_fei_variants_count ), steps );
    else
        LOG_ERROR( "the front end's enter FAILED %d checks against retail's", failed );
    return failed == 0;
}

// c_ui_action (hydro core/ui/action.hxx) against retail's screen actions
// (oracle case ui_actions): every authored VuPush / Pop / Set / Reload
// ScreenUIAction in the real Screens projects as entity_load_project built it
// (its "Screen" as the load left it), and hand-made ones with names around
// VuParams' 0x80 limit (their ctors' property nodes too), each fired through
// Trigger: the VuRetVal and every broadcast, params byte for byte.
inline bool verify_ui_actions( )
{
    const ui_loaded_t& loaded = ui_loaded( );
    int failed = 0;
    auto bad = [ &failed ]( const char* what, int k )
    {
        if ( ++failed <= 8 )
            LOG_ERROR( "ui actions: %s differs from retail (row %d)", what, k );
    };
    int authored = 0;
    for ( std::size_t k = 0; k < g_golden_acts_count; ++k )
    {
        const golden_act_t& want = g_golden_acts[ k ];
        const int row = static_cast< int >( k );
        ui_action_kind_t kind;
        if ( !ui_action_kind_of( want.m_class, kind ) )
        {
            bad( "the class", row );
            continue;
        }
        std::string screen = want.m_screen ? want.m_screen : "";
        if ( want.m_project )
        {
            const entity_t* have = nullptr;
            for ( std::size_t p = 0; p < g_golden_ui_projects_count; ++p )
                if ( std::strcmp( g_golden_ui_projects[ p ].m_asset, want.m_project ) == 0 && want.m_index >= 0 &&
                     static_cast< std::size_t >( want.m_index ) < loaded.trees[ p ].size( ) )
                    have = loaded.trees[ p ][ static_cast< std::size_t >( want.m_index ) ].second;
            if ( !have || have->m_type != want.m_class || have->m_name != want.m_name || ui_is_state( *have ) != ( want.m_screen != nullptr ) )
            {
                if ( failed < 8 )
                    LOG_ERROR( "ui actions: %s #%d is %s '%s' (state %d), retail %s '%s'", want.m_project, want.m_index, have ? have->m_type.c_str( ) : "-",
                        have ? have->m_name.c_str( ) : "-", have ? static_cast< int >( ui_is_state( *have ) ) : -1, want.m_class, want.m_name );
                bad( "the authored entity", row );
                continue;
            }
            ++authored;
            const ui_state_t* s = want.m_screen ? static_cast< const ui_state_t* >( have->m_state.get( ) ) : nullptr;
            bool found = false;
            for ( int n = 0; s && n < s->m_count; ++n )
                if ( std::strcmp( s->m_rows[ n ]->m_name, "Screen" ) == 0 )
                {
                    found = true;
                    if ( !want.m_screen || s->m_value[ n ].m_text != want.m_screen )
                        bad( "the loaded Screen", row );
                    screen = s->m_value[ n ].m_text;
                }
            if ( found != ( want.m_screen != nullptr ) )
                bad( "the Screen node's presence", row );
        }
        else
        {
            int count = 0;
            const ui_property_row_t* first = ui_rows_for( want.m_class, count );
            if ( count != want.m_nodes )
                bad( "the ctor's node count", row );
            for ( int n = 0; first && n < count && n < want.m_nodes; ++n )
                if ( std::strcmp( first[ n ].m_name, g_golden_act_nodes[ want.m_first_node + n ].m_name ) != 0 ||
                     first[ n ].m_vtable != g_golden_act_nodes[ want.m_first_node + n ].m_vtable )
                    bad( "a ctor node", row );
        }
        ui_params_t trigger;
        for ( std::size_t c = 0; want.m_trigger[ c ] && want.m_trigger[ c + 1 ] && trigger.m_size < 128; c += 2 )
            trigger.m_bytes[ trigger.m_size++ ] = static_cast< std::uint8_t >( std::stoul( std::string( want.m_trigger + c, 2 ), nullptr, 16 ) );
        std::vector< std::pair< std::uint32_t, std::string > > log;
        c_ui_action action( kind, screen );
        action.m_broadcast = [ &log ]( std::uint32_t hash, const ui_params_t& p ) { log.push_back( { hash, ui_hex( p ) } ); };
        char ret[ 20 ];
        std::snprintf( ret, sizeof( ret ), "%08X%08X", action.trigger( trigger ), 0u );
        if ( std::strcmp( ret, want.m_ret ) != 0 )
            bad( "the VuRetVal", row );
        bool same = log.size( ) == static_cast< std::size_t >( want.m_logs );
        for ( int n = 0; same && n < want.m_logs; ++n )
            same = log[ n ].first == g_golden_act_logs[ want.m_first_log + n ].m_hash && log[ n ].second == g_golden_act_logs[ want.m_first_log + n ].m_params;
        if ( !same )
            bad( "the broadcasts", row );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "c_ui_action matches retail's screen actions (Trigger 0x472800 -> slot 12: push 0x4718C0, set 0x471600, pop 0x4719C0, "
                     "reload 0x471AB0) on %d authored instances in the loaded Screens projects and %d hand-made: Screen, VuRetVal, "
                     "broadcast hash + params",
            authored, static_cast< int >( g_golden_acts_count ) - authored );
    else
        LOG_ERROR( "the screen actions FAILED %d checks against retail's", failed );
    return failed == 0;
}

// c_ui_hud_element (hydro core/ui/hud.hxx) against retail's VuHUDElementEntity
// slot 12 0x423F40 (oracle case hud_element_text): every Type 0..8 over the
// hand-made worlds - the text, the alpha +264, the StringDB keys asked for.
inline bool verify_ui_hud_element( )
{
    std::vector< std::string > keys;
    ui_hud_hooks_t hooks;
    hooks.m_string = [ &keys ]( const char* key )
    {
        keys.push_back( key );
        return std::string( "<" ) + key + ">";
    };
    int failed = 0;
    c_ui_hud_element fresh( hooks );
    std::uint32_t alpha_bits = 0;
    std::memcpy( &alpha_bits, &fresh.m_alpha, 4 );
    if ( alpha_bits != g_golden_hud_initial_alpha || fresh.m_type != g_golden_hud_initial_type )
    {
        LOG_ERROR( "ui hud element: the ctor's alpha / Type differ from retail" );
        ++failed;
    }
    const auto bits = []( unsigned w )
    {
        float f;
        std::memcpy( &f, &w, 4 );
        return f;
    };
    for ( std::size_t k = 0; k < g_golden_hud_vectors_count; ++k )
    {
        const golden_hud_vector_t& want = g_golden_hud_vectors[ k ];
        const golden_hud_world_t& gw = g_golden_hud_worlds[ want.m_world ];
        std::vector< ui_hud_boat_t > store( static_cast< std::size_t >( gw.m_boats ) );
        ui_hud_world_t world;
        world.m_manager = gw.m_manager != 0;
        world.m_settings = gw.m_settings != 0;
        world.m_local = gw.m_local;
        world.m_boat_count = gw.m_count;
        world.m_units = gw.m_units;
        for ( int b = 0; b < gw.m_boats; ++b )
        {
            const golden_hud_boat_t& g = g_golden_hud_boats[ gw.m_first_boat + b ];
            ui_hud_boat_t& o = store[ static_cast< std::size_t >( b ) ];
            o.m_lap = g.m_lap;
            o.m_time_1200 = bits( g.m_time_1200 );
            o.m_time_1204 = bits( g.m_time_1204 );
            o.m_time_1208 = bits( g.m_time_1208 );
            o.m_place = g.m_place;
            o.m_laps = g.m_laps;
            o.m_wrong_way = bits( g.m_wrong_way );
            world.m_boats.push_back( g.m_present ? &o : nullptr );
        }
        c_ui_hud_element e( hooks );
        e.m_type = want.m_type;
        e.m_alpha = bits( 0x12345678u );
        keys.clear( );
        const std::string& text = e.text( world );
        std::memcpy( &alpha_bits, &e.m_alpha, 4 );
        bool same = text == want.m_text && &text == &e.m_text && alpha_bits == want.m_alpha && keys.size( ) == static_cast< std::size_t >( want.m_keys );
        for ( int n = 0; same && n < want.m_keys; ++n )
            same = keys[ static_cast< std::size_t >( n ) ] == g_golden_hud_keys[ want.m_first_key + n ];
        if ( !same && ++failed <= 8 )
            LOG_ERROR( "ui hud element: world '%s' Type %d gave '%s', retail '%s'", gw.m_name, want.m_type, text.c_str( ), want.m_text );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "c_ui_hud_element matches retail's VuHUDElementEntity text 0x423F40 (+ the time formatter 0x53F5E0) on %d vectors: "
                     "every Type over %d worlds - the text, the alpha +264, the StringDB keys",
            static_cast< int >( g_golden_hud_vectors_count ), static_cast< int >( sizeof( g_golden_hud_worlds ) / sizeof( g_golden_hud_worlds[ 0 ] ) ) );
    else
        LOG_ERROR( "the HUD element's text FAILED %d checks against retail's", failed );
    return failed == 0;
}

// c_ui_hud_speedometer::on_tick (hydro core/ui/hud.hxx) against retail's
// VuHUDSpeedometerEntity OnUITick 0x42B860 (oracle case hud_speedometer_tick):
// the ctor's +224 / +228 / +232, then every scripted step's three fields.
inline bool verify_ui_hud_speedometer( )
{
    c_ui_hud_speedometer s;
    ui_hud_boat_t boat;
    ui_hud_world_t world;
    int failed = 0;
    const auto fields = [ &s ]( std::uint32_t out[ 3 ] )
    {
        out[ 0 ] = s.m_metric;
        std::memcpy( &out[ 1 ], &s.m_v_offset, 4 );
        std::memcpy( &out[ 2 ], &s.m_countdown, 4 );
    };
    std::uint32_t have[ 3 ];
    fields( have );
    if ( have[ 0 ] != g_golden_spd_initial[ 0 ] || have[ 1 ] != g_golden_spd_initial[ 1 ] || have[ 2 ] != g_golden_spd_initial[ 2 ] )
    {
        LOG_ERROR( "ui speedometer: the ctor's fields differ from retail" );
        ++failed;
    }
    for ( std::size_t k = 0; k < g_golden_spd_steps_count; ++k )
    {
        const golden_spd_step_t& st = g_golden_spd_steps[ k ];
        const std::string op = st.m_op;
        if ( op == "world" )
        {
            boat.m_byte_812 = static_cast< std::uint8_t >( st.m_word );
            world = ui_hud_world_t( );
            world.m_manager = true;
            world.m_settings = true;
            world.m_units = st.m_text;
            world.m_boats.push_back( st.m_mode == 2 ? nullptr : &boat );
            world.m_local = st.m_mode == 1 ? 1 : 0;
        }
        else if ( op == "write" )
            std::memcpy( st.m_offset == 228 ? static_cast< void* >( &s.m_v_offset ) : static_cast< void* >( &s.m_countdown ), &st.m_word, 4 );
        else
        {
            std::uint8_t bytes[ 128 ] = { };
            int size = 0;
            for ( std::size_t c = 0; st.m_text[ c ] && st.m_text[ c + 1 ] && size < 128; c += 2 )
                bytes[ size++ ] = static_cast< std::uint8_t >( std::stoul( std::string( st.m_text + c, 2 ), nullptr, 16 ) );
            s.on_tick( bytes, size, world );
        }
        fields( have );
        if ( ( have[ 0 ] != st.m_fields[ 0 ] || have[ 1 ] != st.m_fields[ 1 ] || have[ 2 ] != st.m_fields[ 2 ] || s.m_unknown ) && ++failed <= 8 )
            LOG_ERROR( "ui speedometer: step %d (%s) gave %08X %08X %08X, retail %08X %08X %08X", static_cast< int >( k ), st.m_op, have[ 0 ], have[ 1 ],
                have[ 2 ], st.m_fields[ 0 ], st.m_fields[ 1 ], st.m_fields[ 2 ] );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "c_ui_hud_speedometer matches retail's VuHUDSpeedometerEntity OnUITick 0x42B860 on %d steps: the metric flag, "
                     "the boost texture's V offset (x87 round-to-even wrap), the countdown",
            static_cast< int >( g_golden_spd_steps_count ) );
    else
        LOG_ERROR( "the speedometer's tick FAILED %d checks against retail's", failed );
    return failed == 0;
}

// front_end_engine_t::ui_fade_t (hydro core/ui/flow.hxx) against retail's
// VuFadeManager create 0x4AED60 and tick 0x4AEE50 (oracle case fade_tick):
// the four fields after every step, the draw gate 0x4AEF70, the broadcasts.
inline bool verify_ui_fade( )
{
    using fade_t = front_end_engine_t::ui_fade_t;
    fade_t f = fade_t::created( );
    std::vector< std::string > log;
    const std::function< void( const char* ) > broadcast = [ &log ]( const char* name ) { log.push_back( name ); };
    const auto words = [ &f ]( std::uint32_t out[ 4 ] )
    {
        std::memcpy( &out[ 0 ], &f.m_duration, 4 );
        std::memcpy( &out[ 1 ], &f.m_value, 4 );
        std::memcpy( &out[ 2 ], &f.m_rate, 4 );
        out[ 3 ] = f.m_skip;
    };
    int failed = 0;
    std::uint32_t have[ 4 ];
    words( have );
    if ( std::memcmp( have, g_golden_fade_initial, sizeof( have ) ) != 0 )
    {
        LOG_ERROR( "ui fade: the create's fields differ from retail" );
        ++failed;
    }
    for ( std::size_t k = 0; k < g_golden_fade_steps_count; ++k )
    {
        const golden_fade_step_t& st = g_golden_fade_steps[ k ];
        log.clear( );
        if ( std::strcmp( st.m_op, "write" ) == 0 )
        {
            if ( st.m_offset == 16 )
                f.m_skip = st.m_word;
            else
                std::memcpy( st.m_offset == 8 ? &f.m_value : &f.m_rate, &st.m_word, 4 );
        }
        else
        {
            float delta;
            std::memcpy( &delta, &st.m_word, 4 );
            f.tick( delta, broadcast );
        }
        words( have );
        bool same = std::memcmp( have, st.m_fields, sizeof( have ) ) == 0 && f.draws( ) == ( st.m_draws != 0 ) &&
                    log.size( ) == static_cast< std::size_t >( st.m_logs );
        for ( int n = 0; same && n < st.m_logs; ++n )
            same = log[ static_cast< std::size_t >( n ) ] == g_golden_fade_logs[ st.m_first_log + n ];
        if ( !same && ++failed <= 8 )
            LOG_ERROR( "ui fade: step %d (%s) gave %08X %08X %08X %u, retail %08X %08X %08X %u", static_cast< int >( k ), st.m_op, have[ 0 ], have[ 1 ],
                have[ 2 ], have[ 3 ], st.m_fields[ 0 ], st.m_fields[ 1 ], st.m_fields[ 2 ], st.m_fields[ 3 ] );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "the fade manager matches retail's VuFadeManager (create 0x4AED60, tick 0x4AEE50, draw gate 0x4AEF70) on %d steps: "
                     "value / rate / skip, OnFadeOutComplete / OnFadeInComplete",
            static_cast< int >( g_golden_fade_steps_count ) );
    else
        LOG_ERROR( "the fade manager FAILED %d checks against retail's", failed );
    return failed == 0;
}

inline std::vector< std::uint8_t > ui_unhex( const char* s )
{
    std::vector< std::uint8_t > out;
    for ( std::size_t c = 0; s[ c ] && s[ c + 1 ]; c += 2 )
        out.push_back( static_cast< std::uint8_t >( std::stoul( std::string( s + c, 2 ), nullptr, 16 ) ) );
    return out;
}

// One ui_state_t slot by its node's name, or null.
inline const ui_slot_t* ui_slot_named( const ui_state_t& s, const char* name )
{
    for ( int k = 0; k < s.m_count; ++k )
        if ( std::strcmp( s.m_rows[ k ]->m_name, name ) == 0 )
            return &s.m_value[ k ];
    return nullptr;
}

// The draw's fields out of retail's entity bytes (+0x60..+0x110).
inline ui_text_base_t ui_text_base_of( const std::vector< std::uint8_t >& b, std::uint32_t depth )
{
    ui_text_base_t t;
    const auto at = [ &b ]( int offset ) { return b.data( ) + ( offset - 0x60 ); };
    std::memcpy( t.m_screen_scale, at( 0x68 ), 8 );
    std::memcpy( t.m_rect, at( 0x70 ), 16 );
    std::memcpy( t.m_params, at( 0xA0 ), 60 );
    std::memcpy( &t.m_h_align, at( 0xE4 ), 4 );
    std::memcpy( &t.m_v_align, at( 0xE8 ), 4 );
    t.m_no_clip = *at( 0xEC );
    t.m_single_line = *at( 0xED );
    t.m_wordbreak = *at( 0xEE );
    std::memcpy( &t.m_anchor.m_horizontal, at( 0xF0 ), 4 );
    std::memcpy( &t.m_anchor.m_vertical, at( 0xF4 ), 4 );
    std::memcpy( &t.m_anchor.m_h_ratio, at( 0xF8 ), 4 );
    std::memcpy( &t.m_anchor.m_v_ratio, at( 0xFC ), 4 );
    std::memcpy( t.m_offset, at( 0x100 ), 8 );
    std::memcpy( &t.m_alpha, at( 0x108 ), 4 );
    std::memcpy( &t.m_depth, &depth, 4 );
    return t;
}

// ui_text_draw (hydro core/ui/draw.hxx) against retail's VuUITextBaseEntity draw
// 0x627C60 -> font draw 0x536C10 -> VuGfxSort 0x426B80 / 0x429010 (oracle cases
// text_draw, text_draw_sweep): every new command's 8 words and its record's
// bytes. For the real entities the property-backed fields come from OUR loaded
// tree (entity_load_project) and are compared with retail's bytes first; the
// font params (+0xA0, FontDB - 0x626CF0 not recreated), the offset and the
// alpha come from retail's bytes.
inline bool verify_ui_text_draw( )
{
    // the case's stand-ins (TXT_REGION 0x5D000000): the two shader sets, the fonts
    constexpr std::uint32_t k_material[ 2 ] = { 0x5D001000u, 0x5D001800u };
    constexpr std::uint32_t k_font = 0x0F0E0D0Cu;
    constexpr std::uint32_t k_default_font = 0x0B0A0908u;
    const ui_loaded_t& loaded = ui_loaded( );
    ui_gfx_sort_t sorts[ 2 ];
    for ( ui_gfx_sort_t& s : sorts )
    {
        s.m_buffers[ 0 ].m_capacity = 0x300000;
        s.m_buffers[ 1 ].m_used = 0x35;
        s.m_buffers[ 1 ].m_capacity = 0x100000;
        for ( auto& b : s.m_buffers )
            b.m_memory.resize( static_cast< std::size_t >( b.m_capacity ) );
    }
    int failed = 0, commands = 0, fields = 0, real = 0;
    for ( std::size_t k = 0; k < g_golden_txt_vectors_count; ++k )
    {
        const golden_txt_vector_t& v = g_golden_txt_vectors[ k ];
        const golden_txt_env_t& e = g_golden_txt_envs[ v.m_env ];
        ui_gfx_sort_t& sort = sorts[ v.m_sweep ];
        ui_crop_t crop;
        std::memcpy( &crop.m_matrix[ 0 ], &e.m_crop[ 0 ], 4 );
        std::memcpy( &crop.m_matrix[ 5 ], &e.m_crop[ 1 ], 4 );
        std::memcpy( &crop.m_matrix[ 12 ], &e.m_crop[ 2 ], 4 );
        std::memcpy( &crop.m_matrix[ 13 ], &e.m_crop[ 3 ], 4 );
        ui_font_draw_env_t env;
        std::memcpy( &env.m_matrix[ 0 ], &e.m_matrix[ 0 ], 4 );
        std::memcpy( &env.m_matrix[ 5 ], &e.m_matrix[ 1 ], 4 );
        std::memcpy( &env.m_matrix[ 12 ], &e.m_matrix[ 2 ], 4 );
        std::memcpy( &env.m_matrix[ 13 ], &e.m_matrix[ 3 ], 4 );
        std::memcpy( &env.m_top, &e.m_top, 4 );
        for ( int m = 0; m < 2; ++m )
        {
            env.m_material[ m ] = k_material[ m ];
            env.m_material_key[ m ] = g_golden_txt_material_keys[ m ];
        }
        ui_string_db_t db;
        db.m_present = e.m_sdb != 0;
        db.m_slot12 = e.m_sdb_answer != 0;
        const std::string mode = v.m_font;
        ui_text_font_t font;
        font.m_asset = mode == "asset";
        font.m_asset_font = k_font;
        font.m_default = mode == "default" || ( e.m_default && mode != "none" );
        font.m_default_font = k_default_font;
        const std::vector< std::uint8_t > raw = ui_unhex( v.m_text );
        const std::string text( raw.begin( ), raw.end( ) );
        ui_text_base_t t = ui_text_base_of( ui_unhex( v.m_fields ), v.m_depth );
        if ( v.m_sweep == 0 )
        {
            // our loaded entity: the same project and pre-order index
            const entity_t* have = nullptr;
            for ( std::size_t p = 0; p < g_golden_ui_projects_count && !have; ++p )
                if ( std::strcmp( g_golden_ui_projects[ p ].m_asset, v.m_project ) == 0 && v.m_index < static_cast< int >( loaded.trees[ p ].size( ) ) )
                    have = loaded.trees[ p ][ static_cast< std::size_t >( v.m_index ) ].second;
            const ui_state_t* s = have && ui_is_state( *have ) ? static_cast< const ui_state_t* >( have->m_state.get( ) ) : nullptr;
            const ui_state_t* draw = nullptr;
            for ( const entity_component_t& c : have ? have->m_components : std::vector< entity_component_t >( ) )
                if ( c.m_vtable == 0x738A44u && c.m_state )
                    draw = static_cast< const ui_state_t* >( c.m_state.get( ) );
            ui_text_base_t ours = t;
            bool same = s && draw;
            const auto take = [ & ]( const char* name, void* target, int bytes, const ui_state_t* from )
            {
                const ui_slot_t* slot = from ? ui_slot_named( *from, name ) : nullptr;
                if ( !slot )
                {
                    same = false;
                    return;
                }
                if ( std::memcmp( target, slot->m_bits, static_cast< std::size_t >( bytes ) ) != 0 )
                    same = false;
                std::memcpy( target, slot->m_bits, static_cast< std::size_t >( bytes ) );
                ++fields;
            };
            if ( same )
            {
                take( "Screen Scale", ours.m_screen_scale, 8, s );
                take( "X", &ours.m_rect[ 0 ], 4, s );
                take( "Y", &ours.m_rect[ 1 ], 4, s );
                take( "Width", &ours.m_rect[ 2 ], 4, s );
                take( "Height", &ours.m_rect[ 3 ], 4, s );
                take( "Horizontal Alignment", &ours.m_h_align, 4, s );
                take( "Vertical Alignment", &ours.m_v_align, 4, s );
                take( "No Clip", &ours.m_no_clip, 1, s );
                take( "Single Line", &ours.m_single_line, 1, s );
                take( "Wordbreak", &ours.m_wordbreak, 1, s );
                take( "Horizontal Anchor", &ours.m_anchor.m_horizontal, 4, s );
                take( "Horizontal Ratio", &ours.m_anchor.m_h_ratio, 4, s );
                take( "Vertical Anchor", &ours.m_anchor.m_vertical, 4, s );
                take( "Vertical Ratio", &ours.m_anchor.m_v_ratio, 4, s );
                take( "Depth", &ours.m_depth, 4, draw );
            }
            if ( !same && ++failed <= 8 )
                LOG_ERROR( "ui text draw: %s entity %d (%s): our loaded fields differ from retail's", v.m_project, v.m_index, v.m_text );
            t = ours;
            ++real;
        }
        sort.m_index = e.m_index;
        sort.m_key_lo = e.m_key_lo;
        sort.m_key_hi = e.m_key_hi;
        std::vector< ui_gfx_command_t >& list = sort.m_commands[ e.m_index ];
        const std::size_t before = list.size( );
        const bool known = ui_text_draw( sort, env, crop, db, t, font, text, [ &v ] { float f; std::memcpy( &f, &v.m_alpha, 4 ); return f; }( ) );
        bool same = known && list.size( ) - before == static_cast< std::size_t >( v.m_commands );
        for ( int c = 0; same && c < v.m_commands; ++c )
        {
            const golden_txt_command_t& want = g_golden_txt_commands[ v.m_first + c ];
            const ui_gfx_command_t& have = list[ before + static_cast< std::size_t >( c ) ];
            const std::vector< std::uint8_t > record = ui_unhex( want.m_record );
            const auto& memory = sort.m_buffers[ e.m_index ].m_memory;
            same = std::memcmp( have.m_words, want.m_words, sizeof( have.m_words ) ) == 0 && have.m_words[ 3 ] + record.size( ) <= memory.size( ) &&
                   std::memcmp( memory.data( ) + have.m_words[ 3 ], record.data( ), record.size( ) ) == 0;
            ++commands;
        }
        if ( !same && ++failed <= 8 )
            LOG_ERROR( "ui text draw: vector %d (sweep %d, '%s') gave %d commands, retail %d - or a word / record byte differs", static_cast< int >( k ),
                v.m_sweep, v.m_project, static_cast< int >( list.size( ) - before ), v.m_commands );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "ui_text_draw matches retail's VuUITextBaseEntity draw 0x627C60 (anchor 0x6282D0, flags 0x5546B0, font draw 0x536C10, "
                     "VuGfxSort 0x426B80 / 0x429010) on %d vectors (%d real entities, %d of their loaded fields): %d commands and records byte for byte",
            static_cast< int >( g_golden_txt_vectors_count ), real, fields, commands );
    else
        LOG_ERROR( "the UI text draw FAILED %d checks against retail's", failed );
    return failed == 0;
}

// The UI wiring end to end against retail (oracle case ui_wiring): the real
// Screens projects loaded through ui_scripted_world (plugs from game/plugs.hxx,
// links by script_load), every output plug whose links all reach a screen
// action found the same way retail's case finds them (owner pre-order index,
// plug index, name, return type, each link's owner + plug), fired through
// c_ui_wiring::fire (0x5023A0) - the VuRetVal and every broadcast (hash, params
// bytes); then each fire's broadcasts into ONE c_front_end in order, its whole
// state and hooks after each fire.
inline bool verify_ui_wiring( )
{
    const ui_loaded_t& base = ui_loaded( );
    std::vector< std::string > names( g_golden_entity_templates, g_golden_entity_templates + g_golden_entity_templates_count );
    static const entity_registry_t registry = build_entity_registry( names );
    const std::map< std::string, value_t >* templates = &base.templates;
    const entity_world_t world = ui_scripted_world( registry, [ templates ]( const std::string& name ) -> const value_t*
    {
        const auto found = templates->find( "VuTemplateAsset/" + name );
        return found == templates->end( ) ? nullptr : &found->second;
    } );
    const unsigned* cdb = g_golden_vujb_words + g_golden_vujb_offsets[ 0 ];
    const unsigned char* cdb_raw = reinterpret_cast< const unsigned char* >( cdb + 2 );
    const value_t constant_db = c_reader( std::vector< std::uint8_t >( cdb_raw, cdb_raw + cdb[ 1 ] ) ).parse( );
    std::vector< std::pair< std::string, std::string > > fe_log;
    front_end_engine_t engine;
    engine.m_display = []( int& w, int& h ) { w = 1280; h = 720; };
    engine.m_sound = [ &fe_log ]( const char* name ) { fe_log.push_back( { "sound", name } ); };
    engine.m_load_project = [ &fe_log ]( const std::string& name ) { fe_log.push_back( { "load", name } ); return 0; };
    engine.m_analytics = [ &fe_log ]( const char* event, const std::string& ) { fe_log.push_back( { "analytics", event } ); };
    c_front_end fe( engine, constant_db );
    int failed = 0;
    auto bad = [ &failed ]( const char* what, int k )
    {
        if ( ++failed <= 8 )
            LOG_ERROR( "ui wiring: %s differs from retail (fire %d)", what, k );
    };
    auto same_state = [ &fe, &engine ]( const golden_wire_state_t& st ) -> bool
    {
        if ( fe.m_mode != st.m_mode || fe.m_next != st.m_next || fe.m_current != st.m_current )
            return false;
        if ( fe.m_stack.size( ) != static_cast< std::size_t >( st.m_stack ) )
            return false;
        for ( int k = 0; k < st.m_stack; ++k )
            if ( fe.m_stack[ k ] != g_golden_wire_stack[ st.m_first_stack + k ] )
                return false;
        if ( static_cast< unsigned >( fe.m_depth ) != st.m_depth || engine.m_operation != st.m_operation || engine.m_screen != st.m_screen )
            return false;
        if ( ( fe.m_fsm.current( ) != nullptr ) != ( st.m_state != nullptr ) || ( st.m_state && fe.m_fsm.current( )->m_name != st.m_state ) )
            return false;
        std::uint32_t time = 0;
        std::memcpy( &time, &fe.m_fsm.m_time, 4 );
        if ( time != st.m_time || fe.m_fsm.m_conditions.size( ) != static_cast< std::size_t >( st.m_condition_count ) )
            return false;
        for ( int k = 0; k < st.m_condition_count; ++k )
            if ( static_cast< unsigned >( fe.m_fsm.m_conditions[ k ].m_value | ( fe.m_fsm.m_conditions[ k ].m_pulse << 8 ) ) != st.m_conditions[ k ] )
                return false;
        return true;
    };
    if ( !same_state( g_golden_wire_initial ) )
        bad( "the front end's initial state", -1 );
    std::vector< std::pair< std::uint32_t, ui_params_t > > sent;
    c_ui_wiring wiring;
    wiring.m_broadcast = [ &sent ]( std::uint32_t hash, const ui_params_t& p ) { sent.push_back( { hash, p } ); };
    std::size_t next = 0;
    int links = 0;
    for ( std::size_t w = 0; w < g_golden_wire_projects_count; ++w )
    {
        const golden_ui_project_t* project = nullptr;
        for ( std::size_t p = 0; p < g_golden_ui_projects_count; ++p )
            if ( std::strcmp( g_golden_ui_projects[ p ].m_asset, g_golden_wire_projects[ w ] ) == 0 )
                project = &g_golden_ui_projects[ p ];
        if ( !project )
        {
            bad( "a project's presence in the loaded set", -1 );
            continue;
        }
        const unsigned char* raw = reinterpret_cast< const unsigned char* >( project->m_words );
        const value_t document = c_reader( std::vector< std::uint8_t >( raw, raw + project->m_bytes ) ).parse( );
        const std::unique_ptr< entity_t > root = entity_load_project( member( document, "VuProject" ), project->m_path, world );
        std::vector< entity_t* > order;
        const auto walk = [ &order ]( const auto& self, entity_t& e ) -> void
        {
            order.push_back( &e );
            for ( auto& child : e.m_children )
                self( self, *child );
        };
        if ( root )
            walk( walk, *root );
        // the case's selection: an output plug, links non-empty, every link's owner a screen action
        std::vector< std::pair< int, int > > found;
        for ( std::size_t i = 0; i < order.size( ); ++i )
        {
            const script_component_t* sc = script_of( *order[ i ] );
            for ( std::size_t k = 0; sc && k < sc->m_plugs.size( ); ++k )
            {
                const script_plug_t& p = sc->m_plugs[ k ];
                if ( p.m_row->m_input || p.m_links.empty( ) )
                    continue;
                bool all = true;
                ui_action_kind_t kind;
                for ( const script_link_t& l : p.m_links )
                    all = all && ui_action_kind_of( l.m_entity->m_type, kind );
                if ( all )
                    found.push_back( { static_cast< int >( i ), static_cast< int >( k ) } );
            }
        }
        std::size_t first = next;
        while ( next < g_golden_wire_fires_count && std::strcmp( g_golden_wire_fires[ next ].m_project, g_golden_wire_projects[ w ] ) == 0 )
            ++next;
        if ( found.size( ) != next - first )
        {
            LOG_ERROR( "ui wiring: %s has %d wired output plugs, retail %d", g_golden_wire_projects[ w ], static_cast< int >( found.size( ) ),
                static_cast< int >( next - first ) );
            bad( "the wired plugs' count", static_cast< int >( first ) );
            continue;
        }
        for ( std::size_t f = first; f < next; ++f )
        {
            const golden_wire_fire_t& want = g_golden_wire_fires[ f ];
            const int row = static_cast< int >( f );
            const auto [ index, plug ] = found[ f - first ];
            entity_t& owner = *order[ static_cast< std::size_t >( index ) ];
            const script_plug_t& p = script_of( owner )->m_plugs[ static_cast< std::size_t >( plug ) ];
            if ( index != want.m_index || plug != want.m_plug_index || owner.m_name != want.m_name || std::strcmp( p.m_row->m_name, want.m_plug ) != 0 ||
                 p.m_row->m_type != want.m_type )
            {
                bad( "the fired plug", row );
                continue;
            }
            bool same = p.m_links.size( ) == static_cast< std::size_t >( want.m_links );
            for ( int k = 0; same && k < want.m_links; ++k )
            {
                const script_link_t& l = p.m_links[ static_cast< std::size_t >( k ) ];
                const golden_wire_link_t& gl = g_golden_wire_links[ want.m_first_link + k ];
                const auto at = std::find( order.begin( ), order.end( ), l.m_entity );
                same = at - order.begin( ) == gl.m_index && std::strcmp( script_of( *l.m_entity )->m_plugs[ static_cast< std::size_t >( l.m_plug ) ].m_row->m_name, gl.m_plug ) == 0;
            }
            if ( !same )
                bad( "the resolved links", row );
            links += want.m_links;
            sent.clear( );
            const ui_ret_t ret = wiring.fire( owner, plug, ui_params_t{ } );
            char text[ 20 ];
            std::snprintf( text, sizeof( text ), "%08X%08X", ret.m_words[ 0 ], ret.m_words[ 1 ] );
            if ( !ret.m_known || std::strcmp( text, want.m_ret ) != 0 )
                bad( "the VuRetVal", row );
            same = sent.size( ) == static_cast< std::size_t >( want.m_logs );
            for ( int k = 0; same && k < want.m_logs; ++k )
                same = std::strcmp( g_golden_wire_logs[ want.m_first_log + k ].m_kind, "broadcast" ) == 0 &&
                       sent[ static_cast< std::size_t >( k ) ].first == g_golden_wire_logs[ want.m_first_log + k ].m_hash &&
                       ui_hex( sent[ static_cast< std::size_t >( k ) ].second ) == g_golden_wire_logs[ want.m_first_log + k ].m_params;
            if ( !same )
                bad( "the broadcasts", row );
            fe_log.clear( );
            for ( const auto& b : sent )
                fe.handle( b.first, b.second );
            same = fe_log.size( ) == static_cast< std::size_t >( want.m_fe_logs );
            for ( int k = 0; same && k < want.m_fe_logs; ++k )
                same = fe_log[ static_cast< std::size_t >( k ) ].first == g_golden_wire_fe_logs[ want.m_first_fe_log + k ].m_kind &&
                       fe_log[ static_cast< std::size_t >( k ) ].second == g_golden_wire_fe_logs[ want.m_first_fe_log + k ].m_a;
            if ( !same )
                bad( "the front end's hooks", row );
            if ( !same_state( want.m_front_end ) )
                bad( "the front end's state", row );
        }
    }
    if ( next != g_golden_wire_fires_count )
        bad( "the fires' project order", static_cast< int >( next ) );
    if ( wiring.m_unknown != 0 )
        bad( "an input without a recreated handler", -1 );
    if ( failed == 0 )
        LOG_SUCCESS( "the UI wiring matches retail on %d Screens projects (ui_scripted_world: plugs.hxx + script_load; "
                     "c_ui_wiring::fire = 0x5023A0 -> Trigger 0x472800 -> the actions' slot 12 -> c_front_end): %d wired output plugs, "
                     "%d links, VuRetVal, broadcasts hash + params, the front end's state + hooks after each fire",
            static_cast< int >( g_golden_wire_projects_count ), static_cast< int >( g_golden_wire_fires_count ), links );
    else
        LOG_ERROR( "the UI wiring FAILED %d checks against retail's", failed );
    return failed == 0;
}

// ui_texture_draw against retail's texture draw 0x507360 (oracle case
// texture_draw): one sort through every vector in order - the commands' words
// and the 104-byte records byte for byte.
inline bool verify_ui_texture_draw( )
{
    ui_gfx_sort_t sort;
    sort.m_buffers[ 0 ].m_capacity = 0x300000;
    sort.m_buffers[ 1 ].m_used = 0x35;
    sort.m_buffers[ 1 ].m_capacity = 0x100000;
    for ( auto& b : sort.m_buffers )
        b.m_memory.resize( static_cast< std::size_t >( b.m_capacity ) );
    int failed = 0, commands = 0;
    for ( std::size_t k = 0; k < g_golden_tex_vectors_count; ++k )
    {
        const golden_tex_vector_t& v = g_golden_tex_vectors[ k ];
        const golden_txt_env_t& e = g_golden_txt_envs[ v.m_env ];
        ui_font_draw_env_t env;
        const std::vector< std::uint8_t > matrix = ui_unhex( v.m_matrix );
        std::memcpy( env.m_matrix, matrix.data( ), 64 );
        env.m_texture_material = g_golden_tex_material;
        env.m_texture_material_key = g_golden_tex_material_key;
        float a[ 4 ], b[ 4 ], depth;
        std::memcpy( a, v.m_a, 16 );
        std::memcpy( b, v.m_b, 16 );
        std::memcpy( &depth, &v.m_depth, 4 );
        sort.m_index = e.m_index;
        sort.m_key_lo = e.m_key_lo;
        sort.m_key_hi = e.m_key_hi;
        std::vector< ui_gfx_command_t >& list = sort.m_commands[ e.m_index ];
        const std::size_t before = list.size( );
        ui_texture_draw( sort, env, v.m_texture, v.m_colour, a, b, depth );
        bool same = list.size( ) - before == static_cast< std::size_t >( v.m_commands );
        for ( int c = 0; same && c < v.m_commands; ++c )
        {
            const golden_txt_command_t& want = g_golden_tex_commands[ v.m_first + c ];
            const ui_gfx_command_t& have = list[ before + static_cast< std::size_t >( c ) ];
            const std::vector< std::uint8_t > record = ui_unhex( want.m_record );
            const auto& memory = sort.m_buffers[ e.m_index ].m_memory;
            same = std::memcmp( have.m_words, want.m_words, sizeof( have.m_words ) ) == 0 && have.m_words[ 3 ] + record.size( ) <= memory.size( ) &&
                   std::memcmp( memory.data( ) + have.m_words[ 3 ], record.data( ), record.size( ) ) == 0;
            ++commands;
        }
        if ( !same && ++failed <= 8 )
            LOG_ERROR( "ui texture draw: vector %d gave %d commands, retail %d - or a word / record byte differs", static_cast< int >( k ),
                static_cast< int >( list.size( ) - before ), v.m_commands );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "ui_texture_draw matches retail's texture draw 0x507360 (VuGfxSort 0x426B80 / 0x429010) on %d vectors: %d commands and "
                     "104-byte records byte for byte",
            static_cast< int >( g_golden_tex_vectors_count ), commands );
    else
        LOG_ERROR( "the UI texture draw FAILED %d checks against retail's", failed );
    return failed == 0;
}

// ui_hud_draw (core/ui compose.hxx) against retail's VuHUDEntity draw 0x423A70
// (0x50B4B0, 0x50B530, 0x628070; oracle case hud_draw) on the root of every
// loaded project (OUR trees: which children have a UI draw component, and which
// one comes first) under three environments: every child draw reached in order
// (child, component, alpha bits, both stacks' tops and sizes, the sort key),
// then the sizes and the key after.
inline bool verify_ui_hud_draw( )
{
    const ui_loaded_t& loaded = ui_loaded( );
    int failed = 0, calls = 0;
    for ( std::size_t r = 0; r < g_golden_hud_runs_count; ++r )
    {
        const golden_hud_run_t& run = g_golden_hud_runs[ r ];
        const golden_hud_env_t& he = g_golden_hud_envs[ run.m_env ];
        const golden_txt_env_t& e = g_golden_txt_envs[ he.m_txt ];
        const entity_t* root = nullptr;
        for ( std::size_t p = 0; p < g_golden_ui_projects_count && !root; ++p )
            if ( std::strcmp( g_golden_ui_projects[ p ].m_asset, run.m_project ) == 0 )
                root = loaded.roots[ p ].get( );
        if ( !root || root->m_children.size( ) != static_cast< std::size_t >( run.m_children ) )
        {
            if ( ++failed <= 8 )
                LOG_ERROR( "ui hud draw: %s has no loaded root or a different child count", run.m_project );
            continue;
        }
        ui_gfx_sort_t sort;
        sort.m_index = e.m_index;
        sort.m_key_lo = e.m_key_lo;
        sort.m_key_hi = he.m_key_hi;
        ui_gfx_stacks_t stacks;
        stacks.m_matrices.resize( static_cast< std::size_t >( run.m_matrices_before ) );
        stacks.m_floats.resize( static_cast< std::size_t >( run.m_floats_before ) );
        ui_impl_t impl;
        const std::vector< std::uint8_t > crop = ui_unhex( run.m_crop );
        std::memcpy( impl.m_crop.m_matrix, crop.data( ), 64 );
        std::memcpy( &impl.m_float, &he.m_float, 4 );
        float alpha;
        std::memcpy( &alpha, &he.m_alpha, 4 );
        bool same = true;
        int at = 0;
        ui_hud_draw( sort, stacks, impl, *root, alpha, [ & ]( const entity_t& child, int component, float a )
        {
            if ( at >= run.m_calls )
            {
                same = false;
                return;
            }
            const golden_hud_call_t& want = g_golden_hud_calls[ run.m_first + at++ ];
            int index = -1;
            for ( std::size_t k = 0; k < root->m_children.size( ); ++k )
                if ( root->m_children[ k ].get( ) == &child )
                    index = static_cast< int >( k );
            std::uint32_t bits, top;
            std::memcpy( &bits, &a, 4 );
            std::memcpy( &top, &stacks.m_floats.back( ), 4 );
            const std::vector< std::uint8_t > matrix = ui_unhex( want.m_matrix );
            same = same && index == want.m_child && component == want.m_component && bits == want.m_alpha &&
                   stacks.m_matrices.size( ) == static_cast< std::size_t >( want.m_matrices ) &&
                   std::memcmp( stacks.m_matrices.back( ).data( ), matrix.data( ), 64 ) == 0 &&
                   stacks.m_floats.size( ) == static_cast< std::size_t >( want.m_floats ) && top == want.m_float &&
                   sort.m_key_lo == want.m_key[ 0 ] && sort.m_key_hi == want.m_key[ 1 ];
            ++calls;
        } );
        same = same && at == run.m_calls && stacks.m_matrices.size( ) == static_cast< std::size_t >( run.m_matrices_after ) &&
               stacks.m_floats.size( ) == static_cast< std::size_t >( run.m_floats_after ) && sort.m_key_lo == run.m_key[ 0 ] && sort.m_key_hi == run.m_key[ 1 ];
        if ( !same && ++failed <= 8 )
            LOG_ERROR( "ui hud draw: %s env %d reached %d child draws, retail %d - or a call / stack / key differs", run.m_project, run.m_env, at, run.m_calls );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "ui_hud_draw matches retail's VuHUDEntity draw 0x423A70 (0x50B4B0, 0x50B530, 0x628070) on %d runs over the loaded projects' "
                     "roots: %d child draws - child, component, alpha, both stacks' tops and sizes, the sort key - and the state after",
            static_cast< int >( g_golden_hud_runs_count ), calls );
    else
        LOG_ERROR( "the HUD draw FAILED %d checks against retail's", failed );
    return failed == 0;
}

// ui_speedometer_draw / ui_speedometer_layout_draw (core/ui gauge.hxx) against
// retail's VuHUDSpeedometerEntity draws 0x42BA00 / 0x42C1E0 (+ the rect draw
// 0x507A00; oracle case speedometer_draw): one sort through every vector in
// order - the commands' words and records byte for byte. For the real
// speedometers the property-backed fields come from OUR loaded tree and are
// compared with retail's bytes first; +0xE0 (OnUITick's), the textures and the
// material are the case's stand-ins, the boat and the display size inputs.
inline bool verify_ui_speedometer_draw( )
{
    const ui_loaded_t& loaded = ui_loaded( );
    ui_gfx_sort_t sort;
    sort.m_buffers[ 0 ].m_capacity = 0x300000;
    sort.m_buffers[ 1 ].m_used = 0x35;
    sort.m_buffers[ 1 ].m_capacity = 0x100000;
    for ( auto& b : sort.m_buffers )
        b.m_memory.resize( static_cast< std::size_t >( b.m_capacity ) );
    const auto texture = []( std::uint32_t slot )
    {
        ui_texture_t t;
        for ( const golden_spd_texture_t& g : g_golden_spd_textures )
            if ( g.m_slot == slot )
                t = { g.m_handle, g.m_width, g.m_height };
        return t;
    };
    int failed = 0, commands = 0, fields = 0, real = 0;
    for ( std::size_t k = 0; k < g_golden_spd_vectors_count; ++k )
    {
        const golden_spd_vector_t& v = g_golden_spd_vectors[ k ];
        const golden_txt_env_t& e = g_golden_txt_envs[ v.m_env ];
        ui_crop_t crop;
        std::memcpy( &crop.m_matrix[ 0 ], &e.m_crop[ 0 ], 4 );
        std::memcpy( &crop.m_matrix[ 5 ], &e.m_crop[ 1 ], 4 );
        std::memcpy( &crop.m_matrix[ 12 ], &e.m_crop[ 2 ], 4 );
        std::memcpy( &crop.m_matrix[ 13 ], &e.m_crop[ 3 ], 4 );
        ui_font_draw_env_t env;
        const std::vector< std::uint8_t > matrix = ui_unhex( v.m_matrix );
        std::memcpy( env.m_matrix, matrix.data( ), 64 );
        env.m_texture_material = g_golden_spd_materials[ 0 ][ 0 ];
        env.m_texture_material_key = g_golden_spd_materials[ 0 ][ 1 ];
        env.m_rect_material = g_golden_spd_materials[ 1 ][ 0 ];
        env.m_rect_material_key = g_golden_spd_materials[ 1 ][ 1 ];
        const std::vector< std::uint8_t > b = ui_unhex( v.m_fields );
        const auto at = [ &b ]( int offset ) { return b.data( ) + ( offset - 0x60 ); };
        ui_speedometer_t s;
        std::memcpy( s.m_screen_scale, at( 0x60 ), 8 );
        std::memcpy( s.m_rect, at( 0x68 ), 16 );
        std::memcpy( &s.m_anchor.m_horizontal, at( 0x78 ), 4 );
        std::memcpy( &s.m_anchor.m_vertical, at( 0x7C ), 4 );
        std::memcpy( &s.m_anchor.m_h_ratio, at( 0x80 ), 4 );
        std::memcpy( &s.m_anchor.m_v_ratio, at( 0x84 ), 4 );
        std::memcpy( &s.m_metric, at( 0xE0 ), 4 );
        std::memcpy( &s.m_depth, &v.m_depth, 4 );
        s.m_material = g_golden_spd_materials[ 2 ][ 0 ];
        s.m_material_key = g_golden_spd_materials[ 2 ][ 1 ];
        s.m_back_imperial = texture( 0xA0 );
        s.m_back_metric = texture( 0xA4 );
        s.m_needle = texture( 0xA8 );
        for ( int d = 0; d < 8; ++d )
            s.m_boost[ d ] = texture( 0xC0u + 4u * static_cast< std::uint32_t >( d ) );
        s.m_self = v.m_entity;
        if ( v.m_sweep == 0 )
        {
            // our loaded entity: the same project and pre-order index
            const entity_t* have = nullptr;
            for ( std::size_t p = 0; p < g_golden_ui_projects_count && !have; ++p )
                if ( std::strcmp( g_golden_ui_projects[ p ].m_asset, v.m_project ) == 0 && v.m_index < static_cast< int >( loaded.trees[ p ].size( ) ) )
                    have = loaded.trees[ p ][ static_cast< std::size_t >( v.m_index ) ].second;
            const ui_state_t* st = have && ui_is_state( *have ) ? static_cast< const ui_state_t* >( have->m_state.get( ) ) : nullptr;
            const ui_state_t* draw = nullptr;
            for ( const entity_component_t& c : have ? have->m_components : std::vector< entity_component_t >( ) )
                if ( c.m_vtable == 0x738A44u && c.m_state )
                    draw = static_cast< const ui_state_t* >( c.m_state.get( ) );
            bool same = st && draw;
            const auto take = [ & ]( const char* name, void* target, int bytes, const ui_state_t* from )
            {
                const ui_slot_t* slot = from ? ui_slot_named( *from, name ) : nullptr;
                if ( !slot )
                {
                    same = false;
                    return;
                }
                if ( std::memcmp( target, slot->m_bits, static_cast< std::size_t >( bytes ) ) != 0 )
                    same = false;
                std::memcpy( target, slot->m_bits, static_cast< std::size_t >( bytes ) );
                ++fields;
            };
            if ( same )
            {
                take( "Screen Scale", s.m_screen_scale, 8, st );
                take( "X", &s.m_rect[ 0 ], 4, st );
                take( "Y", &s.m_rect[ 1 ], 4, st );
                take( "Width", &s.m_rect[ 2 ], 4, st );
                take( "Height", &s.m_rect[ 3 ], 4, st );
                take( "Horizontal Anchor", &s.m_anchor.m_horizontal, 4, st );
                take( "Horizontal Ratio", &s.m_anchor.m_h_ratio, 4, st );
                take( "Vertical Anchor", &s.m_anchor.m_vertical, 4, st );
                take( "Vertical Ratio", &s.m_anchor.m_v_ratio, 4, st );
                take( "Depth", &s.m_depth, 4, draw );
            }
            if ( !same && ++failed <= 8 )
                LOG_ERROR( "ui speedometer draw: %s entity %d: our loaded fields differ from retail's", v.m_project, v.m_index );
            ++real;
        }
        ui_speedometer_boat_t boat;
        boat.m_present = v.m_boat.m_present == 1;
        std::memcpy( boat.m_velocity, v.m_boat.m_velocity, 8 );
        boat.m_boost_count = v.m_boat.m_count;
        std::memcpy( &boat.m_digit_scale, &v.m_boat.m_scale, 4 );
        boat.m_byte_32c = static_cast< std::uint8_t >( v.m_boat.m_byte_32c );
        std::memcpy( &boat.m_boost, &v.m_boat.m_boost, 4 );
        boat.m_mode = v.m_boat.m_mode;
        boat.m_byte_32d = static_cast< std::uint8_t >( v.m_boat.m_byte_32d );
        sort.m_index = e.m_index;
        sort.m_key_lo = e.m_key_lo;
        sort.m_key_hi = e.m_key_hi;
        std::vector< ui_gfx_command_t >& list = sort.m_commands[ e.m_index ];
        const std::size_t before = list.size( );
        if ( v.m_fn == 0 )
            ui_speedometer_draw( sort, env, crop, s, boat, v.m_display );
        else
            ui_speedometer_layout_draw( sort, env, crop, s, boat, v.m_display, v.m_fn == 1 );
        bool same = list.size( ) - before == static_cast< std::size_t >( v.m_commands );
        for ( int c = 0; same && c < v.m_commands; ++c )
        {
            const golden_txt_command_t& want = g_golden_spd_commands[ v.m_first + c ];
            const ui_gfx_command_t& have = list[ before + static_cast< std::size_t >( c ) ];
            const std::vector< std::uint8_t > record = ui_unhex( want.m_record );
            const auto& memory = sort.m_buffers[ e.m_index ].m_memory;
            same = std::memcmp( have.m_words, want.m_words, sizeof( have.m_words ) ) == 0 && have.m_words[ 3 ] + record.size( ) <= memory.size( ) &&
                   std::memcmp( memory.data( ) + have.m_words[ 3 ], record.data( ), record.size( ) ) == 0;
            ++commands;
        }
        if ( !same && ++failed <= 8 )
            LOG_ERROR( "ui speedometer draw: vector %d (sweep %d, fn %d) gave %d commands, retail %d - or a word / record byte differs", static_cast< int >( k ),
                v.m_sweep, v.m_fn, static_cast< int >( list.size( ) - before ), v.m_commands );
    }
    if ( failed == 0 )
        LOG_SUCCESS( "ui_speedometer_draw matches retail's VuHUDSpeedometerEntity draws 0x42BA00 / 0x42C1E0 (rect draw 0x507A00, texture draw "
                     "0x507360, anchor 0x6282D0, 0x40B5F0 / 0x410AB0 / 0x407DB0 / 0x4078C0) on %d vectors (%d on real speedometers, %d of their "
                     "loaded fields): %d commands and records byte for byte",
            static_cast< int >( g_golden_spd_vectors_count ), real, fields, commands );
    else
        LOG_ERROR( "the speedometer draw FAILED %d checks against retail's", failed );
    return failed == 0;
}

inline bool verify_ui_all( )
{
    bool ok = true;
    ok = verify_ui_speedometer_draw( ) && ok;
    ok = verify_ui_hud_draw( ) && ok;
    ok = verify_ui_texture_draw( ) && ok;
    ok = verify_ui_wiring( ) && ok;
    ok = verify_ui_text_draw( ) && ok;    ok = verify_ui_fade( ) && ok;
    ok = verify_ui_hud_speedometer( ) && ok;
    ok = verify_ui_hud_element( ) && ok;
    ok = verify_ui_actions( ) && ok;
    ok = verify_ui_front_end_enter( ) && ok;
    ok = verify_ui_screen_events( ) && ok;
    ok = verify_ui_button( ) && ok;
    ok = verify_ui_front_end( ) && ok;
    ok = verify_ui_projects( ) && ok;
    ok = verify_ui_properties( ) && ok;
    ok = verify_ui_components( ) && ok;
    return ok;
}