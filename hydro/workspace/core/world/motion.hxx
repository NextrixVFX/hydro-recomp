#pragma once

// c_world's motion - member definitions moved out of world.hxx (one class, split by concern).

// Record a rotator's driven placements, each as a matrix RELATIVE to the
// rotator's base, so the per-frame update is base' * relative.
//
// WHICH placements is decided by the motion entity's "Entity" REF, not by
// the subtree it sits in. VuMotionEntity_ctor @0x61a000 declares that ref
// and VuMotionEntity_onActivate @0x61a4f0 drives exactly what it resolves
// to. Every rotator on Hydroelectric names ONE child node
// ("WarningSiren/SirenLight01"), so adopting the whole subtree span the
// siren's housing as well - the ref is what caught it.
inline void c_world::adopt_rotator( int rotator, const std::vector< collected_t >& collected )
{
    rotator_t& motion = m_rotators[ static_cast< std::size_t >( rotator ) ];
    const std::string target = ref_target( motion.path, "Entity" );
    float inverse[ 16 ]{};
    rigid_inverse( inverse, motion.base );
    int taken = 0;
    for ( const collected_t& item : collected )
    {
        // No ref at all: keep the old subtree behaviour rather than
        // silently driving nothing, and say so below.
        if ( !target.empty( ) && !owned_by( item.owner, target ) )
            continue;
        std::array< float, 16 > relative{};
        math_multiply( relative.data( ), inverse, item.world.data( ) );
        motion.placements.push_back( item.handle );
        motion.relative.push_back( relative );
        ++taken;
    }
    LOG_VERBOSE( "rotator %s drives %d of %u collected placements%s",
        motion.path.c_str( ), taken, static_cast< unsigned >( collected.size( ) ),
        target.empty( ) ? " (NO Entity ref - whole subtree)" : "" );
    if ( taken == 0 && !collected.empty( ) )
        LOG_WARNING( "rotator %s drives NOTHING: its ref target %s owns none of the %u placements collected",
            motion.path.c_str( ), target.c_str( ),
            static_cast< unsigned >( collected.size( ) ) );
}

// Every adopter visit() recorded, in its post-order, takes the placements
// made beneath it - and only those not already taken by an adopter inside
// it, as the subtree collector used to hand them up - then adopt_rotator
// keeps the ones its "Entity" ref owns.
inline void c_world::adopt_rotators( std::vector< collected_t > pool )
{
    for ( const auto& adopter : m_adopters )
    {
        std::vector< collected_t > mine;
        std::vector< collected_t > rest;
        for ( collected_t& item : pool )
            ( owned_by( item.owner, adopter.second ) ? mine : rest ).push_back( std::move( item ) );
        pool = std::move( rest );
        adopt_rotator( adopter.first, mine );
    }
}

// Is this placement's owner the ref target, or something beneath it?
inline bool c_world::owned_by( const std::string& owner, const std::string& target )
{
    if ( owner == target )
        return true;
    return owner.size( ) > target.size( )
        && owner.compare( 0, target.size( ), target ) == 0
        && owner[ target.size( ) ] == '/';
}

// Resolve one of an entity's refs to a full entity path.
//
// A ref's authored value comes in TWO forms, both measured off the asset:
// an ABSOLUTE full path (VuAttachEntity's Parent and Child), and a
// TEMPLATE-RELATIVE one (every VuRotateMotionEntity authors
// "WarningSiren/SirenLight01" while living at ".../WarningSiren1/
// RotateMotion1"). In the second form the first segment is the TEMPLATE's
// name and the rest hangs off the owner's parent; dropping that segment
// resolved to a real entity for 13 of 13 instances, which is why the rule
// is this and not a search.
inline std::string c_world::ref_target( const std::string& owner, const std::string& name ) const
{
    const auto found = m_refs.find( std::make_pair( owner, name ) );
    if ( found == m_refs.end( ) )
        return std::string( );
    const std::string& authored = found->second;
    if ( m_entity_type.count( authored ) != 0 )
        return authored;
    const std::size_t slash = owner.rfind( '/' );
    const std::size_t inner = authored.find( '/' );
    if ( slash == std::string::npos || inner == std::string::npos )
        return authored;
    const std::string relative = owner.substr( 0, slash ) + authored.substr( inner );
    return m_entity_type.count( relative ) != 0 ? relative : authored;
}

