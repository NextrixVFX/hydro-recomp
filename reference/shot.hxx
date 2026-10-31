// NOT COMPILED. Not in hydro.vcxproj, not in the unity build.
//
// The front-end camera. read_shot and find_camera read a real authored
// VuSetFrontEndCameraEntity out of a screen project and are 1:1; load_shot,
// present and aim_of are the menu-to-menu blend, which was INFERRED - the
// smoothstep, the pitch clamps at 0.04 / -0.12 and the 1.6s fallback duration
// were all invented and none of them is retail's.
//
// Banked together because they are one path. Keep read_shot / find_camera when
// the front end is built; the blend must be re-derived from the real
// VuGameModeManagerImpl screen transition, not reused.

    void load_shot( const char* screen )
    {
        std::vector< std::uint8_t > project;
        const std::string name = std::string( "VuProjectAsset/Screens/" ) + screen;
        if ( !m_archive.read( name.c_str( ), project ) )
            return;
        shot_t shot{};
        shot.eye[ 0 ] = m_eye[ 0 ];
        shot.eye[ 1 ] = m_eye[ 1 ];
        shot.eye[ 2 ] = m_eye[ 2 ];
        shot.target[ 0 ] = m_target[ 0 ];
        shot.target[ 1 ] = m_target[ 1 ];
        shot.target[ 2 ] = m_target[ 2 ];
        shot.plane = m_far;
        shot.time = 1.6f;
        const value_t root = reader_t( project ).parse( );
        const value_t* project_node = find( root, "VuProject" );
        const value_t* entity = project_node != nullptr ? find( *project_node, "RootEntity" ) : nullptr;
        if ( entity == nullptr || !find_camera( *entity, shot ) )
            return;
        m_shots.emplace( screen, shot );
        LOG_SUCCESS(
            "camera %s  %.1f %.1f %.1f -> %.1f %.1f %.1f  %.1fs",
            screen, shot.eye[ 0 ], shot.eye[ 1 ], shot.eye[ 2 ], shot.target[ 0 ], shot.target[ 1 ], shot.target[ 2 ], shot.time
        );
    }

    void present( const char* screen, float dt )
    {
        if ( screen == nullptr )
            screen = "Start";
        const auto it = m_shots.find( screen );
        if ( it != m_shots.end( ) )
        {
        const shot_t& shot = it->second;
        if ( m_shot != screen )
        {
            std::memcpy( m_from_eye, m_eye, sizeof( m_eye ) );
            std::memcpy( m_from_target, m_target, sizeof( m_target ) );
            aim_of( m_from_eye, m_from_target, m_from_yaw, m_from_pitch, m_from_dist );
            aim_of( shot.eye, shot.target, m_to_yaw, m_to_pitch, m_to_dist );
            m_yaw_delta = m_to_yaw - m_from_yaw;
            if ( m_yaw_delta > 3.14159265f )
                m_yaw_delta -= 6.2831853f;
            if ( m_yaw_delta < -3.14159265f )
                m_yaw_delta += 6.2831853f;
            m_from_far = m_far;
            m_shot = screen;
            m_blend = 0.0f;
            m_duration = shot.time > 0.0f ? shot.time : 1.6f;
        }

        float span = m_blend / m_duration;
        if ( span > 1.0f )
            span = 1.0f;
        const float curved = span * span * ( 3.0f - 2.0f * span );
        for ( int index = 0; index < 2; ++index )
            m_eye[ index ] = m_from_eye[ index ] + ( shot.eye[ index ] - m_from_eye[ index ] ) * curved;
        m_eye[ 2 ] = m_from_eye[ 2 ] + ( shot.eye[ 2 ] - m_from_eye[ 2 ] ) * curved;
        const float yaw = m_from_yaw + m_yaw_delta * curved;
        // Both menu eyes sit at the same height. A pitch blend between them
        // noses the view up through the middle of the turn, so the pitch
        // stays on the horizon and only the yaw changes.
        float pitch = m_from_pitch + ( m_to_pitch - m_from_pitch ) * curved;
        if ( pitch > 0.04f )
            pitch = 0.04f;
        if ( pitch < -0.12f )
            pitch = -0.12f;
        const float distance = m_from_dist + ( m_to_dist - m_from_dist ) * curved;
        const float horizontal = std::cos( pitch ) * distance;
        m_target[ 0 ] = m_eye[ 0 ] + horizontal * std::cos( yaw );
        m_target[ 1 ] = m_eye[ 1 ] + horizontal * std::sin( yaw );
        m_target[ 2 ] = m_eye[ 2 ] + std::sin( pitch ) * distance;
        if ( shot.plane > 1.0f )
            m_far = m_from_far + ( shot.plane - m_from_far ) * curved;
        m_blend += dt;
        }
        const double ocean_start = trace::now( );
        m_ocean.advance( dt );
        trace::frame( ).ocean += trace::now( ) - ocean_start;
        drive_traffic( dt );
        displace_water( );
        draw_traffic( dt );
        const double props_start = trace::now( );
        skin_props( dt );
        trace::frame( ).props += trace::now( ) - props_start;
        if ( m_boat_ready )
            c_engine::lift( 4, m_ocean.height( m_boat_x, m_boat_y ) );
        const double pfx_start = trace::now( );
        m_pfx.draw( dt, m_eye, m_target, [ this ]( const std::string& path ) { return texture_for( path ); },
            [ this ]( const std::string& path ) -> const std::vector< pfx_part_t >& { return pfx_parts( path ); } );
        m_coronas.tick( dt );
        m_coronas.draw( m_eye, m_target, [ this ]( const std::string& path ) { return texture_for( path ); } );
        trace::frame( ).pfx += trace::now( ) - pfx_start;
    }

    static void aim_of( const float* eye, const float* target, float& yaw, float& pitch, float& distance )
    {
        const float dx = target[ 0 ] - eye[ 0 ];
        const float dy = target[ 1 ] - eye[ 1 ];
        const float dz = target[ 2 ] - eye[ 2 ];
        const float horizontal = std::sqrt( dx * dx + dy * dy );
        distance = std::sqrt( horizontal * horizontal + dz * dz );
        if ( distance < 0.001f )
            distance = 0.001f;
        yaw = std::atan2( dy, dx );
        pitch = std::atan2( dz, horizontal );
    }

    // read_shot stayed in world.hxx - the entity walk still reads an authored
    // VuSetFrontEndCameraEntity, and it is the one 1:1 piece of this path with
    // a live caller.

    bool find_camera( const value_t& entity, shot_t& shot ) const
    {
        const value_t* type = find( entity, "type" );
        const value_t* data = find( entity, "data" );
        if ( type != nullptr && type->kind == value_t::kind_t::text && type->text.size( ) > 1 && type->text[ 0 ] == '#' )
        {
            std::vector< std::uint8_t > bytes;
            const std::string asset = std::string( "VuTemplateAsset/" ) + type->text.substr( 1 );
            if ( m_archive.read( asset.c_str( ), bytes ) )
            {
                const value_t parsed = reader_t( bytes ).parse( );
                const value_t* project = find( parsed, "VuProject" );
                const value_t* root = project != nullptr ? find( *project, "RootEntity" ) : nullptr;
                if ( root != nullptr && find_camera( *root, shot ) )
                    return true;
            }
        }
        if ( type != nullptr && type->kind == value_t::kind_t::text && type->text == "VuSetFrontEndCameraEntity" && data != nullptr )
        {
            const value_t* properties = find( *data, "Properties" );
            read_shot( properties, shot.eye, shot.target, shot.plane );
            if ( properties != nullptr && find( *properties, "Interpolation Time" ) != nullptr )
                shot.time = number( find( *properties, "Interpolation Time" ), shot.time );
            return true;
        }
        const value_t* children = data != nullptr ? find( *data, "ChildEntities" ) : nullptr;
        if ( children != nullptr && children->kind == value_t::kind_t::list )
        {
            for ( const value_t& child : children->list )
            {
                if ( find_camera( child, shot ) )
                    return true;
            }
        }
        return false;
    }


    struct shot_t
    {
        float eye[ 3 ]{};
        float target[ 3 ]{};
        float plane = -1.0f;
        float time = 1.6f;
    };
