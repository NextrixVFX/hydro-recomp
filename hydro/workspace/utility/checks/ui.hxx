#pragma once

#include <workspace/core/ui/action.hxx>
#include <workspace/core/ui/entity.hxx>
#include <workspace/core/ui/button.hxx>
#include <workspace/core/ui/flow.hxx>
#include <workspace/core/ui/screen.hxx>

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

inline bool verify_ui_all( )
{
    bool ok = true;
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