// NOT COMPILED. Not in hydro.vcxproj, not in the unity build.
//
// Verified 1:1 code whose caller was deleted in the cull. VuBoatEntity has not
// been transliterated, so nothing here has anywhere to attach yet; it is banked
// verbatim rather than deleted so the boat work starts from recovered code
// instead of from scratch. Each function keeps its own retail annotations.
//
// The hull cluster: fluids-mesh spray mounts, the waterline edge clipper, the
// per-frame spray update, hull skinning with the 48-bone palette, the TurnLR
// blend set, and the hull shader pair.
//
// Reintroduce against the real VuBoatEntity, not against the rider_t struct
// these were written against - that struct carried invented handling defaults
// and was deleted with the rest of the inferred boat.

    void read_spray( rider_t& boat, const value_t* record, const char* list_name, const std::vector< std::uint8_t >* skinned ) const
    {
        const value_t* pfx = record != nullptr ? find( *record, "Pfx" ) : nullptr;
        const value_t* list = pfx != nullptr ? find( *pfx, list_name ) : nullptr;
        if ( list == nullptr || list->kind != value_t::kind_t::list )
            return;
        for ( const value_t& item : list->list )
        {
            if ( boat.spray_count >= 4 )
                break;
            const value_t* effect = find( item, "Effect" );
            if ( effect == nullptr || effect->kind != value_t::kind_t::text || effect->text.empty( ) )
                continue;
            float position[ 3 ]{};
            const value_t* bone = find( item, "Bone" );
            if ( bone != nullptr && bone->kind == value_t::kind_t::text && !bone->text.empty( ) )
            {
                if ( skinned == nullptr || !model_bone( *skinned, bone->text.c_str( ), position ) )
                    continue;
            }
            else
            {
                const value_t* pos = find( item, "PosOffset" );
                position[ 0 ] = number( pos != nullptr ? find( *pos, "X" ) : nullptr, 0.0f );
                position[ 1 ] = number( pos != nullptr ? find( *pos, "Y" ) : nullptr, 0.0f );
                position[ 2 ] = number( pos != nullptr ? find( *pos, "Z" ) : nullptr, 0.0f );
            }
            const int slot = boat.spray_count++;
            boat.spray_path[ slot ] = effect->text;
            boat.spray_pos[ slot ][ 0 ] = position[ 0 ];
            boat.spray_pos[ slot ][ 1 ] = position[ 1 ];
            boat.spray_pos[ slot ][ 2 ] = position[ 2 ];
        }
    }

    // VuFluidsMeshAsset, decoded:
    //
    //   u32              vertexCount
    //   vertexCount x 16 (x, y, z, 1.0f)
    //   u32              edgeCount
    //   edgeCount x 8    (u32 a, u32 b)      an EXPLICIT edge list
    //   u32              triangleCount       always 2 * edgeCount / 3
    //   ...              per-triangle payload
    //
    // Validated on all four hull classes: Banshee_pfx 49v/135e, Flatbottom
    // 28v/78e, Catamaran 80v/222e, DeepV 39v/111e, every index in range and
    // no isolated vertices. Flatbottom and DeepV hit Euler's 3V-6 exactly, so
    // those are closed surfaces; the other two have a boundary.
    //
    // The triangle section is deliberately NOT read: an edge walk does not
    // need faces, and the per-triangle payload's stride is still unknown.
    // The mesh is a low-poly waterline shell - Banshee_pfx spans z -0.39..0.41
    // against a 6024-vertex render hull - which is why the spray comes off it
    // rather than off the drawn geometry.
    void read_fluids( rider_t& boat, const value_t* record ) const
    {
        const value_t* pfx = record != nullptr ? find( *record, "Pfx" ) : nullptr;
        const value_t* hull = pfx != nullptr ? find( *pfx, "HullEffect" ) : nullptr;
        const value_t* mesh = hull != nullptr ? find( *hull, "Fluids Mesh Asset" ) : nullptr;
        if ( mesh == nullptr || mesh->kind != value_t::kind_t::text || mesh->text.empty( ) )
            return;
        std::vector< std::uint8_t > bytes;
        if ( !m_archive.read( ( "VuFluidsMeshAsset/" + mesh->text ).c_str( ), bytes ) || bytes.size( ) < 20 )
            return;
        std::uint32_t count = 0;
        std::memcpy( &count, bytes.data( ), sizeof( count ) );
        if ( count == 0 || count > 4096 || bytes.size( ) < 4u + count * 16u )
            return;

        boat.mist.clear( );
        boat.hull_edges.clear( );
        boat.mist.reserve( static_cast< std::size_t >( count ) * 3 );
        for ( std::uint32_t index = 0; index < count; ++index )
        {
            float vertex[ 4 ]{};
            std::memcpy( vertex, bytes.data( ) + 4 + index * 16, sizeof( vertex ) );
            // The fourth component is the homogeneous 1.0 on every vertex of
            // every hull mesh; a different value means the stride is wrong.
            if ( vertex[ 3 ] < 0.99f || vertex[ 3 ] > 1.01f )
            {
                boat.mist.clear( );
                LOG_WARNING( "fluids mesh %s: vertex %u has w = %.3f, not 1.0",
                    mesh->text.c_str( ), index, vertex[ 3 ] );
                return;
            }
            boat.mist.push_back( vertex[ 0 ] );
            boat.mist.push_back( vertex[ 1 ] );
            boat.mist.push_back( vertex[ 2 ] );
        }

        const std::size_t at = 4u + static_cast< std::size_t >( count ) * 16u;
        if ( at + 4 > bytes.size( ) )
            return;
        std::uint32_t edges = 0;
        std::memcpy( &edges, bytes.data( ) + at, sizeof( edges ) );
        if ( edges == 0 || edges > 8192 || at + 4 + static_cast< std::size_t >( edges ) * 8 > bytes.size( ) )
            return;
        boat.hull_edges.reserve( static_cast< std::size_t >( edges ) * 2 );
        for ( std::uint32_t index = 0; index < edges; ++index )
        {
            std::uint32_t pair[ 2 ]{};
            std::memcpy( pair, bytes.data( ) + at + 4 + index * 8, sizeof( pair ) );
            if ( pair[ 0 ] >= count || pair[ 1 ] >= count )
            {
                boat.hull_edges.clear( );
                LOG_WARNING( "fluids mesh %s: edge %u = (%u %u) exceeds %u vertices",
                    mesh->text.c_str( ), index, pair[ 0 ], pair[ 1 ], count );
                return;
            }
            boat.hull_edges.push_back( static_cast< unsigned short >( pair[ 0 ] ) );
            boat.hull_edges.push_back( static_cast< unsigned short >( pair[ 1 ] ) );
        }

        // Total edge length is the emitter's whole budget, so report it
        // alongside the counts: it is the figure the per-frame submerged
        // length has to be a sane fraction of.
        float total = 0.0f;
        for ( std::uint32_t index = 0; index < edges; ++index )
        {
            const float* pa = boat.mist.data( ) + static_cast< std::size_t >( boat.hull_edges[ index * 2 ] ) * 3;
            const float* pb = boat.mist.data( ) + static_cast< std::size_t >( boat.hull_edges[ index * 2 + 1 ] ) * 3;
            const float dx = pb[ 0 ] - pa[ 0 ];
            const float dy = pb[ 1 ] - pa[ 1 ];
            const float dz = pb[ 2 ] - pa[ 2 ];
            total += std::sqrt( dx * dx + dy * dy + dz * dz );
        }
        // Euler's 3V-6 is the edge count of a closed triangulated surface;
        // a mesh under it has a boundary. Both shapes occur.
        LOG_SUCCESS( "fluids mesh %-16s %3u verts, %3u edges (3V-6 = %d), total edge length %.2f",
            mesh->text.c_str( ), count, edges, 3 * static_cast< int >( count ) - 6, total );
    }

    // Walk the hull's fluids-mesh edges, keep the parts under water, and hand
    // them to the emitter as boat-local segments. The water height is the
    // authored surface plus the wake, the same pair that the trigger planes
    // need - m_ocean alone is the global ocean and reads 0 everywhere.
    void hull_waterline( rider_t& boat, const float* posed )
    {
        if ( boat.hull_edges.empty( ) || boat.mist.empty( ) )
            return;
        boat.wetted.clear( );
        const std::size_t pairs = boat.hull_edges.size( ) / 2;
        for ( std::size_t edge = 0; edge < pairs; ++edge )
        {
            const unsigned short a = boat.hull_edges[ edge * 2 ];
            const unsigned short b = boat.hull_edges[ edge * 2 + 1 ];
            const float* pa = boat.mist.data( ) + static_cast< std::size_t >( a ) * 3;
            const float* pb = boat.mist.data( ) + static_cast< std::size_t >( b ) * 3;

            float wa[ 3 ]{};
            float wb[ 3 ]{};
            transform_local( posed, pa, wa );
            transform_local( posed, pb, wb );
            const float depth_a = water_level( wa[ 0 ], wa[ 1 ] ) - wa[ 2 ];
            const float depth_b = water_level( wb[ 0 ], wb[ 1 ] ) - wb[ 2 ];
            const bool under_a = depth_a > 0.0f;
            const bool under_b = depth_b > 0.0f;
            if ( !under_a && !under_b )
                continue;

            // A partly submerged edge contributes only the wet part, so the
            // waterline moves along the hull as the boat pitches rather than
            // the edge switching on and off whole.
            float t0 = 0.0f;
            float t1 = 1.0f;
            if ( under_a != under_b )
            {
                const float span = depth_a - depth_b;
                const float cross = std::fabs( span ) > 1.0e-6f ? depth_a / span : 0.5f;
                if ( under_a )
                    t1 = cross;
                else
                    t0 = cross;
            }
            if ( t1 - t0 < 1.0e-4f )
                continue;
            for ( int end = 0; end < 2; ++end )
            {
                const float t = end == 0 ? t0 : t1;
                for ( int axis = 0; axis < 3; ++axis )
                    boat.wetted.push_back( pa[ axis ] + ( pb[ axis ] - pa[ axis ] ) * t );
            }
        }
        for ( int slot = 0; slot < boat.spray_count; ++slot )
        {
            m_pfx.waterline( boat.spray[ slot ], boat.wetted.data( ),
                static_cast< int >( boat.wetted.size( ) / 6 ) );
        }

        // Countable acceptance test, reported on a frame counter so that
        // "stopped moving" never masquerades as "stopped reporting". Compare
        // a run at rest against --drive: the submerged length should be a
        // sensible fraction of the hull's total edge length, and the spray
        // rate should rise with speed because the authored fountain blends
        // between its own Min/Max Speed.
        if ( boat.player && ++m_wet_ticks % 600 == 0 )
        {
            const int wet = static_cast< int >( boat.wetted.size( ) / 6 );
            const float length = boat.spray_count > 0 ? m_pfx.submerged( boat.spray[ 0 ] ) : 0.0f;
            LOG_INFO( "hull spray: %d of %u edges wet, submerged length %.2f, speed %.1f mph, live particles %d",
                wet, static_cast< unsigned >( pairs ), length,
                boat.speed * 2.2369363f, m_pfx.live( ) );
        }
    }

    // Mount every effect on the boat's final pose and refresh the submerged
    // waterline. Kept as its own step with no other dependencies because the
    // scripted --drive path returns early out of the physics, and an earlier
    // version left the spray inside that skipped region - so the harness
    // bypassed the exact code it existed to exercise and reported nothing at
    // speed. A test harness must not skip its subject.
    void update_spray( rider_t& boat )
    {
        float posed[ 16 ]{};
        pose_hull( posed, boat );
        const float top = boat.max_mph * 0.44704f;
        float rate = top > 0.1f ? boat.speed / top : 0.0f;
        if ( rate < 0.0f )
            rate = 0.0f;
        if ( rate > 1.0f )
            rate = 1.0f;
        for ( int slot = 0; slot < boat.spray_count; ++slot )
        {
            float mounted[ 16 ]{};
            place_spray( mounted, posed, boat.spray_pos[ slot ][ 0 ],
                boat.spray_pos[ slot ][ 1 ], boat.spray_pos[ slot ][ 2 ] );
            m_pfx.move( boat.spray[ slot ], mounted, rate, boat.speed );
        }
        hull_waterline( boat, posed );
    }

    void skin_hull( rider_t& boat, float dt )
    {
        const auto mesh = m_skinned.find( boat.model );
        const auto bones = m_skeletons.find( boat.model );
        const auto named = m_xform.find( boat.model );
        if ( mesh == m_skinned.end( ) || bones == m_skeletons.end( ) || named == m_xform.end( ) )
        {
            if ( !m_skin_checked )
            {
                m_skin_checked = true;
                LOG_WARNING( "skin %s: mesh %d skeleton %d xform %d",
                    boat.model.c_str( ),
                    mesh != m_skinned.end( ) ? 1 : 0,
                    bones != m_skeletons.end( ) ? 1 : 0,
                    named != m_xform.end( ) ? 1 : 0 );
            }
            return;
        }
        const auto found = m_clips.find( named->second );
        if ( found == m_clips.end( ) || !found->second.valid( ) )
            return;

        const c_clip& clip = found->second;
        const std::vector< model_bone_t >& bind = bones->second;
        const skinned_mesh_t& source = mesh->second;
        if ( clip.bones( ) != static_cast< int >( bind.size( ) ) )
            return;

        if ( boat.hull_geometry < 0 )
        {
            boat.hull_geometry = c_engine::geometry(
                source.bind.data( ), static_cast< int >( source.bind.size( ) ),
                source.indices.data( ), static_cast< int >( source.indices.size( ) ) );
            if ( boat.hull_geometry < 0 )
                return;
        }

        boat.clip_time += dt;
        const float rate = 30.0f;
        const int frame = static_cast< int >( boat.clip_time * rate );
        // c_clip::at wraps internally, so the clock can run forever. The
        // wrapped index is what the guards below need: the round-trip is only
        // meaningful on the clip's first frame, and that must come round
        // again every cycle rather than once in the first 33 ms.
        const int cycle = clip.frames( ) > 0 ? frame % clip.frames( ) : 0;

        // TurnLR is a SPARSE DELTA clip, measured rather than assumed. 39 of
        // its 41 bones hold an exact identity delta - zero translation,
        // rotation (0, 0, 0, 1), unit scale - and the only two that do not are
        // Vector_Moto_R and Vector_Moto_L, the outboard motors. Those two
        // carry position (0, 0, 0) on every one of the three frames and differ
        // solely by rotation.
        //
        // The zero translation is what proves these are deltas and not poses:
        // an absolute pose would carry the bind translation (0.648, -3.334,
        // 0.602), which is exactly what the body clip's frame 0 does carry for
        // the same bone. Frame 1 is the identity (centre) and frames 0 and 2
        // are +-(0, 0, 0.1736, 0.9848), that is 2*asin(0.17364) = 20 degrees
        // about Z. So the 39 bystanders compose as a harmless identity and
        // only the motors swivel.
        const c_clip* turn = nullptr;
        const auto turning = m_turn.find( boat.model );
        if ( turning != m_turn.end( ) )
        {
            const auto entry = m_clips.find( turning->second );
            if ( entry != m_clips.end( ) && entry->second.blend_set( )
                && entry->second.bones( ) == static_cast< int >( bind.size( ) ) )
                turn = &entry->second;
        }
        // Frame order follows the asset's own name, TurnLR - left, centre,
        // right - so positive steering takes the last frame. SUBSTITUTION:
        // the sign convention comes from the filename, not from reversed
        // code, so if the motors swing the wrong way the two extremes swap.
        // Everything else here is read from the data.
        const float steering = boat.turn > 1.0f ? 1.0f : ( boat.turn < -1.0f ? -1.0f : boat.turn );
        const int extreme = steering >= 0.0f ? ( turn != nullptr ? turn->frames( ) - 1 : 0 ) : 0;
        const float swing = std::fabs( steering );

        std::vector< float > palette;
        build_palette( palette, bind, clip, frame, turn, extreme, swing );
        mesh_vertex_t* target = skin_geometry( boat.hull_geometry, source, bind, palette );
        if ( target == nullptr )
            return;

        // Frame 0 must reproduce the bind pose exactly: the palette is then
        // bindWorld * inverse(bindWorld), the identity. Kept as a permanent
        // regression guard, because this is what caught model_invert() writing
        // the 3x3 adjugate transposed - a fault invisible on the 35 of 41
        // bones whose bind rotation is identity, which only doubled the angle
        // on the other six and so moved just 290 of 6024 vertices. Looking at
        // the render would never have found that.
        // Steering must be neutral as well, or the motor delta legitimately
        // moves vertices and the guard would cry wolf on a turning AI boat.
        if ( !m_skin_checked && cycle == 0 && std::fabs( steering ) < 0.001f )
        {
            m_skin_checked = true;
            int drift = 0;
            for ( std::size_t v = 0; v < source.bind.size( ); ++v )
            {
                const mesh_vertex_t& rest = source.bind[ v ];
                const mesh_vertex_t& now = target[ v ];
                const float dx = now.position[ 0 ] - rest.position[ 0 ];
                const float dy = now.position[ 1 ] - rest.position[ 1 ];
                const float dz = now.position[ 2 ] - rest.position[ 2 ];
                if ( dx * dx + dy * dy + dz * dz > 1.0e-4f )
                    ++drift;
            }
            if ( drift == 0 )
            {
                LOG_SUCCESS( "skin round-trip: all %u vertices reproduce the bind pose",
                    static_cast< unsigned >( source.bind.size( ) ) );
            }
            else
            {
                LOG_ERROR( "skin round-trip: %d of %u vertices drift from the bind pose",
                    drift, static_cast< unsigned >( source.bind.size( ) ) );
            }
        }

        // The round-trip proves frame 0 is right; it says nothing about the
        // clock. draw_traffic used to default dt to 0, which pinned every
        // boat at frame 0 and made a static pose look like a working one. So
        // measure the opposite half of the clip too: mid-cycle the hull MUST
        // have left the bind pose, and by a visible amount.
        if ( !m_skin_moved && clip.frames( ) > 2 && cycle == clip.frames( ) / 2 )
        {
            m_skin_moved = true;
            int moved = 0;
            float worst = 0.0f;
            for ( std::size_t v = 0; v < source.bind.size( ); ++v )
            {
                const mesh_vertex_t& rest = source.bind[ v ];
                const mesh_vertex_t& now = target[ v ];
                const float dx = now.position[ 0 ] - rest.position[ 0 ];
                const float dy = now.position[ 1 ] - rest.position[ 1 ];
                const float dz = now.position[ 2 ] - rest.position[ 2 ];
                const float distance = std::sqrt( dx * dx + dy * dy + dz * dz );
                if ( distance > worst )
                    worst = distance;
                if ( distance > 0.01f )
                    ++moved;
            }
            if ( moved > 0 )
            {
                LOG_SUCCESS( "skin animating: frame %d moves %d of %u vertices, worst %.4f",
                    cycle, moved, static_cast< unsigned >( source.bind.size( ) ), worst );
            }
            else
            {
                LOG_ERROR( "skin STATIC at frame %d: the clip clock is not advancing", cycle );
            }
        }

        // Structural guard on the turn clip. The delta composition above rests
        // on four properties, all measured on Vector: three frames, a sparse
        // body, an identity middle frame, and NO translation on the bones that
        // do carry data. A boat whose clip breaks any of them would be posed
        // wrongly in silence, so check rather than assume it generalises.
        if ( !m_turn_checked && turning != m_turn.end( ) )
        {
            const auto entry = m_clips.find( turning->second );
            if ( entry != m_clips.end( ) && entry->second.valid( )
                && entry->second.bones( ) == static_cast< int >( bind.size( ) ) )
            {
                m_turn_checked = true;
                const c_clip& turn = entry->second;
                int empty = 0;
                int movers = 0;
                int shifted = 0;
                int off_centre = 0;
                std::string named;
                for ( std::size_t b = 0; b < bind.size( ); ++b )
                {
                    const clip_pose_t& left = turn.at( 0, static_cast< int >( b ) );
                    const clip_pose_t& centre = turn.at( 1, static_cast< int >( b ) );
                    const clip_pose_t& right = turn.at( turn.frames( ) - 1, static_cast< int >( b ) );

                    // Vector's clip has none of these - every quaternion is
                    // unit length - so a non-zero count here means another
                    // boat's clip is shaped differently and the composition
                    // needs looking at again.
                    float unit = 0.0f;
                    for ( int c = 0; c < 4; ++c )
                        unit += left.rotation[ c ] * left.rotation[ c ];
                    if ( std::sqrt( unit ) < 0.5f )
                    {
                        ++empty;
                        continue;
                    }

                    for ( int f = 0; f < turn.frames( ); ++f )
                    {
                        const clip_pose_t& pose = turn.at( f, static_cast< int >( b ) );
                        if ( std::fabs( pose.position[ 0 ] ) + std::fabs( pose.position[ 1 ] )
                            + std::fabs( pose.position[ 2 ] ) > 0.0001f )
                            ++shifted;
                    }
                    if ( std::fabs( std::fabs( centre.rotation[ 3 ] ) - 1.0f ) > 0.001f )
                        ++off_centre;

                    float dot = 0.0f;
                    for ( int c = 0; c < 4; ++c )
                        dot += left.rotation[ c ] * right.rotation[ c ];
                    if ( std::fabs( std::fabs( dot ) - 1.0f ) < 0.001f )
                        continue;
                    ++movers;
                    if ( movers <= 8 )
                    {
                        named += " ";
                        named += bind[ b ].name;
                    }
                }
                LOG_INFO( "turn %s: %d frames, %d of %u bones all-zero, %d swivel:%s",
                    turning->second.c_str( ), turn.frames( ),
                    empty, static_cast< unsigned >( bind.size( ) ), movers, named.c_str( ) );
                if ( shifted > 0 )
                {
                    LOG_ERROR( "turn %s: %d carrier frames carry a translation, so it is a pose and not a delta",
                        turning->second.c_str( ), shifted );
                }
                if ( off_centre > 0 )
                {
                    LOG_ERROR( "turn %s: %d carrier bones are not neutral on the middle frame",
                        turning->second.c_str( ), off_centre );
                }
            }
        }
    }

    // Load a boat's animation clips and check each against the hull it is
    // meant to drive. The bone counts come from two unrelated places - the
    // model's bone block and the clip header - so a mismatch means one of
    // them is being read wrong, and is worth failing loudly over.
    void load_clips( const std::string& hull, const value_t* record )
    {
        const value_t* animation = record != nullptr ? find( *record, "Animation" ) : nullptr;
        if ( animation == nullptr || animation->kind != value_t::kind_t::object )
            return;

        const auto skeleton = m_skeletons.find( hull );
        const int expected = skeleton != m_skeletons.end( )
            ? static_cast< int >( skeleton->second.size( ) ) : -1;

        for ( const auto& pair : animation->object )
        {
            if ( pair.second.kind != value_t::kind_t::text || pair.second.text.empty( ) )
                continue;
            const std::string path = "VuAnimationAsset/" + pair.second.text;
            if ( m_clips.find( path ) != m_clips.end( ) )
                continue;

            std::vector< std::uint8_t > bytes;
            c_clip clip{};
            if ( !m_archive.read( path.c_str( ), bytes ) || !clip.load( bytes ) )
            {
                LOG_WARNING( "clip %s failed to parse", path.c_str( ) );
                continue;
            }

            const bool agrees = expected < 0 || clip.bones( ) == expected;
            if ( agrees )
            {
                LOG_SUCCESS( "clip key '%s' -> %-40s bones %-4d frames %-4d %s",
                    pair.first.c_str( ), pair.second.text.c_str( ),
                    clip.bones( ), clip.frames( ),
                    clip.blend_set( ) ? "(blend set)" : "" );
            }
            else
            {
                LOG_WARNING( "clip %s bones %d but hull %s has %d",
                    pair.second.text.c_str( ), clip.bones( ), hull.c_str( ), expected );
            }
            // The record names clips by role, not by filename: TransformAsset
            // is the body animation (Xform), TurnAsset the steering blend set
            // (TurnLR), ThrustAsset the throttle clip. Key off the role.
            if ( pair.first == "TransformAsset" )
                m_xform.emplace( hull, path );
            if ( pair.first == "TurnAsset" )
                m_turn.emplace( hull, path );
            m_clips.emplace( path, std::move( clip ) );
        }
    }

    // Attach a hull model's authored shading constants to a placement made
    // directly by place(). The skinned path merges every part into one draw,
    // so the part carrying the reflective material is the one that counts -
    // on the boats that is the single Boat_proxy/Proxy_diffuse part.
    void shade_hull( int handle, const std::string& asset )
    {
        if ( handle < 0 || c_engine::option( "no-reflect" ) != nullptr )
            return;
        const cached_part_t* chosen = nullptr;
        for ( const cached_part_t& part : model_for( asset ) )
        {
            if ( part.environment_amount > 0.0f )
            {
                chosen = &part;
                break;
            }
            if ( chosen == nullptr && part.specular_amount > 0.0f )
                chosen = &part;
        }
        if ( chosen == nullptr )
            return;
        const int cube = chosen->environment_amount > 0.0f ? cube_for( chosen->cube ) : -1;
        static std::string reported;
        if ( reported != asset )
        {
            reported = asset;
            LOG_VERBOSE( "hull shading %s: cube '%s' -> id %d  env %.2f  spec %.2f %.2f %.2f x%.2f",
                asset.c_str( ), chosen->cube.c_str( ), cube, chosen->environment_amount,
                chosen->specular[ 0 ], chosen->specular[ 1 ], chosen->specular[ 2 ],
                chosen->specular_amount );
        }
        c_engine::shade( handle, cube, chosen->specular,
            chosen->specular_amount, chosen->environment_amount, chosen->diffuse );
    }

    // VuDockedBoatEntity stores the pose. The hull, lods, and clips live on
    // the boat record in VuDBAsset/BoatDB. The menu boat is Vector.
    const std::string& docked_hull( )
    {
        if ( m_hull_ready )
            return m_hull;
        m_hull_ready = true;
        std::vector< std::uint8_t > bytes;
        if ( !m_archive.read( "VuDBAsset/BoatDB", bytes ) )
            return m_hull;
        const value_t root = reader_t( bytes ).parse( );
        const value_t* db = find( root, "VuDBAsset" );
        const value_t* record = db != nullptr ? find( *db, "Vector" ) : nullptr;
        const value_t* model = record != nullptr ? find( *record, "Model Asset" ) : nullptr;
        const value_t* lod2 = record != nullptr ? find( *record, "LOD 2 Model Asset" ) : nullptr;
        if ( model != nullptr && model->kind == value_t::kind_t::text && !model_for( model->text ).empty( ) )
            m_hull = model->text;
        else if ( lod2 != nullptr && lod2->kind == value_t::kind_t::text )
            m_hull = lod2->text;
        return m_hull;
    }

