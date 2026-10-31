// NOT COMPILED. Not in hydro.vcxproj, not in the unity build.
//
// VuBoatCamera: VuBoatCamera_ctor @0x409750, VuBoatCamera_update, and the DB
// reader VuBoatDB_readCameraSection @0x409af0. Verified 1:1, including the
// doubled vertical lag that retail actually ships (0x40a3d9 / 0x40a3fd).
//
// It needs the boat's world matrix, the rigid body's linear velocity and the
// engine's Max Forward Speed - see the parameter comment on chase(). Banked
// until VuBoatEntity exists to supply them. boat_camera_t travels with it.

    // VuBoatCamera_update @0x409d00, transliterated from the disassembly.
    // Nothing here is inferred: the field offsets, the two snap branches,
    // the ramp test, the yaw wrap and every constant were read off the
    // instruction stream.
    //
    // NOT recreated: the alternate target branch taken when the boat's byte
    // at +1131 is set and its node list at +568 is non-empty. It reads a
    // node transform (boat+568 -> +156 -> fields 108..110) that this engine
    // does not have; the default branch below is the one the chase camera
    // takes. The three other camera modes VuBoatCamera_tick @0x409900 can
    // select - cockpit (VuBoatCamera_readCamOffset), nose/reverse
    // (sub_40AC10) and the AI/menu variant (@0x40a4d0) - are also absent.
    // The three things retail reads off the boat are parameters here, because
    // VuBoatEntity is not transliterated yet and nothing may be substituted
    // for it: `world` is the 64 bytes at *(boat+80)+80 (column 0 right,
    // 1 forward, 2 up), `velocity` the rigid body's linear velocity at
    // *(boat+540)+320, and `top_speed` the engine's authored Max Forward
    // Speed. Everything below is VuBoatCamera_update unchanged.
    void chase( float dt, const float* world, const float* velocity, float top_speed )
    {
        if ( world == nullptr || velocity == nullptr )
            return;

        boat_camera_t& cam = m_boat_cam;

        // target = M * Target Local Offset, then lifted in world space.
        float target[ 3 ];
        for ( int axis = 0; axis < 3; ++axis )
        {
            target[ axis ] = cam.target_local[ 0 ] * world[ axis ]
                           + cam.target_local[ 1 ] * world[ 4 + axis ]
                           + cam.target_local[ 2 ] * world[ 8 + axis ]
                           + world[ 12 + axis ];
        }
        target[ 2 ] += cam.target_world_z;

        // The ramp hands the lag over from "opposite the velocity" to
        // "straight out behind the camera". It is cut dead while the boat
        // reverses faster than 10 mph (-4.4703999 m/s) along its forward
        // axis, or has rolled far enough that its up axis drops below 0.5.
        // The dot uses last frame's smoothed velocity, before the lerp.
        const float reversing = world[ 4 ] * cam.velocity[ 0 ]
                              + world[ 5 ] * cam.velocity[ 1 ]
                              + world[ 6 ] * cam.velocity[ 2 ];
        if ( reversing < -4.4703999f || world[ 10 ] < 0.5f )
            cam.ramp = 0.0f;
        else
            cam.ramp = std::fmin( ( 1.0f - cam.ramp ) * dt + cam.ramp, 1.0f );

        const float ideal_yaw = std::atan2( world[ 1 ], world[ 0 ] ) - 1.5707964f;
        if ( cam.snap )
        {
            cam.yaw = ideal_yaw;
        }
        else
        {
            // The yaw being eased away from is re-derived from the previous
            // eye and the new target rather than read back from the stored
            // angle - the eye is the state that persists, not the yaw.
            const float current = std::atan2( cam.eye[ 1 ] - target[ 1 ], cam.eye[ 0 ] - target[ 0 ] );
            const float turns = ( ideal_yaw - current + 3.1415927f ) * 0.15915494f;
            // Retail rounds with fistp, which honours the current rounding
            // mode, so this is a round-to-nearest and not a truncation.
            const float wrapped = ( turns - std::nearbyint( turns - 0.5f ) ) * 6.2831855f - 3.1415927f;
            cam.yaw = wrapped * cam.ramp + current;
        }

        const float pitch = cam.ideal_pitch * 0.017453292f + 1.5707964f;
        const float sin_pitch = std::sin( pitch );
        // The unit vector from the target back out to the eye.
        const float unit[ 3 ] = {
            sin_pitch * std::cos( cam.yaw ),
            sin_pitch * std::sin( cam.yaw ),
            std::cos( pitch )
        };
        for ( int axis = 0; axis < 3; ++axis )
            cam.eye[ axis ] = target[ axis ] + unit[ axis ] * cam.ideal_distance;

        if ( cam.snap )
        {
            std::memcpy( cam.velocity, velocity, sizeof( cam.velocity ) );
        }
        else
        {
            const float blend = std::fmin( dt * 4.0f, 1.0f );
            for ( int axis = 0; axis < 3; ++axis )
                cam.velocity[ axis ] = ( 1.0f - blend ) * cam.velocity[ axis ] + velocity[ axis ] * blend;
        }

        // Top speed, in mph. boat+556 is the boat's engine component: p[12]
        // is its authored "Max Forward Speed" (sub_4140C0, the Engine
        // section reader) and p[4] a per-racer scale that sub_4896A0 writes
        // from VuDBAsset/Race "TopSpeed" as a percentage - a rubber band
        // across the grid, which is unimplemented here and so stays at the
        // authored default of 100%. The third term, boat+1136, is a speed
        // modifier whose only write anywhere in the image is a literal 1.0f
        // at 0x41bec9.
        //
        // Negative, so the lag runs against the velocity: 0.44703999 is
        // mph -> m/s, and the authored speeds are mph.
        const float scale = cam.lag_distance / ( top_speed * -0.44703999f );
        const float speed = std::sqrt( cam.velocity[ 0 ] * cam.velocity[ 0 ]
                                     + cam.velocity[ 1 ] * cam.velocity[ 1 ]
                                     + cam.velocity[ 2 ] * cam.velocity[ 2 ] );
        const float magnitude = speed * cam.lag_distance / ( top_speed * 0.44703999f );

        m_eye[ 0 ] = cam.eye[ 0 ] + ( 1.0f - cam.ramp ) * ( cam.velocity[ 0 ] * scale )
                   + cam.ramp * ( magnitude * unit[ 0 ] );
        m_eye[ 1 ] = cam.eye[ 1 ] + ( 1.0f - cam.ramp ) * ( cam.velocity[ 1 ] * scale )
                   + cam.ramp * ( magnitude * unit[ 1 ] );
        // Retail lerps the vertical lag between two identical expressions,
        // so the ramp cancels out, and then adds the result to the eye a
        // second time (0x40a3d9 stores eye.z + lag, 0x40a3fd adds lag to it
        // again). The doubling looks like a slip in the original, but it is
        // what ships, so it is reproduced rather than tidied away.
        const float lag_z = ( 1.0f - cam.ramp ) * ( cam.velocity[ 2 ] * scale )
                          + cam.ramp * ( cam.velocity[ 2 ] * scale );
        m_eye[ 2 ] = cam.eye[ 2 ] + lag_z + lag_z;

        std::memcpy( m_target, target, sizeof( m_target ) );
        // The orientation is VuMatrix_lookAtBasis(target - eye, up=(0,0,1))
        // composed with a rotation about Y by a roll that is always zero -
        // an identity - so aiming the viewport, whose up axis is also
        // (0,0,1), reproduces it.
        //
        // The far plane is not set by VuBoatCamera_update; retail takes it
        // from the gfx settings zone's "Camera Far Plane", which is still
        // unimplemented, so the existing placeholder stands.
        m_far = 1400.0f;
        cam.snap = false;
    }

    // Read the boat's authored "Camera" section. Absent keys keep the
    // constructor's default, exactly as the retail reader does.
    void read_boat_camera( const value_t* record )
    {
        m_boat_cam = boat_camera_t{};
        const value_t* camera = record != nullptr ? find( *record, "Camera" ) : nullptr;
        if ( camera == nullptr )
            return;
        const value_t* local = find( *camera, "Target Local Offset" );
        if ( local != nullptr )
        {
            m_boat_cam.target_local[ 0 ] = number( find( *local, "X" ), m_boat_cam.target_local[ 0 ] );
            m_boat_cam.target_local[ 1 ] = number( find( *local, "Y" ), m_boat_cam.target_local[ 1 ] );
            m_boat_cam.target_local[ 2 ] = number( find( *local, "Z" ), m_boat_cam.target_local[ 2 ] );
        }
        m_boat_cam.target_world_z = number( find( *camera, "Target World Z Offset" ), m_boat_cam.target_world_z );
        m_boat_cam.ideal_distance = number( find( *camera, "Ideal Distance" ), m_boat_cam.ideal_distance );
        m_boat_cam.ideal_pitch = number( find( *camera, "Ideal Pitch" ), m_boat_cam.ideal_pitch );
        m_boat_cam.lag_distance = number( find( *camera, "Lag Distance" ), m_boat_cam.lag_distance );
    }


    // VuBoatCamera. The authored fields are read per boat out of the
    // BoatDB "Camera" section by VuBoatDB_readCameraSection @0x409af0;
    // every default below is the literal the constructor stores,
    // VuBoatCamera_ctor @0x409750. The retail byte offsets are kept in the
    // comments so this can be read against the disassembly.
    //
    // Max Roll Angle (616, default 3.0) and Roll Period (620, default 10.0)
    // are read by the DB and stored, but VuBoatCamera_update never loads
    // either, and the roll angle at 716 is only ever written to zero by the
    // constructor - so the retail chase camera does not roll, and neither
    // field is carried here.
    struct boat_camera_t
    {
        float target_local[ 3 ]{ 0.0f, -1.0f, 0.0f }; // 580/584/588
        float target_world_z = 1.5f;                 // 596
        float ideal_distance = 4.5f;                 // 600
        float ideal_pitch = -10.0f;                  // 604, DEGREES
        float lag_distance = 4.0f;                   // 612
        bool snap = true;                            // 708
        float yaw = 0.0f;                            // 712
        float eye[ 3 ]{};                            // 676/680/684
        float velocity[ 3 ]{};                       // 724/728/732
        float ramp = 0.0f;                           // 772
    };
    boat_camera_t m_boat_cam;