// Transliterated from VuRotateMotionEntity_tick @0x5d1090. The angle
// integration, the wrap constants and the Rodrigues matrix are exactly
// the shipped arithmetic; the compose order comes from
// VuMatrix_rotateAxisAngle_pre/_post, which differ ONLY in the argument
// order given to VuMatrix_multiply.
inline void c_world::update_rotators( float dt )
{
    for ( rotator_t& motion : m_rotators )
    {
        if ( motion.placements.empty( ) )
            continue;

        const float x = dt * motion.speed + motion.angle + 3.1415927f;
        const float magnitude = std::fabs( x );
        const float wrapped =
            ( magnitude - static_cast< float >( static_cast< int >( magnitude * 0.15915494f ) ) * 6.2831855f )
            - 3.1415927f;
        motion.angle = x < 0.0f ? -wrapped : wrapped;

        const float c = std::cos( motion.angle );
        const float s = std::sin( motion.angle );
        const float t = 1.0f - c;
        const float ax = motion.axis[ 0 ];
        const float ay = motion.axis[ 1 ];
        const float az = motion.axis[ 2 ];

        float rotation[ 16 ]{};
        rotation[ 0 ] = t * ax * ax + c;
        rotation[ 1 ] = t * ax * ay + s * az;
        rotation[ 2 ] = t * ax * az - s * ay;
        rotation[ 4 ] = t * ax * ay - s * az;
        rotation[ 5 ] = t * ay * ay + c;
        rotation[ 6 ] = t * ay * az + s * ax;
        rotation[ 8 ] = t * ax * az + s * ay;
        rotation[ 9 ] = t * ay * az - s * ax;
        rotation[ 10 ] = t * az * az + c;
        rotation[ 15 ] = 1.0f;

        float posed[ 16 ]{};
        if ( motion.local )
            math_multiply( posed, motion.base, rotation );
        else
            math_multiply( posed, rotation, motion.base );

        for ( std::size_t index = 0; index < motion.placements.size( ); ++index )
        {
            float world[ 16 ]{};
            math_multiply( world, posed, motion.relative[ index ].data( ) );
            c_engine::move( motion.placements[ index ], world );
        }
    }
}

// VuOffsetAttachComponent_update @0x5af900, the whole body:
//
//   childWorld = relative * parentWorld
//   childLinear = parentLinear + cross( relative.translation, parentAngular )
//   childAngular = parentAngular
//
// Only the first line has anywhere to land here: nothing in this tree
// carries a per-entity velocity yet, so the two velocity lines are a named
// gap rather than a guess. The ANIMATED variant @0x61e010 inserts the
// parent's node matrix between the two - every attach on this level
// resolves to the offset component (the census prints "no node"), so that
// path is still unwritten.
inline void c_world::update_attaches( )
{
    for ( const attach_t& join : m_attaches )
    {
        if ( !join.attached )
            continue;
        const std::string parent = ref_target( join.path, "Parent" );
        const std::string child = ref_target( join.path, "Child" );
        const auto seat = m_entity_world.find( parent );
        if ( seat == m_entity_world.end( ) )
            continue;

        // The relative matrix, exactly as VuAttachEntity_doAttach
        // @0x5f5550 builds it: identity, multiplied by
        // VuMatrix_fromEuler("Relative Rotation"), then the three
        // translation slots overwritten with "Relative Position" - which
        // is what math_transform composes, with the rotation in degrees
        // because the property is a VuRotation3dProperty.
        const float scale[ 3 ] = { 1.0f, 1.0f, 1.0f };
        float relative[ 16 ]{};
        math_transform( relative, join.position, join.rotation, scale );
        float placed[ 16 ]{};
        math_multiply( placed, seat->second.data( ), relative );
        if ( m_coronas.owns( child ) )
            m_coronas.place( child, placed );
    }
}

// Inverse of a rigid transform: transpose the basis and negate the
// rotated translation. Exact for the rotation-plus-translation matrices
// the entity walk produces.
inline void c_world::rigid_inverse( float* out, const float* m )
{
    math_identity( out );
    for ( int row = 0; row < 3; ++row )
        for ( int column = 0; column < 3; ++column )
            out[ column * 4 + row ] = m[ row * 4 + column ];
    for ( int row = 0; row < 3; ++row )
        out[ 12 + row ] = -( out[ row ] * m[ 12 ]
            + out[ 4 + row ] * m[ 13 ] + out[ 8 + row ] * m[ 14 ] );
}
